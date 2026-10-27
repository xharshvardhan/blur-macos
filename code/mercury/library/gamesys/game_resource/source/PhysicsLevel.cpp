#include "GameResourcePch.h"

#include <system/serialise/C_FileBlock.h>
#include <game_resource/GameResource.h>
#include <game_resource/PhysicsLevel.h>
#include <gravity/sdk/PhysicsSDK.h>	// [Blur2PC] Gravity::C_PhysicsSDK


namespace GameResource
{

enum E_PhysicsFileVersion
{
	E_PhysicsFileVersion_Initial,

	E_PhysicsFileVersion_Current = E_PhysicsFileVersion_Initial
};


// C_PhysicsLevel
C_PhysicsLevel::C_PhysicsLevel()
	: m_pMainImportScene( NULL )
{
}

C_PhysicsLevel::~C_PhysicsLevel()
{
	const S32 routeCount = GetRouteCount();
	for(S32 routeNo=0; routeNo<routeCount; routeNo++)
	{
		if( m_pRouteImportScenes[routeNo] )
		{
			Gravity::C_PhysicsSDK::PhysicsSDK()->ReleaseImportScene(m_pRouteImportScenes[routeNo]);
		}
	}

	Gravity::C_PhysicsSDK::PhysicsSDK()->ReleaseImportScene(m_pMainImportScene);
	m_pMainImportScene = NULL;
}

#if defined(PLATFORM_MACOS)
namespace {
// [Blur2PC] Validate the original block header before C_FileBlock owns it: its
// destructor seeks to the declared end, which must remain inside its parent.
bool NativePhysicsBlockHeader(Sys::I_File* file, Sys::C_BlockHeader& header)
{
    static_assert(sizeof(Sys::C_BlockHeader) == 16, "Original PC block header width changed");
    const auto position = file->GetPosition();
    const auto length = file->Length();
    if(position > length || length-position < sizeof(Sys::C_BlockHeader)) return false;
    Sys::BinaryDeserialiser<Ut::NoEndianSwap> ar(*file);
    ar >> header;
    file->SetPosition(position);
    return true;
}
bool NativePhysicsBlockFits(Sys::I_File* file, const Sys::C_BlockHeader& header)
{
    return header.m_Size >= sizeof(Sys::C_BlockHeader) &&
        header.m_Size <= file->Length()-file->GetPosition();
}
}

bool C_PhysicsLevel::Load(Sys::I_File* pFile)
{
    // [Blur2PC] Read failure must leave neither a partial scene nor a changed
    // parser version published to the resource owner. The previous successful
    // value remains valid until all original main/route imports have completed.
    if(!pFile) return false;
    struct VersionScope {
        Sys::I_File* file; U32 version;
        ~VersionScope(){file->SetVersionNumber(version);}
    } version{pFile, pFile->GetVersionNumber()};
    C_PhysicsLevel pending;
    if(!pending.NativeLoadContents(pFile)) return false;
    Gravity::C_ImportScene* previous = m_pMainImportScene;
    m_pMainImportScene = pending.m_pMainImportScene;
    pending.m_pMainImportScene = previous;
    Btl::Swap(m_pRouteImportScenes, pending.m_pRouteImportScenes);
    return true;
}

bool C_PhysicsLevel::NativeLoadContents(Sys::I_File* pFile)
{
    Sys::C_BlockHeader rootHeader;
    if(!NativePhysicsBlockHeader(pFile, rootHeader)) return false;
    if(rootHeader.m_Id != "Physic")
    {
        // Preserve the original legacy raw Gravity importer fallback.
        pFile->SetPosition(0);
        m_pMainImportScene = Gravity::C_PhysicsSDK::PhysicsSDK()->CreateImportScene();
        return m_pMainImportScene->Import(pFile);
    }
    if(!NativePhysicsBlockFits(pFile, rootHeader)) return false;
    Sys::C_FileBlock rootBlock(pFile);
    bool haveHeader = false, haveRoutes = false;
    S32 routeCount = 0, importedRoutes = 0;
    while(rootBlock.GetPosition() < rootBlock.Length())
    {
        Sys::C_BlockHeader next;
        if(!NativePhysicsBlockHeader(&rootBlock, next) || !NativePhysicsBlockFits(&rootBlock, next)) return false;
        Sys::C_FileBlock block(&rootBlock);
        if(block.HeaderId() == "Header")
        {
            if(haveHeader || block.Length()-block.GetPosition() < 2*sizeof(U32)) return false;
            haveHeader = true;
            U32 fileVersion;
            Sys::BinaryDeserialiser<Ut::NoEndianSwap> ar(block);
            ar >> fileVersion >> routeCount;
            pFile->SetVersionNumber(fileVersion);
            // Every declared route needs at least one original block header.
            if(routeCount < 0 || U64(routeCount) > rootBlock.Length()/sizeof(Sys::C_BlockHeader)) return false;
            m_pRouteImportScenes.Resize(routeCount);
            for(S32 i = 0; i < routeCount; ++i) m_pRouteImportScenes[i] = NULL;
        }
        else if(block.HeaderId() == "Main")
        {
            if(m_pMainImportScene) return false;
            m_pMainImportScene = Gravity::C_PhysicsSDK::PhysicsSDK()->CreateImportScene();
            if(!m_pMainImportScene->Import(&block)) return false;
        }
        else if(block.HeaderId() == "Routes")
        {
            if(haveRoutes) return false;
            haveRoutes = true;
            while(block.GetPosition() < block.Length())
            {
                Sys::C_BlockHeader routeHeader;
                if(!NativePhysicsBlockHeader(&block, routeHeader) || !NativePhysicsBlockFits(&block, routeHeader)) return false;
                Sys::C_FileBlock route(&block);
                if(route.HeaderId() == "Route")
                {
                    if(importedRoutes >= routeCount) return false;
                    auto*& scene = m_pRouteImportScenes[importedRoutes];
                    scene = Gravity::C_PhysicsSDK::PhysicsSDK()->CreateImportScene();
                    if(!scene->Import(&route)) return false;
                    ++importedRoutes;
                }
                else LOGWARN(g_Log, "Unknown Block Header '%s'", route.HeaderId().Chars());
            }
        }
        else LOGWARN(g_Log, "Unknown Block Header '%s'", block.HeaderId().Chars());
    }
    // The original level consumer requires Main; Header declares route ownership.
    return m_pMainImportScene && importedRoutes == routeCount;
}
#else
bool C_PhysicsLevel::Load( Sys::I_File* pFile )
{
	const U32 prevFileVersion = pFile->GetVersionNumber();
	U32 fileVersion = E_PhysicsFileVersion_Initial;

	Sys::C_FileBlock rootBlock( pFile );
	if( rootBlock.HeaderId() != "Physic" )
	{
		LOGWARN( g_Log, "Expected a 'Physic' Block" );

		{
			pFile->SetPosition( 0 );
			m_pMainImportScene = Gravity::C_PhysicsSDK::PhysicsSDK()->CreateImportScene();
			return m_pMainImportScene->Import( pFile );
		}

		return false;
	}

	S32 routeCount = 0;
	while ( rootBlock.GetPosition() < rootBlock.Length() )
	{
		Sys::C_FileBlock block( &rootBlock );

		if( block.HeaderId() == "Header" )
		{
			Sys::BinaryDeserialiser<Ut::NoEndianSwap> ar( block );
			
			ar >> fileVersion;
			pFile->SetVersionNumber( fileVersion );

			ar >> routeCount;
			m_pRouteImportScenes.Resize( routeCount );
		}
		else			
		if( block.HeaderId() == "Main" )
		{
			assert( m_pMainImportScene == NULL );
			m_pMainImportScene = Gravity::C_PhysicsSDK::PhysicsSDK()->CreateImportScene();
			m_pMainImportScene->Import( &block );
		}
		else			
		if( block.HeaderId() == "Routes" )
		{
			S32 routeNo = 0;
			while ( block.GetPosition() < block.Length() )
			{
				Sys::C_FileBlock routeBlock( &block );
				if( routeBlock.HeaderId() == "Route" )
				{
					m_pRouteImportScenes[routeNo] = Gravity::C_PhysicsSDK::PhysicsSDK()->CreateImportScene(); 
					m_pRouteImportScenes[routeNo]->Import( &routeBlock );
					routeNo++;
				}
				else
				{
					LOGWARN( g_Log, "Unknown Block Header '%s'", routeBlock.HeaderId().Chars() );
				}
			}
			assert( routeNo == routeCount );
		}
		else
		{
			LOGWARN( g_Log, "Unknown Block Header '%s'", block.HeaderId().Chars() );
		}
	}

	pFile->SetVersionNumber( prevFileVersion );

	return true;
}

#endif // PLATFORM_MACOS

Gravity::C_ImportScene* C_PhysicsLevel::GetMainImportScene() const
{
	return m_pMainImportScene;
}

S32 C_PhysicsLevel::GetRouteCount() const
{
	return m_pRouteImportScenes.Size();
}

Gravity::C_ImportScene* C_PhysicsLevel::GetRouteImportScene( const S32 routeNo ) const
{
	return m_pRouteImportScenes[routeNo];
}

} // namespace GameResource
