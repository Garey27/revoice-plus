#pragma once

#include "IVoiceCodec.h"
#include "iframeencoder.h"

class IGameClient;
class VoiceCodec_Frame: public IVoiceCodec {
public:
	virtual ~VoiceCodec_Frame();
	VoiceCodec_Frame(IFrameEncoder *pEncoder);

	virtual bool Init(int quality);
	virtual void Release();
	virtual int Compress(const char *pUncompressedBytes, int nSamples, char *pCompressed, int maxCompressedBytes, bool bFinal);
	virtual int Decompress(const char *pCompressed, int compressedBytes, char *pUncompressed, int maxUncompressedBytes);
	virtual bool ResetState();
	virtual uint16_t SampleRate();

	virtual void SetSampleRate(uint16_t sampleRate)
	{
		m_pFrameEncoder->SetSampleRate(sampleRate);
	};
	virtual void SetBitRate(float bitrate)
	{
		m_pFrameEncoder->SetBitRate(bitrate);
	};
	virtual int CodecType()
	{
		return m_pFrameEncoder->CodecType();
	};
	void SetClient(IGameClient *client) { m_Client = client; }

protected:
	enum { MAX_FRAMEBUFFER_SAMPLES = 1024 };

	short int m_EncodeBuffer[MAX_FRAMEBUFFER_SAMPLES];
	int m_nEncodeBufferSamples;

	IFrameEncoder *m_pFrameEncoder;
	int m_nRawBytes;
	int m_nRawSamples;
	int m_nEncodedBytes;

public:
	IGameClient *m_Client;
};
