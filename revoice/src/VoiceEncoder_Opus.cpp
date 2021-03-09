#include "precompiled.h"

const int MAX_CHANNELS = 1;
const int MAX_PACKET_LOSS = 10;

VoiceEncoder_Opus::VoiceEncoder_Opus() : m_bitrate(12000), m_samplerate(24000)
{
	m_nEncodeSeq = 0;
	m_nDecodeSeq = 0;
	m_pEncoder = nullptr;
	m_pDecoder = nullptr;
}

VoiceEncoder_Opus::~VoiceEncoder_Opus()
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

bool VoiceEncoder_Opus::Init(int quality)
{
	m_SampleLength = 50;
	m_FrameSize = m_samplerate / m_SampleLength;
	m_MaxFrameSize = m_FrameSize;
	m_nEncodeSeq = 0;
	m_nDecodeSeq = 0;
	m_PacketLossConcealment = true;
	int encSizeBytes = opus_encoder_get_size(MAX_CHANNELS);
	m_pEncoder = (OpusEncoder *)malloc(encSizeBytes);
	if (opus_encoder_init((OpusEncoder *)m_pEncoder, m_samplerate, MAX_CHANNELS, OPUS_APPLICATION_VOIP) != OPUS_OK) {
		free(m_pEncoder);
		m_pEncoder = nullptr;
		return false;
	}

	opus_encoder_ctl((OpusEncoder *)m_pEncoder, OPUS_SET_BITRATE_REQUEST, m_bitrate);
	opus_encoder_ctl((OpusEncoder *)m_pEncoder, OPUS_SET_SIGNAL_REQUEST, OPUS_SIGNAL_VOICE);
	opus_encoder_ctl((OpusEncoder *)m_pEncoder, OPUS_SET_DTX_REQUEST, 1);

	int decSizeBytes = opus_decoder_get_size(MAX_CHANNELS);
	m_pDecoder = (OpusDecoder *)malloc(decSizeBytes);
	if (opus_decoder_init((OpusDecoder *)m_pDecoder, m_samplerate, MAX_CHANNELS) != OPUS_OK) {
		free(m_pDecoder);
		m_pDecoder = nullptr;
		return false;
	}

	return true;
}

void VoiceEncoder_Opus::Release()
{
	delete this;
}

bool VoiceEncoder_Opus::ResetState()
{
	if (m_pEncoder) {
		opus_encoder_ctl(m_pEncoder, OPUS_RESET_STATE);
	}

	if (m_pDecoder) {
		opus_decoder_ctl(m_pDecoder, OPUS_RESET_STATE);
	}

	m_bufOverflowBytes.Clear();
	return true;
}


uint16_t VoiceEncoder_Opus::SampleRate()
{
	return m_samplerate;
}

void VoiceEncoder_Opus::SetFrameSize(int FrameSize)
{
	m_FrameSize = FrameSize;
}


