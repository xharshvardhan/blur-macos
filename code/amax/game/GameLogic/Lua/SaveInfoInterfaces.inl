#include <AmaxText.h> // [Blur2PC] Native UTF-16 literal ABI.
#include <GameApp.h>

#include <GameLogic/Profile/GameProfileManager.h>

#include "AppPhotoCapture.h"
#include <jpeghelper/JpegDecoder.h> 
#include <jpeghelper/JpegEncoder.h>

#include <ui/Game/UILocaliser.h>

#include <Game/Game.h>
#include <GameApp.h>
#include <GameLogic/Structures/LevelState.h>
#include <ui/UiHelpers.h>

#include <GameLogic\ContentServer\ContentServerManager.h>

#include <GameLogic\Photomode\PhotomodeManager.h>

#include <compression.h>
#if defined(PLATFORM_MACOS) // [Blur2PC] Retain exact PC framing and bounded native stream ownership.
#include <memory>
#include <array>
#include <climits>
#endif

#include <Game\GameUtil\ScopedSuspendResume.h>
#include "Structures\VehiclePickupInfo.h"
#include "Game\GameActors\VehicleLogic\VehicleLogicActor.h"

namespace UI
{

#if defined(PLATFORM_MACOS)
namespace NativeProfileEnvelope {
// [Blur2PC] PC Compress uses Z_FULL_FLUSH, not Z_FINISH. Validate all encoded bytes
// and exact output extent without demanding Z_STREAM_END or trusting an advertised allocation.
static U32 Little32(const U8* bytes) {
    return U32(bytes[0]) | (U32(bytes[1])<<8) | (U32(bytes[2])<<16) | (U32(bytes[3])<<24);
}
static bool Inflate(U8* encoded,U32 encodedSize,U32 originalSize,U8* decoded) {
    z_stream stream{};
    if(inflateInit(&stream)!=Z_OK)return false;
    struct End {z_stream* stream;~End(){inflateEnd(stream);}} end{&stream};
    stream.next_in=encoded;stream.avail_in=encodedSize;
    std::array<U8,32768> scratch{};
    U32 written=0;
    for(;;) {
        stream.next_out=scratch.data();stream.avail_out=static_cast<uInt>(scratch.size());
        const auto before=stream.total_in;
        const int status=inflate(&stream,Z_SYNC_FLUSH);
        if(status!=Z_OK && status!=Z_STREAM_END && status!=Z_BUF_ERROR)return false;
        const U32 produced=static_cast<U32>(scratch.size())-stream.avail_out;
        if(produced>originalSize-written)return false;
        if(decoded && produced)std::memcpy(decoded+written,scratch.data(),produced);
        written+=produced;
        if(status==Z_STREAM_END)return stream.avail_in==0 && written==originalSize;
        // [Blur2PC] Full scratch may leave a pending backreference after the final input byte.
        // Drain it; the original full-flush writer terminates at an empty stored-block marker.
        if(stream.avail_in==0 && stream.avail_out!=0) {
            const bool fullFlush=encodedSize>=4 && encoded[encodedSize-4]==0 &&
                encoded[encodedSize-3]==0 && encoded[encodedSize-2]==0xff && encoded[encodedSize-1]==0xff;
            return fullFlush && written==originalSize;
        }
        if(stream.total_in==before && produced==0)return false;
    }
}
static bool Prefix(const U8* bytes,U32 size) {
    // [Blur2PC] Original stream begins with completed/writing U8 plus the existing version S32.
    return bytes && size>=sizeof(U8)+sizeof(S32) && bytes[0]!=1 &&
        IsLoadableSaveGameVersion(static_cast<S32>(Little32(bytes+1)));
}
}
#endif

// --------------------------
// C_SaveInfoInterfaceProfile
// --------------------------
class C_SaveInfoInterfaceProfile : public C_SaveInfoInterface
{
public:
	C_SaveInfoInterfaceProfile()
		: C_SaveInfoInterface()
#if defined(PLATFORM_MACOS) // [Blur2PC] Loading/failed preparation never owns an uninitialized stream.
        ,m_pStream(NULL)
#endif
	{
	#if PLATFORM_PC || defined(PLATFORM_MACOS) // [Blur2PC] Native profiles use the original PC file format.
		m_EnumerationFileExtension = "save";
	#endif
	#ifdef PLATFORM_PS3
		m_TitleId = PlatformPS3::g_ps3_titleId;
		m_FilePath = "MemSavePS3";
		m_uSizeIcon0 = 68327;
		m_uSizeIcon1 = 0;
		m_uSizePic1 = 546985;
		m_uSizeSnd0 = 0;

		SetParamSFO_Title( UI::C_TextStringID( "PRO_PS3_SAVE_TITLE" ).String() );
		SetParamSFO_Detail( UI::C_TextStringID( "PRO_PS3_SAVE_DETAIL" ).String() );
		SetSubtitle( UI::C_TextStringID( "PRO_PS3_SAVE_SUBTITLE" ).String() );

		m_ListName = "TEST";								// debug - not used here
		SetListNewDataTitle( AMAX_WIDE_LITERAL("SHOULD NEVER SEE THIS!") );	// debug - not used here

		SetSecureFieldId( "BEAUTIFUL_CHAOS!" );
	#endif
	#if defined(PLATFORM_PC) || defined(PLATFORM_MACOS) // [Blur2PC] Native profiles use the original PC file format.
		m_SaveRoot = "SAVE";
	#endif
	}

#if defined(PLATFORM_MACOS)
    // [Blur2PC] The lazy profile owner also owns abandoned/failed prepared streams.
    ~C_SaveInfoInterfaceProfile(){SAFE_DELETE_PTR(m_pStream);}
#endif

public:
	void	Setup( const bool bLoading )
			{
#if defined(PLATFORM_MACOS)
                // [Blur2PC] Publish a prepared record only after every serialization/compression acquisition succeeds.
                C_ScopedDeviceSuspend scopedDeviceSuspend;
                m_FileInfo.Clear();SAFE_DELETE_PTR(m_pStream);
                if(bLoading){m_FileInfo.PushBack(C_SaveFileInfo(0,"BLURSAVE"));return;}
                std::unique_ptr<Sys::C_MemoryStream> original(new Sys::C_MemoryStream(4*1024));
                Sys::BinarySerialiserAutoEndian serial(*original);
                if(!Game::SaveGameImpl(serial))return;
                const U64 originalLength=original->GetBufferSize();
                if(originalLength<sizeof(U8)+sizeof(S32) || originalLength>INT_MAX)return;
                original->GetBufferPtr()[0]=0;
                U8* compressed=NULL;
                struct CompressedOwner {U8*& bytes;~CompressedOwner(){delete[] bytes;}} compressedOwner{compressed};
                U32 compressedSize=0;
                CCompression compressor(false);
                if(!compressor.Compress(original->GetBufferPtr(),static_cast<U32>(originalLength),&compressed,&compressedSize) ||
                    !compressed || !compressedSize || compressedSize>static_cast<U32>(INT_MAX)-3*sizeof(U32) ||
                    !NativeProfileEnvelope::Inflate(compressed,compressedSize,static_cast<U32>(originalLength),NULL))return;
                std::unique_ptr<Sys::C_MemoryStream> prepared(new Sys::C_MemoryStream(4*1024));
                Sys::BinarySerialiserAutoEndian packed(*prepared);
                packed<<U32(0xABCDE337)<<static_cast<U32>(originalLength)<<compressedSize;
                if(!packed.Write(compressed,compressedSize))return;
                m_FileInfo.PushBack(C_SaveFileInfo(static_cast<U32>(prepared->GetBufferSize()),"BLURSAVE"));
                m_pStream=prepared.release();
#else

				C_ScopedDeviceSuspend scopedDeviceSuspend;

				m_FileInfo.Clear();

				U32 uFileSize = 0;

				if( !bLoading )
				{
					m_pStream = new Sys::C_MemoryStream( 4 * 1024 );
					if( !m_pStream )
					{
						LOGERROR( g_Log, DBSTR "Failed to allocate");
						return;
					}

					Sys::BinarySerialiserAutoEndian serial( *m_pStream );
					
					// Implement SteveB's debug save for now
					bool result = Game::SaveGameImpl( serial );
					if( !result )
					{
						LOGERROR( g_Log, DBSTR "Failed to save");
						return;
					}
					

					CCompression compress(false);
					U8* pCompressedPtr;
					U32 uCompressedSize;
					U32 uMagicNumber = 0xABCDE337;
					U32 uOriginalSize = m_pStream->GetBufferSize();
					U8* pSourceData = m_pStream->GetBufferPtr();

					// We finished writing okay.
					pSourceData[0] = 0;

					U32 pSourceDataSize = m_pStream->GetBufferSize();
					compress.Compress(pSourceData, pSourceDataSize, &pCompressedPtr, &uCompressedSize );
					
					delete m_pStream;
									
					m_pStream = new Sys::C_MemoryStream( 4 * 1024 );
					Sys::BinarySerialiserAutoEndian serialcompressed( *m_pStream );
					serialcompressed << uMagicNumber;
					serialcompressed << uOriginalSize;
					serialcompressed << uCompressedSize;
					for (U32 x = 0 ; x < uCompressedSize ; x++) 
					{
						char c = pCompressedPtr[x];
						serialcompressed << c;
					}
					
					#if defined(PLATFORM_MACOS) // [Blur2PC] CCompression allocates its output with new U8[].
					delete[] pCompressedPtr;
					#else
					delete pCompressedPtr;
					#endif

					// PR - this is hardcoded so we can do a PS3 TRC
//					uFileSize = ( 5 * 1024 * 1024 );

					uFileSize = (U32)m_pStream->GetBufferSize();
				}

				m_FileInfo.PushBack( 
					C_SaveFileInfo(
						  uFileSize												// m_uFileSize
					#if defined(PLATFORM_PC) || defined(PLATFORM_MACOS) // [Blur2PC] Native profiles use the original PC file format.
						, "BLURSAVE"											// m_FileName ([Blur2PC] T250: retail 0x00c20180)
					#else
						, "AMAXSAVE"											// m_FileName
					#endif
					)
				);
#endif
			}

