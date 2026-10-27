#include "AudioPch.h"
#include "coreheaders.h"



#ifdef AUDIO_OPTIMIZE_OFF
#pragma optimize ("", off) // AUDIO_OPTIMIZE_OFF
#endif	//	AUDIO_OPTIMIZE_OFF

////////////////////////////////////////////////////////////////////////////////
//	static data
////////////////////////////////////////////////////////////////////////////////
bool Audio::Core::DSP::PitchShift::bDisableAll = false;

////////////////////////////////////////////////////////////////////////////////
//	PitchShift
////////////////////////////////////////////////////////////////////////////////
#ifndef PLATFORM_SPU
Audio::Core::DSP::PitchShift::PitchShift(const char* pchInfo,U32 u32NumChannels) : Effect(pchInfo,u32NumChannels)
{
#ifndef AUDIO_DISABLE_DSP

    m_eType = Core::DSP::Effect::ePitchShift;
#if defined(PLATFORM_MACOS) // [Blur2PC] Native effect state is explicit; the old fields were uninitialized.
    if(u32NumChannels!=1&&u32NumChannels!=2)throw std::invalid_argument("Native pitch shift supports mono or stereo");
    m_bActive=true;m_fPitch=0;m_fPrevPitch=0;m_NativeSampleRate=Core::uSampleRate;
    stSoundTouch.setSampleRate(m_NativeSampleRate);stSoundTouch.setChannels(u32NumChannels);
    stSoundTouch.setTempo(1);stSoundTouch.setPitch(1);stSoundTouch.setRate(1);
    return;
#endif

	//setup stuff

// temp, to get compiling.. jkc
//#ifdef PLATFORM_XENON
//	stSoundTouch.setSampleRate(XAUDIOSAMPLERATE_NATIVE);
//#else
	stSoundTouch.setSampleRate(48000);
//#endif
    stSoundTouch.setChannels(2);

    stSoundTouch.setTempoChange(0);//params->tempoDelta
    stSoundTouch.setPitchOctaves(0);//params->pitchDelta
    stSoundTouch.setRateChange(0);//params->rateDelta

    stSoundTouch.setSetting(SETTING_USE_QUICKSEEK, 0);//params->quick
    stSoundTouch.setSetting(SETTING_USE_AA_FILTER, 0);//!params->noAntiAliasy

	stSoundTouch.flush(); //reset
	
	stSoundTouch.setChannels(2);
	
	m_idelayCounter = 0;
	
	m_bufferUnderrunDelay = 30;//gGame.m_VariSpeed.iPitchShiftDelay; // //minimum frame delay required to prevent buffer underrun

	m_fPrevPitch = -1;
	

#endif	//	#ifndef AUDIO_DISABLE_DSP
}
#endif

