#include "precompiled.h"

VoiceEncoder_Silk::VoiceEncoder_Silk()
{
	m_pEncoder = nullptr;
	m_pDecoder = nullptr;
	m_bitrate = 10000;
	m_packetLoss_perc = 0;
	m_samplerate = 24000;
}
VoiceEncoder_Silk::~VoiceEncoder_Silk()
{
	if (m_pEncoder) {
		free(m_pEncoder);
		m_pEncoder = nullptr;
	}

	if (m_pDecoder) {
		free(m_pDecoder);
		m_pDecoder = nullptr;
	}
}

bool VoiceEncoder_Silk::Init(int quality)
{
	m_bitrate = 10000;
	m_packetLoss_perc = 0;

	int encSizeBytes;
	SKP_Silk_SDK_Get_Encoder_Size(&encSizeBytes);
	m_pEncoder = malloc(encSizeBytes);
	SKP_Silk_SDK_InitEncoder(m_pEncoder, &this->m_encControl);

	int decSizeBytes;
	SKP_Silk_SDK_Get_Decoder_Size(&decSizeBytes);
	m_pDecoder = malloc(decSizeBytes);
	SKP_Silk_SDK_InitDecoder(m_pDecoder);

	return true;
}

void VoiceEncoder_Silk::Release()
{
	delete this;
}

bool VoiceEncoder_Silk::ResetState()
{
	if (m_pEncoder)
		SKP_Silk_SDK_InitEncoder(m_pEncoder, &this->m_encControl);

	if (m_pDecoder)
		SKP_Silk_SDK_InitDecoder(m_pDecoder);

	m_bufOverflowBytes.Clear();

	return true;
}

uint16_t VoiceEncoder_Silk::SampleRate()
{
	return m_samplerate;
}

int VoiceEncoder_Silk::Compress(const char *pUncompressedIn, int nSamplesIn, char *pCompressed, int maxCompressedBytes, bool bFinal)
{
	signed int nSamplesToUse; // edi@4
	const int16_t*psRead; // ecx@4
	int nSamples; // edi@5
	int nSamplesToEncode; // esi@6
	char *pWritePos; // ebp@6
	int nSamplesPerFrame; // [sp+28h] [bp-44h]@5
	const char *pWritePosMax; // [sp+2Ch] [bp-40h]@5
	int nSamplesRemaining; // [sp+38h] [bp-34h]@5

	const int inSampleRate = m_samplerate;

	/*
	if ((nSamplesIn + m_bufOverflowBytes.TellPut() / 2) < nSamplesMin && !bFinal) {
		m_bufOverflowBytes.Put(pUncompressedIn, 2 * nSamplesIn);
		return 0;
	}
	*/

	if (m_bufOverflowBytes.TellPut()) {
		m_bufOverflowBytes.Put(pUncompressedIn, BYTES_PER_SAMPLE * nSamplesIn);

		psRead = static_cast<const int16_t*>(m_bufOverflowBytes.Base());
		nSamplesToUse = m_bufOverflowBytes.TellPut() / BYTES_PER_SAMPLE;
	} else {
		psRead = reinterpret_cast<const int16_t*>(pUncompressedIn);
		nSamplesToUse = nSamplesIn;
	}

	nSamplesPerFrame = inSampleRate / 50;
	nSamplesRemaining = nSamplesToUse % nSamplesPerFrame;
	pWritePosMax = pCompressed + maxCompressedBytes;
	nSamples = nSamplesToUse - nSamplesRemaining;
	pWritePos = pCompressed;

	while (nSamples > 0)
	{
		int16 *pWritePayloadSize = (int16 *)pWritePos;
		pWritePos += sizeof(int16); //leave 2 bytes for the frame size (will be written after encoding)

		int originalNBytes = (pWritePosMax - pWritePos > 0xFFFF) ? -1 : (pWritePosMax - pWritePos);
		nSamplesToEncode = (nSamples < nSamplesPerFrame) ? nSamples : nSamplesPerFrame;

		this->m_encControl.useDTX = 0;
		this->m_encControl.maxInternalSampleRate =m_samplerate;
		this->m_encControl.useInBandFEC = 0;
		this->m_encControl.API_sampleRate = inSampleRate;
		this->m_encControl.complexity = 2;
		this->m_encControl.packetSize = inSampleRate/50;
		this->m_encControl.packetLossPercentage = this->m_packetLoss_perc;
		this->m_encControl.bitRate = (m_bitrate >= 0) ? m_bitrate : 0;

		nSamples -= nSamplesToEncode;

		int16 nBytes = originalNBytes;
		int res = SKP_Silk_SDK_Encode(this->m_pEncoder, &this->m_encControl, psRead, nSamplesToEncode, (unsigned char *)pWritePos, &nBytes);
		*pWritePayloadSize = nBytes; //write frame size

		pWritePos += nBytes;
		psRead += nSamplesToEncode;
	}

	m_bufOverflowBytes.Clear();

	if (nSamplesRemaining <= nSamplesIn && nSamplesRemaining) {
		m_bufOverflowBytes.Put(&pUncompressedIn[2 * (nSamplesIn - nSamplesRemaining)], BYTES_PER_SAMPLE * nSamplesRemaining);
	}

	if (bFinal)
	{
		ResetState();

		if (pWritePosMax > pWritePos + BYTES_PER_SAMPLE) {
			uint16 *pWriteEndFlag = (uint16*)pWritePos;
			pWritePos += sizeof(uint16);
			*pWriteEndFlag = 0xFFFF;
		}
	}

	return pWritePos - pCompressed;
}

