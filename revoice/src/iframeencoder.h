#pragma once

class IFrameEncoder {
protected:
	virtual ~IFrameEncoder() {};

public:
	// quality is in [0..10]
	virtual bool Init(int quality, int &rawFrameSize, int &encodedFrameSize) = 0;
	virtual void Release() = 0;
	virtual void EncodeFrame(const char *pUncompressedBytes, char *pCompressed) = 0;
	virtual void DecodeFrame(const char *pCompressed, char *pDecompressedBytes) = 0;
	virtual bool ResetState() = 0;
	virtual uint16_t SampleRate() = 0;
	virtual void SetSampleRate(uint16_t sampleRate) = 0;
	virtual void SetBitRate(float bitrate) = 0;
	virtual int CodecType() = 0;
};