////////////////////////////////////////////////////////////////////////////////
//	Process
////////////////////////////////////////////////////////////////////////////////
bool Audio::Core::DSP::PitchShift::Process(void* pFrameData, const ProcessInfo& info, bool bProcess)
{
	AUDIO_PERF_MICRO_EVENT("Audio.Generate.DSP.PitchShift.Process");
#if defined(PLATFORM_MACOS) // [Blur2PC] Feed actual native 128-frame planar blocks to upstream SoundTouch.
    if(!m_bActive||bDisableAll||m_fPitch==0){
        if(m_fPrevPitch!=0){stSoundTouch.clear();m_fPrevPitch=0;}
        return true;
    }
    const U32 rate=U32(Core::fSampleRate/F32(Core::fOutputRateRatio));
    if(rate!=m_NativeSampleRate){stSoundTouch.setSampleRate(rate);m_NativeSampleRate=rate;}
    if(m_fPitch!=m_fPrevPitch){stSoundTouch.setPitchOctaves(m_fPitch);m_fPrevPitch=m_fPitch;}
    F32 interleaved[2*Core::uFrameLength]{};auto* planar=static_cast<F32*>(pFrameData);
    for(U32 frame=0;frame<Core::uFrameLength;++frame)for(U32 channel=0;channel<m_uChannels;++channel)
        interleaved[frame*m_uChannels+channel]=planar[channel*Core::uFrameLength+frame];
    stSoundTouch.putSamples(interleaved,Core::uFrameLength);
    const U32 received=stSoundTouch.receiveSamples(interleaved,Core::uFrameLength);
    for(U32 frame=0;frame<Core::uFrameLength;++frame)for(U32 channel=0;channel<m_uChannels;++channel)
        planar[channel*Core::uFrameLength+frame]=frame<received?interleaved[frame*m_uChannels+channel]:0.f;
    return true;
#else


#ifndef PLATFORM_SPU
#ifndef AUDIO_DISABLE_DSP

	if( !m_bActive || bDisableAll )
		return true;

	static bool processBypass = true;

	if(!m_fPitch & processBypass)
		return true; //don't process.

	F32* FrameBufferData = (F32*)pFrameData;;

		//process
	int nChannels = 2;
	

	int singleBufferFrameSize = /*OUTBUFF_SIZE*/512 / nChannels;

	int nSamples = singleBufferFrameSize * nChannels;
	//int outBuffSize = 4096 * nChannels;
    int buffSizeSamples;


	if(m_fPitch != m_fPrevPitch)
	{
		stSoundTouch.setPitchOctaves(m_fPitch);
		m_fPrevPitch = m_fPitch;
	}


	SAMPLETYPE sampleBuffer[512];
	SAMPLETYPE tempBuffer[512];
	
    buffSizeSamples = nSamples / nChannels;
	
	//get samples
	for( int j = 0; j < singleBufferFrameSize*nChannels; j++ )
	{
		sampleBuffer[j] =  FrameBufferData[j]/*( FrameBufferData.pSampleBuffer[j] )*/;
		
	}

	if(nChannels == 2)
	{						
	//rearrange buffer for sample interleave
		int tempCounter = 0;
		for(int i = 0;i<singleBufferFrameSize*2;i++)
		{			

			if(i%2 == 0)//odd index
				tempBuffer[i] = sampleBuffer[tempCounter];
			if(i%2 == 1)//even index
			{
			tempBuffer[i] = sampleBuffer[tempCounter+singleBufferFrameSize];
			tempCounter++;
			}
		}

			for(int i = 0;i<singleBufferFrameSize*2;i++)
				sampleBuffer[i] = tempBuffer[i] * .25f;
	}


	stSoundTouch.putSamples(sampleBuffer, buffSizeSamples);

	if(m_idelayCounter >= m_bufferUnderrunDelay)//wait initially 25 frames until we process
	{
		nSamples = stSoundTouch.receiveSamples(sampleBuffer, buffSizeSamples);

	

		if(nSamples != 256 ) //if nothing has come out of the soundtouch process
		{
			int i = 0;
			i++;
		}

		if(nChannels == 2)
		{
			int tempCounter = 0;
			//rearrange buffer for Frame interleave
			for(int i = 0;i<singleBufferFrameSize*2;i++)
			{
				if(i%2 == 0)
					tempBuffer[tempCounter] = sampleBuffer[i];
				if(i%2 == 1)
				{
					tempBuffer[tempCounter+singleBufferFrameSize] = sampleBuffer[i];
					tempCounter++;
				}
			}
			for(int i = 0;i<singleBufferFrameSize*2;i++)
				FrameBufferData[i] = tempBuffer[i] / .25f;
		}

		nChannels++;
		nChannels	= 2;

	}
	else
	for( int j = 0; j < singleBufferFrameSize * nChannels; j++ )//initial buffer underrun delay
		FrameBufferData[j] = 0;

	m_idelayCounter++;

    return true;

#else // AUDIO_DISABLE_DSP
	return false;
#endif // AUDIO_DISABLE_DSP
#else	// PLATFORM_SPU
sLOGERROR(0, "DSP pitch shift not implemented on SPU");
return false;
#endif
#endif // [Blur2PC] native upstream processing
}

#ifdef AUDIO_OPTIMIZE_OFF
#pragma optimize ("", on) // AUDIO_OPTIMIZE_OFF
#endif	//	AUDIO_OPTIMIZE_OFF