	bool	Load( const S32 sPadNo, U8* pBuffer, const U32 uDataSize, const S32 sUserData )
			{
#if defined(PLATFORM_MACOS)
                // [Blur2PC] Bound the exact original PC envelope before any deserializer mutates game owners.
                if(!pBuffer || uDataSize<sizeof(U8)+sizeof(S32) || uDataSize>static_cast<U32>(INT_MAX))return false;
                if(NativeProfileEnvelope::Little32(pBuffer)!=0xABCDE337) {
                    if(!NativeProfileEnvelope::Prefix(pBuffer,uDataSize))return false;
                    Sys::C_MemoryStream stream(pBuffer,uDataSize);
                    Sys::BinaryDeserialiserAutoEndian serial(stream);
                    return Game::LoadGameImpl(serial);
                }
                if(uDataSize<3*sizeof(U32))return false;
                const U32 originalSize=NativeProfileEnvelope::Little32(pBuffer+4);
                const U32 compressedSize=NativeProfileEnvelope::Little32(pBuffer+8);
                if(originalSize<sizeof(U8)+sizeof(S32) || originalSize>static_cast<U32>(INT_MAX) ||
                    !compressedSize || compressedSize!=uDataSize-3*sizeof(U32))return false;
                U8* encoded=pBuffer+3*sizeof(U32);
                if(!NativeProfileEnvelope::Inflate(encoded,compressedSize,originalSize,NULL))return false;
                std::unique_ptr<U8[]> decoded(new U8[originalSize]);
                if(!NativeProfileEnvelope::Inflate(encoded,compressedSize,originalSize,decoded.get()) ||
                    !NativeProfileEnvelope::Prefix(decoded.get(),originalSize))return false;
                Sys::C_MemoryStream stream(decoded.get(),originalSize);
                Sys::BinaryDeserialiserAutoEndian serial(stream);
                const bool result=Game::LoadGameImpl(serial);
                U32 unread=0;U8 extra;
                while(result && serial.Read(&extra,1))++unread;
                LOGINFO(g_Log,"[Blur2PC] profile save load %s (version %d, %u bytes, %u unread)\n",
                    result?"succeeded":"failed",GetLoadingSaveGameVersion(),originalSize,unread);
                return result;
#else

				// Implement SteveB's debug save for now
				U8* pData = pBuffer;
				U32 uMagicNumber		= 0xABCDE337;
			#if PLATFORM_PC || defined(PLATFORM_MACOS) // [Blur2PC] Native profiles use the original PC file format.
				// [Blur2PC] Save() writes the header through the native (little-endian) serialiser; the
				// big-endian read below never matched on PC, so every save was rejected on load.
				U32 uMagicNumberRead	= pData[3] << 24 | pData[2] << 16 | pData[1] << 8 | pData[0];
				U32 uOriginalSize		= pData[7] << 24 | pData[6] << 16 | pData[5] << 8 | pData[4];
				U32 uCompressedSize		= pData[11] << 24 | pData[10] << 16 | pData[9] << 8 | pData[8];
			#else
				U32 uMagicNumberRead	= pData[0] << 24 | pData[1] << 16 | pData[2] << 8 | pData[3];
				U32 uOriginalSize		= pData[4] << 24 | pData[5] << 16 | pData[6] << 8 | pData[7];
				U32 uCompressedSize		= pData[8] << 24 | pData[9] << 16 | pData[10] << 8 | pData[11];
			#endif
				bool result = false;
				if (uMagicNumberRead == uMagicNumber)
				{
					pData+=sizeof(uOriginalSize)+sizeof(uCompressedSize)+sizeof(uMagicNumberRead);
					CCompression decompress;
					U8* pDecompressed = new U8[ uOriginalSize ];
					decompress.Decompress(pData, uCompressedSize, pDecompressed, uOriginalSize);
					Sys::C_MemoryStream stream( pDecompressed, uOriginalSize );
					Sys::BinaryDeserialiserAutoEndian serial( stream );
					result = Game::LoadGameImpl( serial );
				#if PLATFORM_PC || defined(PLATFORM_MACOS) // [Blur2PC] Native profiles use the original PC file format.
					// [Blur2PC] T250: bytes the load left unread (0 when the format matched)
					U32 uUnread = 0;
					U8 uExtra;
					while ( result && serial.Read( &uExtra, 1 ) )
						uUnread++;
					LOGINFO( g_Log, "[Blur2PC] profile save load %s (version %d, %u bytes, %u unread)\n", result ? "succeeded" : "failed",
						GetLoadingSaveGameVersion(), uOriginalSize, uUnread );
				#endif
					#if defined(PLATFORM_MACOS) // [Blur2PC] Match the native array allocation above.
					delete[] pDecompressed;
					#else
					delete pDecompressed;
					#endif
				}
				else
				{
					// Uncompressed 
					Sys::C_MemoryStream stream( pBuffer, uDataSize );
					Sys::BinaryDeserialiserAutoEndian serial( stream );
					result = Game::LoadGameImpl( serial );
					
				}
				
				return result;
#endif
			}