int VoiceEncoder_Silk::Decompress(const char *pCompressed, int compressedBytes, char *pUncompressed, int maxUncompressedBytes)
{
	int nPayloadSize; // ebp@2
	char *pWritePos; // ebx@5
	const char *pReadPos; // edx@5
	char *pWritePosMax; // [sp+28h] [bp-44h]@4
	const char *pReadPosMax; // [sp+3Ch] [bp-30h]@1

	const int outSampleRate = m_samplerate;

	m_decControl.API_sampleRate = outSampleRate;
	int nSamplesPerFrame = outSampleRate / 50;

	if (compressedBytes <= 0) {
		return 0;
	}

	pReadPosMax = &pCompressed[compressedBytes];
	pReadPos = pCompressed;

	pWritePos = pUncompressed;
	pWritePosMax = &pUncompressed[maxUncompressedBytes];

	while (pReadPos < pReadPosMax)
	{
		if (pReadPosMax - pReadPos < BYTES_PER_SAMPLE) {
			break;
		}

		nPayloadSize = *(uint16 *)pReadPos;
		pReadPos += sizeof(uint16);

		if (nPayloadSize == 0xFFFF) {
			ResetState();
			break;
		}

		if (nPayloadSize == 0) {
			//DTX (discontinued transmission)
			int numEmptySamples = nSamplesPerFrame;
			short nSamples = (pWritePosMax - pWritePos) / BYTES_PER_SAMPLE;

			if (nSamples < numEmptySamples) {
				break;
			}

			memset(pWritePos, 0, numEmptySamples * BYTES_PER_SAMPLE);
			pWritePos += numEmptySamples * BYTES_PER_SAMPLE;

			continue;
		}

		if ((pReadPos + nPayloadSize) > pReadPosMax) {
			break;
		}

		do {
			short nSamples = (pWritePosMax - pWritePos) / BYTES_PER_SAMPLE;
			int decodeRes = SKP_Silk_SDK_Decode(m_pDecoder, &m_decControl, 0, (const unsigned char*)pReadPos, nPayloadSize, (int16_t*)pWritePos, &nSamples);

			if (SKP_SILK_NO_ERROR != decodeRes) {
				return (pWritePos - pUncompressed);
			}

			pWritePos += nSamples * BYTES_PER_SAMPLE;
		} while (m_decControl.moreInternalDecoderFrames);
		pReadPos += nPayloadSize;
	}

	return (pWritePos - pUncompressed) / BYTES_PER_SAMPLE;
}

int VoiceEncoder_Silk::CodecType()
{
	return CSteamP2PCodec::PLT_Silk;
}

void VoiceEncoder_Silk::SetBitRate(float bitRate)
{
	if (bitRate != m_bitrate)
	{
		m_bitrate = bitRate;
		if (m_pEncoder) {
			free(m_pEncoder);
			m_pEncoder = nullptr;
		}

		if (m_pDecoder) {
			free(m_pDecoder);
			m_pDecoder = nullptr;
		}
		Init(10);
	}
}

void VoiceEncoder_Silk::SetSampleRate(uint16_t sampleRate)
{
	if (sampleRate != m_samplerate)
	{
		m_samplerate = sampleRate;
		if (m_pEncoder) {
			free(m_pEncoder);
			m_pEncoder = nullptr;
		}

		if (m_pDecoder) {
			free(m_pDecoder);
			m_pDecoder = nullptr;
		}
		Init(10);
	}
}
