// [Blur2PC] Native runtime record layout via actual Instance producers and core consumers.
#include "AudioPch.h"
#include "source/SystemHeaders.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <new>
#include <stdexcept>
void XtLibrary_Application(){}
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T> bool aligned(T* pointer){return reinterpret_cast<std::uintptr_t>(pointer)%alignof(T)==0;}
void run(U32 count){
    using namespace Audio;using namespace Audio::Core;
    // Exactly the size helpers used by Object::CreateMasterInstance, with independent guards.
    alignas(128) std::array<U8,2048> storage;storage.fill(0xa5);
    const U32 streamOffset=16,streamBytes=NativeWaveStreamStorageSize(count);
    const U32 adpcmOffset=streamOffset+streamBytes+16,adpcmBytes=NativeAdpcmInfoStorageSize(count);
    require(adpcmOffset+adpcmBytes+16<storage.size(),"Authored scratch storage too small");
    Instance instance;instance.SetCoreData(storage.data());instance.SetWaveServerOffset(streamOffset);instance.SetAdpcmInfoOffset(adpcmOffset);
    *instance.GetNumStreamsNeedingUpdatePointer()=0;
    *reinterpret_cast<U32*>(instance.GetAdpcmWaveInfoDataChunk())=0;
    alignas(128) std::array<U8,512> encoded{};
    U8* bytes=encoded.data()+32;
    for(U32 packet=0;packet<3;++packet)for(U32 channel=0;channel<2;++channel){U8* p=bytes+(packet*2+channel)*33;p[0]=0;std::fill_n(p+1,32,channel?0xff:0x11);}
    alignas(16) std::array<F32,256> previous{};
    for(U32 i=0;i<count;++i){
        auto* produced=instance.GetNextStreamInfoSlot();require(aligned(produced),"Instance stream record violates native pointer alignment");
        new(produced) WaveStreamData();produced->pData=bytes;produced->hWaveData=StreamHandle(1+i);produced->bResourcesAllocated=true;produced->u32BytesAvailable=198;
        ++*instance.GetNumStreamsCreatedPointer();
        auto* consumed=GetWaveData(WaveServerLocalIndex(i),instance.GetWaveServerData());
        require(consumed==produced&&instance.GetWaveServerStreamData(i)==produced,"Instance/core stream accessors disagree");
        require(reinterpret_cast<U8*>(produced)+sizeof(*produced)<=storage.data()+streamOffset+streamBytes,"Producer stream record exceeds actual Object storage size");
        auto* info=instance.GetAdpcmWaveInfo(i);require(aligned(info),"Instance ADPCM record violates native pointer alignment");new(info) AdpcmWaveInfo();info->pPrevDecodedFrameBuffer=previous.data();info->u16BufferIndex=U16(i+10);instance.IncrementNumberOfAdpcmWavesCreated();
        require(GetAdpcmBufferData(i,instance.GetAdpcmWaveInfoDataChunk())==info&&GetAdpcmBufferData(i,instance.GetAdpcmWaveInfoDataChunk())->pPrevDecodedFrameBuffer==previous.data(),"Core ADPCM consumer changed native pointer/index");
        require(reinterpret_cast<U8*>(info)+sizeof(*info)<=storage.data()+adpcmOffset+adpcmBytes,"Producer ADPCM record exceeds actual Object storage size");
    }
    require(!GetWaveData(WaveServerLocalIndex(),instance.GetWaveServerData()),"Invalid wave index became a runtime pointer");
    auto* stream=instance.GetWaveServerStreamData(count-1);
    // Exercise the real streaming FillBuffer path against its resident-data counterpart.
    ADPCM4bitDecoder::WaveData wave{};wave.uChannels=2;wave.uLength=198;wave.uSampleRate=24000;wave.uPCMLoopPosEnd=192;wave.hWaveServerIndex=WaveServerLocalIndex(count-1);
    StereoADPCM4bitDecoder streaming(false,false),resident(false,false);
    streaming.SetWaveData(wave);ProcessInfo process{};process.decoderDataInfo=instance.GetDecoderDataInfo();process.uFrameInt=1;process.fWidth=1.f;
    streaming.Init(process.decoderDataInfo);streaming.SetPitch(1.f);streaming.Reset(process.decoderDataInfo);
    wave.psData=reinterpret_cast<S8*>(bytes);wave.hWaveServerIndex.SetInvalid();resident.SetWaveData(wave);DecoderDataInfo noStream{};resident.Init(noStream);resident.SetPitch(1.f);resident.Reset(noStream);
    std::array<F32,256> actual{},expected{};require(streaming.FillBuffer(actual.data(),process),"Actual primed streaming decoder unexpectedly ended");ProcessInfo residentProcess{};residentProcess.uFrameInt=1;residentProcess.fWidth=1.f;require(resident.FillBuffer(expected.data(),residentProcess),"Actual resident decoder unexpectedly ended");
    bool signal=false;for(U32 i=0;i<actual.size();++i){require(std::isfinite(actual[i])&&actual[i]==expected[i],"Aligned native stream changed actual decoded PCM output");signal|=actual[i]!=0.f;}require(signal,"PCM comparison exercised silence only");
    require(stream->u32BytesConsumed==132&&stream->u32LocalOffset==132&&stream->u32BytesAvailable==66&&instance.GetNumStreamsNeedingUpdate()==1,"Real stream consumer changed byte/counter accounting");streaming.Shutdown(process.decoderDataInfo);resident.Shutdown(noStream);
    for(U32 i=0;i<count;++i){instance.GetWaveServerStreamData(i)->~WaveStreamData();instance.GetAdpcmWaveInfo(i)->~AdpcmWaveInfo();}
    for(U32 i=0;i<16;++i){require(storage[i]==0xa5&&storage[streamOffset+streamBytes+i]==0xa5&&storage[adpcmOffset+adpcmBytes+i]==0xa5,"Runtime producer/consumer crossed allocated record region");}
    for(U32 i=sizeof(U32);i<NativeWaveStreamPrefix;++i)require(storage[streamOffset+i]==0xa5,"Stream counter overwrote native alignment padding");
    for(U32 i=sizeof(U32);i<NativeAdpcmInfoPrefix;++i)require(storage[adpcmOffset+i]==0xa5,"ADPCM counter overwrote native alignment padding");
}
}
int main(){try{for(U32 count:{1u,2u,7u})run(count);std::cout<<"Actual native audio Instance/core stream and ADPCM alignment, last-slot bounds, counters and streaming/resident PCM agreement PASS (not game banks)\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
