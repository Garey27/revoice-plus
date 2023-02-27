#pragma once

#include "IVoiceCodec.h"
#include "iframeencoder.h"
#include "speex.h"

class VoiceEncoder_Speex: public IFrameEncoder {
public:
	~VoiceEncoder_Speex();
	VoiceEncoder_Speex();

	virtual bool Init(int quality, int &rawFrameSize, int &encodedFrameSize);
	virtual void Release();
	virtual void EncodeFrame(const char *pUncompressedBytes, char *pCompressed);
	virtual void DecodeFrame(const char *pCompressed, char *pDecompressedBytes);
	virtual bool ResetState();
	virtual uint16_t SampleRate();
	virtual void SetSampleRate(uint16_t sampleRate) {};
	virtual void SetBitRate(float bitrate) {};
	virtual int CodecType();

protected:
	bool InitStates();
	void TermStates();

private:
	int m_Quality;
	void *m_EncoderState;
	void *m_DecoderState;
	SpeexBits m_Bits;
};