int VoiceEncoder_Opus::Compress(const char* pUncompressedIn, int nSamplesIn, char* pCompressed, int maxCompressedBytes, bool bFinal)
{
	if ((nSamplesIn + GetNumQueuedEncodingSamples()) < m_FrameSize && !bFinal)
	{
		m_bufOverflowBytes.Put(pUncompressedIn, nSamplesIn * BYTES_PER_SAMPLE);
		return 0;
	}

	int nSamples = nSamplesIn;
	int nSamplesRemaining = nSamplesIn % m_FrameSize;
	char* pUncompressed = (char*)pUncompressedIn;

	CUtlBuffer buf;
	if (m_bufOverflowBytes.TellPut() || (nSamplesRemaining && bFinal))
	{
		buf.Put(m_bufOverflowBytes.Base(), m_bufOverflowBytes.TellPut());
		buf.Put(pUncompressedIn, nSamplesIn * BYTES_PER_SAMPLE);
		m_bufOverflowBytes.Clear();

		nSamples = (buf.TellPut() / BYTES_PER_SAMPLE);
		nSamplesRemaining = (buf.TellPut() / BYTES_PER_SAMPLE) % m_FrameSize;

		if (bFinal && nSamplesRemaining)
		{
			// fill samples with silence
			for (int i = m_FrameSize - nSamplesRemaining; i > 0; i--)
			{
				buf.PutShort(0);
			}

			nSamples = (buf.TellPut() / BYTES_PER_SAMPLE);
			nSamplesRemaining = (buf.TellPut() / BYTES_PER_SAMPLE) % m_FrameSize;
		}

		pUncompressed = (char*)buf.Base();
		Assert(!bFinal || nSamplesRemaining == 0);
	}

	char* psRead = pUncompressed;
	char* pWritePos = pCompressed;
	char* pWritePosMax = pCompressed + maxCompressedBytes;

	int nChunks = nSamples - nSamplesRemaining;
	if (nChunks > 0)
	{
		int nRemainingSamples = (nChunks - 1) / m_FrameSize + 1;
		do
		{
			uint16* pWritePayloadSize = (uint16*)pWritePos;
			pWritePos += sizeof(uint16); // leave 2 bytes for the frame size (will be written after encoding)

			if (m_PacketLossConcealment)
			{
				*(uint16*)pWritePos = m_nEncodeSeq++;
				pWritePos += sizeof(uint16);
			}

			int nBytes = ((pWritePosMax - pWritePos) < 0x7FFF) ? (pWritePosMax - pWritePos) : 0x7FFF;
			int nWriteBytes = opus_encode(m_pEncoder, (const opus_int16*)psRead, m_FrameSize, (unsigned char*)pWritePos, nBytes);

			psRead += m_FrameSize;
			pWritePos += nWriteBytes;

			nRemainingSamples--;			
			*pWritePayloadSize = nWriteBytes;
		} while (nRemainingSamples > 0);
	}

	m_bufOverflowBytes.Clear();

	if (nSamplesRemaining)
	{
		Assert((char*)psRead == pUncompressed + ((nSamples - nSamplesRemaining) * sizeof(int16)));
		m_bufOverflowBytes.Put(pUncompressed + ((nSamples - nSamplesRemaining) * sizeof(int16)), nSamplesRemaining * BYTES_PER_SAMPLE);
	}

	if (bFinal)
	{
		ResetState();

		if (pWritePosMax > pWritePos + 2)
		{
			*(uint16*)pWritePos = 0xFFFF;
			pWritePos += sizeof(uint16);
		}

		m_nEncodeSeq = 0;
	}

	return pWritePos - pCompressed;
}

int VoiceEncoder_Opus::Decompress(const char* pCompressed, int compressedBytes, char* pUncompressed, int maxUncompressedBytes)
{
	const char* pReadPos = pCompressed;
	const char* pReadPosMax = &pCompressed[compressedBytes];

	char* pWritePos = pUncompressed;
	char* pWritePosMax = &pUncompressed[maxUncompressedBytes];

	while (pReadPos < pReadPosMax)
	{
		uint16 nPayloadSize = *(uint16*)pReadPos;
		pReadPos += sizeof(uint16);

		if (nPayloadSize == 0xFFFF)
		{
			ResetState();
			m_nDecodeSeq = 0;
			break;
		}

		if (m_PacketLossConcealment)
		{
			uint16 nCurSeq = *(uint16*)pReadPos;
			pReadPos += sizeof(uint16);

			if (nCurSeq < m_nDecodeSeq)
			{
				ResetState();
			}
			else if (nCurSeq != m_nDecodeSeq)
			{
				int nPacketLoss = nCurSeq - m_nDecodeSeq;
				if (nPacketLoss > MAX_PACKET_LOSS) {
					nPacketLoss = MAX_PACKET_LOSS;
				}

				for (int i = 0; i < nPacketLoss; i++)
				{
					if ((pWritePos + m_MaxFrameSize) >= pWritePosMax)
					{
						Assert(false);
						break;
					}

					int nBytes = opus_decode(m_pDecoder, 0, 0, (opus_int16*)pWritePos, m_FrameSize, 0);
					if (nBytes <= 0)
					{
						// raw corrupted
						continue;
					}

					pWritePos += nBytes * BYTES_PER_SAMPLE;
				}
			}

			m_nDecodeSeq = nCurSeq + 1;
		}

		if ((pReadPos + nPayloadSize) > pReadPosMax)
		{
			Assert(false);
			break;
		}

		if ((pWritePos + m_MaxFrameSize) > pWritePosMax)
		{
			Assert(false);
			break;
		}

		memset(pWritePos, 0, m_MaxFrameSize);

		if (nPayloadSize == 0)
		{
			// DTX (discontinued transmission)
			pWritePos += m_MaxFrameSize;
			continue;
		}

		int nBytes = opus_decode(m_pDecoder, (const unsigned char*)pReadPos, nPayloadSize, (opus_int16*)pWritePos, m_FrameSize, 0);
		if (nBytes <= 0)
		{
			// raw corrupted
		}
		else
		{
			pWritePos += nBytes * BYTES_PER_SAMPLE;
		}

		pReadPos += nPayloadSize;
	}

	return (pWritePos - pUncompressed) / BYTES_PER_SAMPLE;
}