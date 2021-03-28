#pragma once

#include "IVoiceCodec.h"
#include "utlbuffer.h"
#include "opus.h"


class VoiceEncoder_Opus: public IVoiceCodec {
private:
	OpusEncoder *m_pEncoder;
	OpusDecoder *m_pDecoder;
	CUtlBuffer m_bufOverflowBytes;

	int m_samplerate;
	int m_bitrate;

	uint16 m_nEncodeSeq;
	uint16 m_nDecodeSeq;

	bool m_PacketLossConcealment;
	int m_SampleLength;
	int m_FrameSize;
	int m_MaxFrameSize;

public:
	VoiceEncoder_Opus();

	virtual ~VoiceEncoder_Opus();

	virtual bool Init(int quality);
	virtual void Release();
	virtual bool ResetState();
	virtual int Compress(const char *pUncompressedBytes, int nSamplesIn, char *pCompressed, int maxCompressedBytes, bool bFinal);
	virtual int Decompress(const char *pCompressed, int compressedBytes, char *pUncompressed, int maxUncompressedBytes);
	virtual uint16_t SampleRate();
	virtual void SetFrameSize(int frame_size);

	int GetNumQueuedEncodingSamples() const { return m_bufOverflowBytes.TellPut() / BYTES_PER_SAMPLE; }
};