	bool	Save( const S32 sPadNo, U8* pBuffer, const U32 uDataSize, const S32 sUserData )
			{
#if defined(PLATFORM_MACOS)
                // [Blur2PC] A failed/abandoned Setup cannot masquerade as a writable record.
                if(!pBuffer || !m_pStream || uDataSize!=m_pStream->GetBufferSize())return false;
#endif

				if( !m_pStream )
					return false;

				memcpy( pBuffer, m_pStream->GetBufferPtr(), m_pStream->GetBufferSize() );

				SAFE_DELETE_PTR( m_pStream );
				return true;
			}

	Sys::C_MemoryStream*	m_pStream;
};

// -------------------------
// C_SaveInfoInterfacePhotos
// -------------------------
class C_SaveInfoInterfacePhotos : public C_SaveInfoInterface
{
public:
	C_SaveInfoInterfacePhotos()
		: C_SaveInfoInterface()
	{
		m_eType = E_UiSaveInfoType_Enumerate;
	#if PLATFORM_PC
		m_EnumerationFileExtension = "photo";
	#endif
	#ifdef PLATFORM_PS3
		m_TitleId = PlatformPS3::g_ps3_titleId;
		m_FilePath = "MemSavePS3";
		m_uSizeIcon0 = 68327;
		m_uSizeIcon1 = 0;
		m_uSizePic1 = 546985;
		m_uSizeSnd0 = 0;

		SetParamSFO_Title( UI::C_TextStringID( "PRO_PS3_SAVE_PHOTO_TITLE" ).String() );
		SetParamSFO_Detail( UI::C_TextStringID( "PRO_PS3_SAVE_PHOTO_DETAIL" ).String() );
		SetSubtitle( UI::C_TextStringID( "PRO_PS3_SAVE_PHOTO_SUBTITLE" ).String() );

		m_ListName = "PHOTO";
		SetListNewDataTitle( UI::C_TextStringID( "PRO_PS3_SAVE_PHOTO_NEW_DATA" ).String() );

		SetSecureFieldId( "BEAUTIFUL_CHAOS!" );
	#endif
	#ifdef PLATFORM_PC
		m_SaveRoot = "PHOTO";
	#endif
	}

public:
	void	Setup( const bool bLoading, const S32 sEnumeration )
			{
				m_FileInfo.Clear();

				m_sEnumeration = sEnumeration;

				U32 uFileSize = 0;	/* set from save-system on load */

				if ( !bLoading )
				{
					// Lock buffer and encode to JPEG:
					PhotoMode::C_AppPhotoCapture& photoCapture = PhotoMode::AppPhotoCapture();
					U8* bits = 0;
					S32 pitch = 0, width = 0, height = 0, format = 0;
					photoCapture.LockBitmap(bits, pitch, width, height, format);

					//make a comma delimited comment for the upload
					Game::C_LevelState& levelState = Game::C_Game::GetInstance()->GetLevelState();

					Util::C_String16 str;
					Util::C_String256 comment_str;

					if(Game::C_PhotomodeManager::Instance().InUI())
					{
						comment_str.Printf("DOWNLOADED");
					}
					else
					{
						//CONTENT_SERVER_TAGS_PHOTO_CONTENT_TYPE
						str.Printf("%x,",Game::CONTENT_SERVER_TYPE_PHOTO); comment_str+=str;

						//CONTENT_SERVER_TAGS_PHOTO_LOCATION
						str.Printf("%x,",levelState.mCity); comment_str+=str;

						Game::C_RaceLogicLayer* pLayer = C_GameApp::GetInstance()->GetRaceBookLayer()->GetRaceLogicLayer();
						pLayer->CalculateBiggestOnScreenVehicles();

						Btl::Vector<int> sorted_vehicles;
						for(unsigned int a=0;a<levelState.mNumCars;a++)
						{
							sorted_vehicles.PushBack(a);
						}

						bool sorting=true;
						while(sorting)
						{
							sorting=false;

							for(int a=0;a<sorted_vehicles.Size()-1;a++)
							{
								if(pLayer->mBiggestOnScreenVehicles[sorted_vehicles[a]] < pLayer->mBiggestOnScreenVehicles[sorted_vehicles[a+1]])
								{
									int temp=sorted_vehicles[a];
									sorted_vehicles[a]=sorted_vehicles[a+1];
									sorted_vehicles[a+1]=temp;

									sorting=true;
								}
							}
						}

						for(unsigned int a=0;a<Game::MAX_CONTENT_SERVER_PHOTO_VEHICLE_TAGS;a++)
						{
							U32 carID=-1;
							if(a < levelState.mNumCars && pLayer->mBiggestOnScreenVehicles[sorted_vehicles[a]])
							{
								carID=levelState.mCars[sorted_vehicles[a]].mCarID;
							}

							str.Printf("%x,",carID); comment_str+=str;
						}

						for(int a=0;a<Game::MAX_CONTENT_SERVER_PHOTO_POWERUP_TAGS;a++)
						{
							if(UI::GetVehicleLogicActor(a))
							{
								VehiclePickupInfo info=UI::GetVehicleLogicActor(a)->GetPickupInfo();
								
								if(info.mCurrentlyUsingType == E_PICKUP_TYPE_NONE)
								{
									str.Printf("%x,",info.mCurrentlyUsingType); comment_str+=str;
								}
								else
								{
									str.Printf("%x,",info.mCurrentlyUsingType + (info.mCurrentlyUsingLevel*100)); comment_str+=str;
								}
							}
							else
							{
								str.Printf("%x,",-1); comment_str+=str;
							}
						}

						//race position (for an achievement)
						str.Printf("%x,",UI::GetVehicleLogicActor(0)->GetPositionInfo().mRacePosition); comment_str+=str;
					}

					m_Encoder.SetComments(comment_str.Chars(), comment_str.Length() + 1);


					m_Encoder.EncodeFromBuffer(bits, pitch, width, height, format);


					photoCapture.UnlockBitmap();

					uFileSize = m_Encoder.JpegLength();

					// current time
					Time::C_DecomposedTime time( Time::GetCurrentTime() );

				#if PLATFORM_PC
					m_FileName.Printf( "%S.photo", C_UILocaliser::Instance().ConvertSystemTimeToFileName( time ) );
				#endif
				}

				m_FileInfo.PushBack( 
					C_SaveFileInfo(
						  uFileSize							// m_uFileSize
					#if PLATFORM_PC
						, m_FileName						// m_FileName
					#else
						, "PHOTO"							// m_FileName
					#endif
					)
				);
			}

