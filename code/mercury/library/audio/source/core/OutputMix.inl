// [Blur2PC] Shared original retail-PC mixing: included by the PC and native output engines.
////////////////////////////////////////////////////////////////////////////////
//	Mixing (0x00544130, 0x00544400, 0x005458c0): renderer frames (planar, 128 samples per channel) added into
//	an interleaved frame of uChannels channels
////////////////////////////////////////////////////////////////////////////////
void Audio::Core::Engine::MixMono(const F32* pfIn, F32* pfOut, U32 uChannels)
{
	switch (uChannels)
	{
		case 2:
		case 3:
		case 4:
			for (U32 j = 0; j < uFrameLength; j++)
			{
				pfOut[j * uChannels] += pfIn[j];
				pfOut[j * uChannels + 1] += pfIn[j];
			}
			break;

		case 5:
		case 6:
		case 8:
			for (U32 j = 0; j < uFrameLength; j++)
				pfOut[j * uChannels + 2] += pfIn[j];
			break;

		default:
			break;
	}
}

void Audio::Core::Engine::MixStereo(const F32* pfIn, F32* pfOut, U32 uChannels)
{
	// retail reads input j and j + 1 for the two sides of output frame j, as the snapshot's OpenAL mix did,
	// and uses this for the surround frame as well
	if (m_uChannels >= 2 && (m_uChannels <= 6 || m_uChannels == 8))
	{
		for (U32 j = 0; j < uFrameLength; j++)
		{
			pfOut[j * uChannels] += pfIn[j];
			pfOut[j * uChannels + 1] += pfIn[j + 1];
		}
	}
}

void Audio::Core::Engine::MixSurroundLFE(const F32* pfIn, F32* pfOut, U32 uChannels)
{
	// input planes: 0 front left, 1 front right, 2 centre, 3 LFE, 4 surround left, 5 surround right
	const F32* pfL = pfIn;
	const F32* pfR = pfIn + uFrameLength;
	const F32* pfC = pfIn + uFrameLength * 2;
	const F32* pfLFE = pfIn + uFrameLength * 3;
	const F32* pfSL = pfIn + uFrameLength * 4;
	const F32* pfSR = pfIn + uFrameLength * 5;

	switch (m_uChannels)
	{
		case 2:
			for (U32 j = 0; j < uFrameLength; j++)
			{
				F32* pf = pfOut + j * uChannels;
				pf[0] += pfL[j];
				pf[1] += pfR[j];
				pf[0] += pfC[j] * 0.5f;
				pf[1] += pfC[j] * 0.5f;
			}
			break;

		case 3:
			for (U32 j = 0; j < uFrameLength; j++)
			{
				F32* pf = pfOut + j * uChannels;
				pf[0] += pfL[j];
				pf[1] += pfR[j];
				pf[2] += pfLFE[j];
				pf[0] += pfSL[j] * 0.5f;
				pf[1] += pfSR[j] * 0.5f;
			}
			break;

		case 4:
			for (U32 j = 0; j < uFrameLength; j++)
			{
				F32* pf = pfOut + j * uChannels;
				pf[0] += pfL[j];
				pf[1] += pfR[j];
				pf[2] += pfSL[j];
				pf[3] += pfSR[j];
			}
			break;

		case 5:
			for (U32 j = 0; j < uFrameLength; j++)
			{
				F32* pf = pfOut + j * uChannels;
				pf[0] += pfL[j];
				pf[1] += pfR[j];
				pf[2] += pfC[j];
				pf[3] += pfSL[j];
				pf[4] += pfSR[j];
			}
			break;

		case 6:
			for (U32 j = 0; j < uFrameLength; j++)
			{
				F32* pf = pfOut + j * uChannels;
				pf[0] += pfL[j];
				pf[1] += pfR[j];
				pf[2] += pfC[j];
				pf[3] += pfLFE[j];
				pf[4] += pfSL[j];
				pf[5] += pfSR[j];
			}
			break;

		case 8:
			for (U32 j = 0; j < uFrameLength; j++)
			{
				F32* pf = pfOut + j * uChannels;
				pf[0] += pfL[j];
				pf[1] += pfR[j];
				pf[2] += pfC[j];
				pf[3] += pfLFE[j];
				pf[4] += pfSL[j];
				pf[5] += pfSR[j];
				pf[6] += pfSL[j] * 0.25f;
				pf[7] += pfSR[j] * 0.25f;
			}
			break;

		default:
			break;
	}
}

