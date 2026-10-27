// [Blur2PC][macOS] FFmpeg owns demux/codec conversion; SDL owns CoreAudio output.
// Movie/UI scheduling uses media timestamps and elapsed time, never rendered frame count.
#include "../../stdafx.h"
#include <movie/mac/MacMovie.h>
#include <system/serialise/C_FileSystem.h>
#include <SDL3/SDL.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <stdexcept>
#include <vector>
namespace Movie {
namespace {
using Clock=std::chrono::steady_clock;
void check(bool value,const char* error){if(!value)throw std::runtime_error(error);}
void avcheck(int result,const char* operation){if(result<0){char text[AV_ERROR_MAX_STRING_SIZE];av_strerror(result,text,sizeof(text));throw std::runtime_error(std::string(operation)+": "+text);}}
void sdlcheck(bool result){if(!result)throw std::runtime_error(SDL_GetError());}
}
struct C_MacMovie::State {
    struct VideoFrame {std::shared_ptr<AVFrame> frame;double pts=0,end=0;};
    struct AudioBlock {std::vector<F32> samples;double pts=0;};
    struct AudioTrack {
        int index=-1;
        AVCodecContext* codec=nullptr;
        SwrContext* converter=nullptr;
        SDL_AudioStream* output=nullptr;
        std::deque<AudioBlock> blocks;
        AVChannelLayout inputLayout{};
        int inputRate=0,inputFormat=-1;
        double decodedEnd=0,queuedEnd=0;
        ~AudioTrack(){if(output)SDL_DestroyAudioStream(output);swr_free(&converter);av_channel_layout_uninit(&inputLayout);avcodec_free_context(&codec);}
    };
    AVFormatContext* format=nullptr;
    AVIOContext* io=nullptr;
    AVCodecContext* video=nullptr;
    AVPacket* packet=nullptr;
    AVFrame* scratch=nullptr;
    SwsContext* colour=nullptr;
    std::unique_ptr<Sys::I_File> file;
    std::vector<U8> ownedData;
    const U8* memory=nullptr;
    U64 length=0,position=0;
    int videoIndex=-1,width=0,height=0;
    double origin=0,frameDuration=0,decodedEnd=0,time=0;
    bool eof=false,ended=false,failed=false,dirty=false,audioReference=false;
    std::deque<VideoFrame> frames;
    VideoFrame current;
    std::vector<std::unique_ptr<AudioTrack>> audio;
    Clock::time_point last=Clock::now();
    U64 videoFrames=0,audioFrames=0,submittedFrames=0;
    F32 audioPeak=0;
    ~State(){
        audio.clear();if(audioReference)SDL_QuitSubSystem(SDL_INIT_AUDIO);
        sws_freeContext(colour);av_frame_free(&scratch);av_packet_free(&packet);avcodec_free_context(&video);avformat_close_input(&format);
        if(io)av_freep(&io->buffer);avio_context_free(&io);
    }
    static int Read(void* opaque,U8* destination,int bytes){
        auto& s=*static_cast<State*>(opaque);
        try {
            const U64 remaining=s.length-s.position;
            if(!remaining)return AVERROR_EOF;
            const int amount=int(std::min<U64>(remaining,U64(bytes)));
            if(s.memory)std::memcpy(destination,s.memory+s.position,amount);
            else if(!s.file||!s.file->Read(destination,U32(amount)))return AVERROR(EIO);
            s.position+=amount;return amount;
        }catch(...){return AVERROR(EIO);}
    }
    static int64_t Seek(void* opaque,int64_t offset,int whence){
        auto& s=*static_cast<State*>(opaque);
        try {
            if(whence==AVSEEK_SIZE)return int64_t(s.length);
            whence&=~AVSEEK_FORCE;
            const S64 base=whence==SEEK_SET?0:whence==SEEK_CUR?S64(s.position):whence==SEEK_END?S64(s.length):-1;
            if(base<0||(offset>0&&base>std::numeric_limits<S64>::max()-offset)||(offset<0&&offset< -base))return AVERROR(EINVAL);
            const S64 target=base+offset;if(target<0||U64(target)>s.length)return AVERROR(EINVAL);
            if(s.file)s.file->SetPosition(U64(target));s.position=U64(target);return target;
        }catch(...){return AVERROR(EIO);}
    }
    static AVCodecContext* OpenDecoder(AVStream* stream){
        const auto* implementation=avcodec_find_decoder(stream->codecpar->codec_id);check(implementation,"FFmpeg has no decoder for this movie stream");
        AVCodecContext* codec=avcodec_alloc_context3(implementation);check(codec,"FFmpeg codec allocation failed");
        try{avcheck(avcodec_parameters_to_context(codec,stream->codecpar),"Movie codec parameters");avcheck(avcodec_open2(codec,implementation,nullptr),"Movie codec open");}
        catch(...){avcodec_free_context(&codec);throw;}return codec;
    }
    double Pts(const AVFrame& frame,int index,double fallback)const{
        return frame.best_effort_timestamp==AV_NOPTS_VALUE?fallback:frame.best_effort_timestamp*av_q2d(format->streams[index]->time_base)-origin;
    }
    void ReceiveVideo(){
        for(;;){const int result=avcodec_receive_frame(video,scratch);if(result==AVERROR(EAGAIN)||result==AVERROR_EOF)break;avcheck(result,"Movie video decode");
            check(scratch->width==width&&scratch->height==height,"Movie changed resolution during playback");
            AVFrame* retained=av_frame_clone(scratch);check(retained,"Movie frame retention failed");
            const double pts=Pts(*scratch,videoIndex,decodedEnd);
            const double duration=scratch->duration>0?scratch->duration*av_q2d(format->streams[videoIndex]->time_base):frameDuration;
            frames.push_back({std::shared_ptr<AVFrame>(retained,[](AVFrame* p){av_frame_free(&p);}),pts,pts+duration});
            decodedEnd=std::max(decodedEnd,pts+duration);++videoFrames;av_frame_unref(scratch);
            check(frames.size()<=1024,"Movie demux ordering exceeded the bounded video queue");
        }
    }
    void ReceiveAudio(AudioTrack& track){
        for(;;){const int result=avcodec_receive_frame(track.codec,scratch);if(result==AVERROR(EAGAIN)||result==AVERROR_EOF)break;avcheck(result,"Movie audio decode");
            const int rate=scratch->sample_rate;check(rate>0&&scratch->ch_layout.nb_channels>0,"Decoded movie audio has no channel/sample-rate metadata");
            if(!track.converter){
                const AVChannelLayout stereo=AV_CHANNEL_LAYOUT_STEREO;
                avcheck(swr_alloc_set_opts2(&track.converter,&stereo,AV_SAMPLE_FMT_FLT,48000,&scratch->ch_layout,AVSampleFormat(scratch->format),rate,0,nullptr),"Movie audio resampler allocation");
                avcheck(swr_init(track.converter),"Movie audio resampler initialization");
                track.inputRate=rate;track.inputFormat=scratch->format;avcheck(av_channel_layout_copy(&track.inputLayout,&scratch->ch_layout),"Movie audio channel layout");
            }
            check(track.inputRate==rate&&track.inputFormat==scratch->format&&av_channel_layout_compare(&track.inputLayout,&scratch->ch_layout)==0,"Movie audio changed its stream format during playback");
            const S64 delay=swr_get_delay(track.converter,rate);
            const S64 capacity=av_rescale_rnd(delay+scratch->nb_samples,48000,rate,AV_ROUND_UP);
            check(capacity>0&&capacity<=48000*10,"Movie audio frame is too large");
            AudioBlock block;block.pts=Pts(*scratch,track.index,track.decodedEnd)-double(delay)/rate;block.samples.resize(std::size_t(capacity)*2);
            U8* output=reinterpret_cast<U8*>(block.samples.data());
            const int produced=swr_convert(track.converter,&output,int(capacity),const_cast<const U8**>(scratch->extended_data),scratch->nb_samples);avcheck(produced,"Movie audio conversion");block.samples.resize(std::size_t(produced)*2);
            for(F32 value:block.samples){check(std::isfinite(value),"Movie decoder produced non-finite audio");audioPeak=std::max(audioPeak,std::abs(value));}
            audioFrames+=produced;track.decodedEnd=std::max(track.decodedEnd,block.pts+double(produced)/48000);
            if(produced)track.blocks.push_back(std::move(block));av_frame_unref(scratch);
            check(track.blocks.size()<=4096,"Movie demux ordering exceeded the bounded audio queue");
        }
    }
    void SendVideo(const AVPacket* value){
        int result=avcodec_send_packet(video,value);if(result==AVERROR(EAGAIN)){ReceiveVideo();result=avcodec_send_packet(video,value);}if(result!=AVERROR_EOF)avcheck(result,"Movie video packet");ReceiveVideo();
    }
    void SendAudio(AudioTrack& track,const AVPacket* value){
        int result=avcodec_send_packet(track.codec,value);if(result==AVERROR(EAGAIN)){ReceiveAudio(track);result=avcodec_send_packet(track.codec,value);}if(result!=AVERROR_EOF)avcheck(result,"Movie audio packet");ReceiveAudio(track);
    }
    void FlushAudio(AudioTrack& track){
        if(!track.converter)return;
        for(;;){
            const int capacity=std::max(32,swr_get_out_samples(track.converter,0));avcheck(capacity,"Movie audio flush capacity");
            AudioBlock block;block.pts=track.decodedEnd;block.samples.resize(std::size_t(capacity)*2);U8* output=reinterpret_cast<U8*>(block.samples.data());
            const int produced=swr_convert(track.converter,&output,capacity,nullptr,0);avcheck(produced,"Movie audio resampler drain");if(!produced)break;
            block.samples.resize(std::size_t(produced)*2);for(F32 value:block.samples){check(std::isfinite(value),"Movie decoder produced non-finite audio tail");audioPeak=std::max(audioPeak,std::abs(value));}
            audioFrames+=produced;track.decodedEnd+=double(produced)/48000;track.blocks.push_back(std::move(block));
        }
    }
    void DecodeUntil(double target){
        // Keep a short timestamp window ahead. Audio packets interleaved with that
        // window are decoded too; no frame-count cadence or whole-movie preload.
        U32 packets=0;
        while(!eof&&(frames.empty()||frames.back().pts<target+.20)){
            check(++packets<=65536,"Movie made no bounded demux progress");
            const int result=av_read_frame(format,packet);
            if(result==AVERROR_EOF){SendVideo(nullptr);for(auto& track:audio){SendAudio(*track,nullptr);FlushAudio(*track);}eof=true;break;}
            avcheck(result,"Movie packet read");
            if(packet->stream_index==videoIndex)SendVideo(packet);
            else for(auto& track:audio)if(packet->stream_index==track->index){SendAudio(*track,packet);break;}
            av_packet_unref(packet);
        }
        while(!frames.empty()&&(!current.frame||frames.front().pts<=target)){
            current=std::move(frames.front());frames.pop_front();dirty=true;
            if(!frames.empty()&&frames.front().pts>target)break;
        }
    }
    void AudioControls(bool enabled,bool paused,S32 volume){
        const F32 gain=enabled?F32(std::max(0,volume))/32768.f:0;
        for(auto& track:audio){sdlcheck(SDL_SetAudioStreamGain(track->output,gain));sdlcheck(paused?SDL_PauseAudioStreamDevice(track->output):SDL_ResumeAudioStreamDevice(track->output));}
    }
    void QueueAudio(double target){
        for(auto& track:audio)while(!track->blocks.empty()&&track->blocks.front().pts<=target+.20){
            auto block=std::move(track->blocks.front());track->blocks.pop_front();
            // Preserve real timestamp gaps/overlaps instead of concatenating
            // independently timed tracks or playing delayed audio too early.
            check(std::isfinite(block.pts)&&block.pts>double(std::numeric_limits<S64>::min())/48000+1&&block.pts<double(std::numeric_limits<S64>::max())/48000-1,"Movie audio timestamp exceeds the sample address range");
            const S64 desired=std::llround(block.pts*48000);
            S64 queued=std::llround(track->queuedEnd*48000);
            if(desired>queued){check(desired-queued<=48000*60,"Movie audio timestamp gap exceeds one minute");std::vector<F32> silence(std::size_t(desired-queued)*2,0);sdlcheck(SDL_PutAudioStreamData(track->output,silence.data(),int(silence.size()*sizeof(F32))));submittedFrames+=desired-queued;queued=desired;}
            const S64 frames=S64(block.samples.size()/2);const S64 skip=std::max<S64>(0,queued-desired);
            if(skip<frames){const S64 amount=frames-skip;sdlcheck(SDL_PutAudioStreamData(track->output,block.samples.data()+skip*2,int(amount*2*sizeof(F32))));submittedFrames+=amount;queued+=amount;}
            track->queuedEnd=double(queued)/48000;
        }
    }
    void SeekStart(){
        const auto* stream=format->streams[videoIndex];const S64 start=stream->start_time==AV_NOPTS_VALUE?0:stream->start_time;
        avcheck(av_seek_frame(format,videoIndex,start,AVSEEK_FLAG_BACKWARD),"Movie rewind");avcodec_flush_buffers(video);av_packet_unref(packet);
        for(auto& track:audio){avcodec_flush_buffers(track->codec);if(track->converter){swr_close(track->converter);avcheck(swr_init(track->converter),"Movie rewind audio resampler");}track->blocks.clear();track->decodedEnd=track->queuedEnd=0;sdlcheck(SDL_ClearAudioStream(track->output));}
        frames.clear();current={};time=decodedEnd=0;eof=ended=dirty=false;last=Clock::now();DecodeUntil(0);QueueAudio(0);
    }
};
C_MacMovie::C_MacMovie(){m_Audio=true;} // Bink playback originally enables its selected sound tracks by default.
C_MacMovie::~C_MacMovie(){DestroyBink();}
bool C_MacMovie::CreateBink(const C_MovieParams& params){
    if(m_State){m_Error="Native movie is already open";return false;}
    m_Error.clear();m_CurrentTime=m_TotalTime=0;
    try{
        check(CreateShaders(),"Native movie requires a created Display device");
        auto state=std::make_unique<State>();const U32 modes=params.m_CreateFlags&~U32(E_MovieCreateFlags_SetBufferIOSize);
        check(!(params.m_CreateFlags&~15U)&&(modes==E_MovieCreateFlags_StreamFromDisk||modes==E_MovieCreateFlags_LoadIntoMemory||modes==E_MovieCreateFlags_PlayFromMemory),"Unsupported native movie creation flags");
        const U32 ioSize=params.m_CreateFlags&E_MovieCreateFlags_SetBufferIOSize?params.m_BinkIOSize:32768;
        check(ioSize>=4096&&ioSize<=16*1024*1024,"Movie I/O buffer must be between 4096 and 16777216 bytes");
        // The original boot/UI contract also uses PlayFromMemory with a filename
        // and no external buffer: the engine filesystem supplies owned bytes.
        // External buffers remain bounded explicitly; never infer their size.
        if(modes==E_MovieCreateFlags_PlayFromMemory&&params.m_pMovieData){check(params.m_MovieDataSize,"Memory movie requires an explicit m_MovieDataSize");state->memory=static_cast<const U8*>(params.m_pMovieData);state->length=params.m_MovieDataSize;}
        else {
            check(modes!=E_MovieCreateFlags_PlayFromMemory||!params.m_MovieDataSize,"Memory movie byte size requires a corresponding pointer");
            check(m_FileName.Length()!=0,"Movie filename is empty");
            auto* filesystem=Sys::GetDefaultFileSystem();check(filesystem,"Movie requires the engine's configured default filesystem");
            state->file.reset(filesystem->Open(m_FileName.Chars(),Sys::E_FileAccess_Read));check(bool(state->file),"Movie file could not be opened by the engine filesystem");state->length=state->file->Length();
            if(modes==E_MovieCreateFlags_LoadIntoMemory||modes==E_MovieCreateFlags_PlayFromMemory){check(state->length<=std::numeric_limits<std::size_t>::max(),"Movie is too large to preload");state->ownedData.resize(std::size_t(state->length));U64 offset=0;while(offset<state->length){const U32 amount=U32(std::min<U64>(state->length-offset,1U<<24));check(state->file->Read(state->ownedData.data()+offset,amount),"Movie preload read failed");offset+=amount;}state->memory=state->ownedData.data();state->file.reset();}
        }
        check(state->length&&state->length<=U64(std::numeric_limits<S64>::max()),"Movie byte range is empty or exceeds the seekable range");
        U8* buffer=static_cast<U8*>(av_malloc(ioSize));check(buffer,"Movie I/O allocation failed");state->io=avio_alloc_context(buffer,int(ioSize),0,state.get(),State::Read,nullptr,State::Seek);if(!state->io){av_free(buffer);throw std::runtime_error("Movie I/O context allocation failed");}
        state->format=avformat_alloc_context();check(state->format,"Movie format allocation failed");state->format->pb=state->io;state->format->flags|=AVFMT_FLAG_CUSTOM_IO;
        avcheck(avformat_open_input(&state->format,nullptr,nullptr,nullptr),"Movie format open");avcheck(avformat_find_stream_info(state->format,nullptr),"Movie stream discovery");
        state->videoIndex=av_find_best_stream(state->format,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);avcheck(state->videoIndex,"Movie video stream");auto* stream=state->format->streams[state->videoIndex];state->video=State::OpenDecoder(stream);
        state->width=state->video->width;state->height=state->video->height;check(state->width>0&&state->height>0&&state->width<=16384&&state->height<=16384,"Movie resolution exceeds native texture limits");
        const AVRational rate=av_guess_frame_rate(state->format,stream,nullptr);check(rate.num>0&&rate.den>0,"Movie does not expose its video cadence");state->frameDuration=av_q2d(av_inv_q(rate));
        state->origin=state->format->start_time!=AV_NOPTS_VALUE?double(state->format->start_time)/AV_TIME_BASE:stream->start_time!=AV_NOPTS_VALUE?stream->start_time*av_q2d(stream->time_base):0;
        if(state->format->duration>0)m_TotalTime=F32(double(state->format->duration)/AV_TIME_BASE);
        else if(stream->duration>0)m_TotalTime=F32(stream->duration*av_q2d(stream->time_base));
        U32 ordinal=0;
        for(U32 i=0;i<state->format->nb_streams;++i)if(state->format->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            // FFmpeg's Bink demuxer preserves the file's track IDs in AVStream::id;
            // retain the original language-track selection contract. Generic new
            // media uses zero-based audio ordinals, independent of its video index.
            const U32 trackId=std::strcmp(state->format->iformat->name,"bink")==0?U32(state->format->streams[i]->id):ordinal;
            const bool selected=trackId!=~U32(0)&&std::find(std::begin(params.m_TrackIDsToPlay),std::end(params.m_TrackIDsToPlay),trackId)!=std::end(params.m_TrackIDsToPlay);++ordinal;if(!selected)continue;
            if(!state->audioReference){sdlcheck(SDL_InitSubSystem(SDL_INIT_AUDIO));state->audioReference=true;}
            auto track=std::make_unique<State::AudioTrack>();track->index=int(i);track->codec=State::OpenDecoder(state->format->streams[i]);SDL_AudioSpec spec{};spec.format=SDL_AUDIO_F32;spec.channels=2;spec.freq=48000;
            track->output=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);check(track->output,SDL_GetError());state->audio.push_back(std::move(track));
        }
        state->packet=av_packet_alloc();state->scratch=av_frame_alloc();check(state->packet&&state->scratch,"Movie frame/packet allocation failed");
        state->DecodeUntil(0);check(bool(state->current.frame),"Movie has no decodable video frame");state->QueueAudio(0);state->last=Clock::now();
        m_Params=params;m_State=std::move(state);return true;
    }catch(const std::exception& error){m_Error=error.what();LOGWARN(g_MovieLog,"Native movie open failed: %s",m_Error.c_str());return false;}
}
void C_MacMovie::DestroyBink(){m_State.reset();m_CurrentTime=m_TotalTime=0;}
bool C_MacMovie::CreateTextures(){
    if(!m_State)return false;
    try{auto& s=*m_State;check(Display::Device()->CreateRenderTexture2D(s.width,s.height,1,Display::E_Usage_Colour,Display::E_Format_A8R8G8B8,Display::E_Multisample_None,nullptr,&m_RenderTexture),"Native movie output texture allocation failed");UploadFrame();s.AudioControls(m_Audio,m_Paused,m_Volume);s.last=Clock::now();return true;}
    catch(const std::exception& error){m_Error=error.what();DestroyTextures();return false;}
}
void C_MacMovie::DestroyTextures(){if(m_RenderTexture.IsCreated())Display::Device()->Destroy(&m_RenderTexture);}
void C_MacMovie::UploadFrame(){
    if(!m_State||!m_State->dirty||!m_Video)return;auto& s=*m_State;auto* frame=s.current.frame.get();check(frame,"Movie has no selected decoded frame");
    s.colour=sws_getCachedContext(s.colour,s.width,s.height,AVPixelFormat(frame->format),s.width,s.height,AV_PIX_FMT_BGRA,SWS_BILINEAR,nullptr,nullptr,nullptr);check(s.colour,"Movie pixel converter allocation failed");
    const int* coefficients=sws_getCoefficients(frame->colorspace==AVCOL_SPC_UNSPECIFIED?SWS_CS_DEFAULT:int(frame->colorspace));
    avcheck(sws_setColorspaceDetails(s.colour,coefficients,frame->color_range==AVCOL_RANGE_JPEG,coefficients,1,0,1<<16,1<<16),"Movie colour conversion metadata");
    auto& texture=m_RenderTexture.GetTexture();const auto mapped=texture.Lock(0);check(mapped.pBits,"Movie output texture is still recorded/mapped by another operation");
    U8* output[]={mapped.pBits,nullptr,nullptr,nullptr};const int stride[]={int(mapped.pitch),0,0,0};const int rows=sws_scale(s.colour,frame->data,frame->linesize,0,s.height,output,stride);texture.Unlock(0);check(rows==s.height,"Movie pixel conversion produced incomplete output");s.dirty=false;
}
bool C_MacMovie::IsCreated()const{return m_State&&!m_State->failed&&m_RenderTexture.IsCreated();}
bool C_MacMovie::HasReachedEnd()const{return !m_State||m_State->ended||m_State->failed;}
void C_MacMovie::SetAudio(bool audio){m_Audio=audio;if(m_State)m_State->AudioControls(m_Audio,m_Paused,m_Volume);}
void C_MacMovie::SetPaused(bool paused){if(m_State&&!m_Paused)Update();m_Paused=paused;if(m_State){m_State->last=Clock::now();m_State->AudioControls(m_Audio,m_Paused,m_Volume);}}
void C_MacMovie::SetVolume(S32 volume){m_Volume=std::max(0,volume);if(m_State)m_State->AudioControls(m_Audio,m_Paused,m_Volume);}
void C_MacMovie::SetBinkIOSize(U32 size){m_Params.m_BinkIOSize=size;}
bool C_MacMovie::CreateShaders(){return Display::DeviceExists()&&Display::Device()->IsCreated();}
void C_MacMovie::DestroyShaders(){} // The native decoder owns no YUV conversion shaders; UI samples real BGRA textures.
void C_MacMovie::Rewind(){if(!m_State)return;try{m_State->SeekStart();m_CurrentTime=0;m_State->AudioControls(m_Audio,m_Paused,m_Volume);}catch(const std::exception& error){m_Error=error.what();m_State->failed=true;}}
bool C_MacMovie::Update(){
    if(!m_State)return true;auto& s=*m_State;if(s.failed)return true;
    try{
        if(s.ended){if(m_Looping){Rewind();return s.failed;}return true;}
        const auto now=Clock::now();if(!m_Paused)s.time+=std::chrono::duration<double>(now-s.last).count();s.last=now;
        if(!m_Paused){s.DecodeUntil(s.time);s.QueueAudio(s.time);}
        double end=s.decodedEnd;for(auto& track:s.audio)end=std::max(end,track->decodedEnd);
        if(s.eof){m_TotalTime=F32(std::max(double(m_TotalTime),end));if(s.time>=end&&s.frames.empty())s.ended=true;}
        m_CurrentTime=F32(s.ended?end:s.time);return s.ended;
    }catch(const std::exception& error){m_Error=error.what();s.failed=s.ended=true;LOGERROR(g_MovieLog,"Native movie playback failed: %s",m_Error.c_str());return true;}
}
void C_MacMovie::Render(){Update();if(!m_State||m_State->failed)return;try{UploadFrame();}catch(const std::exception& error){m_Error=error.what();m_State->failed=m_State->ended=true;LOGERROR(g_MovieLog,"Native movie upload failed: %s",m_Error.c_str());}}
void C_MacMovie::DebugRender(F32& x,F32& y,F32 size){if(!IsCreated()||!m_Video)return;check(Display::BasicRenderExists(),"Movie debug drawing requires the original BasicRender renderer");Display::BasicRender()->DrawTextured2DRect(x,y,size,size,Maths::RGBA(255,255,255,255),&m_RenderTexture.GetTexture());x+=size;}
const std::string& C_MacMovie::GetNativeError()const{return m_Error;}
U64 C_MacMovie::GetDecodedVideoFrames()const{return m_State?m_State->videoFrames:0;}
U64 C_MacMovie::GetDecodedAudioFrames()const{return m_State?m_State->audioFrames:0;}
U64 C_MacMovie::GetSubmittedAudioFrames()const{return m_State?m_State->submittedFrames:0;}
F32 C_MacMovie::GetDecodedAudioPeak()const{return m_State?m_State->audioPeak:0;}
}