	bool	Load( const S32 sPadNo, U8* pBuffer, const U32 uDataSize, const S32 sUserData )
			{
				PhotoMode::C_AppPhotoCapture& photoCapture = PhotoMode::AppPhotoCapture();

				m_Decoder.SetData( pBuffer, uDataSize );
				
				U8* bits = 0;
				S32 pitch = 0, width = 0, height = 0, format = 0;
				photoCapture.LockBitmap(bits, pitch, width, height, format);
				m_Decoder.DecodeIntoBuffer(bits, pitch, width, height, format);
				
				Game::C_PhotomodeManager::Instance().SetJpegTagStr((const char *)m_Decoder.Comments());
				
				photoCapture.UnlockBitmap();

				m_Decoder.Exit();

				return true;
			}

	bool	Save( const S32 sPadNo, U8* pBuffer, const U32 uDataSize, const S32 sUserData )
			{
				memcpy( pBuffer, m_Encoder.JpegData(), uDataSize );

				m_Encoder.Exit();

				if ( Game::C_GameProfileManager::Instance().GetProfile( sPadNo ) )
					Game::C_GameProfileManager::Instance().GetProfile( sPadNo )->mPhotosSaved++;

				return true;
			}

public:
	JpegHelper::C_JpegEncoder 	m_Encoder;
	JpegHelper::C_JpegDecoder 	m_Decoder;

#if PLATFORM_PC
	Util::C_FixedString< SAVE_FILENAME_SIZE >		m_FileName;
#endif
};

/*
// ---------------------------
// C_VinylInfoInterfaceProfile
// ---------------------------
class C_VinylInfoInterfaceProfile : public C_SaveInfoInterface
{
public:
	C_VinylInfoInterfaceProfile()
		: C_SaveInfoInterface()
	{
	#if PLATFORM_XENON || PLATFORM_PC
		m_EnumerationFileExtension = "vinyl";
	#endif
	#ifdef PLATFORM_XENON
		m_SaveRoot = "VINYL";
		m_bRoamable = true;
	#endif
	#ifdef PLATFORM_PS3
		m_TitleId = PlatformPS3::g_ps3_titleId;
		m_FilePath = "MemSavePS3";
		m_uSizeIcon0 = 68327;
		m_uSizeIcon1 = 0;
		m_uSizePic1 = 546985;
		m_uSizeSnd0 = 0;
		m_ListName = "VINYL";

		SetParamSFO_Title( AMAX_WIDE_LITERAL("Test") );							// TODO - localise
		SetParamSFO_Detail( AMAX_WIDE_LITERAL("Test Save Data Detail Info") );	// TODO - localise
		SetSubtitle( AMAX_WIDE_LITERAL("Save Game") );							// TODO - localise
		SetListNewDataTitle( AMAX_WIDE_LITERAL("Create New Data") );				// TODO - localise

		SetSecureFieldId( "BEAUTIFUL_CHAOS!" );
	#endif
	#ifdef PLATFORM_PC
		m_SaveRoot = "VINYL";
	#endif
	}

public:
	void			SetupFiles()
					{
						m_FileInfo.PushBack( 
							C_SaveFileInfo(
								  50 * 1024												// m_uFileSize
								, "VINYLSAVE"											// m_FileName
							#ifdef PLATFORM_XENON
								, "SAVE:\\vinyl"										// m_InternalName
								, UI::C_TextStringID( "PRO_SAVE_GAME_NAME" ).String()	// m_DisplayName
							#endif
							)
						);
					}

	bool			Load( const S32 sPadNo, U8* pBuffer, const U32 uDataSize, const S32 sUserData )
					{
						return UI::C_VinylManager::Instance().ReadBuffer( pBuffer, uDataSize );
					}

	bool			Save( const S32 sPadNo, U8* pBuffer, const U32 uDataSize, const S32 sUserData )
					{
						return UI::C_VinylManager::Instance().WriteBuffer( pBuffer, uDataSize );
					}
};
*/

}; // namespace UI
