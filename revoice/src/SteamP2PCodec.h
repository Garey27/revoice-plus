#pragma once

#include "revoice_shared.h"
#include "IVoiceCodec.h"

class CSteamP2PCodec: public IVoiceCodec {
public:
	CSteamP2PCodec(IVoiceCodec *backend);

	enum PayLoadType : uint8
	{
		PLT_Silence       = 0, // Number of empty samples, which should be set to NULL.
		PLT_UnknownCodec  = 1,
		PLT_Speex         = 2,
		PLT_Raw           = 3,
		PLT_Silk          = 4,
		PLT_OPUS          = 5,
		PLT_OPUS_PLC      = 6,
		PLT_Unknown       = 10,
		PLT_SamplingRate  = 11
	};

	virtual bool Init(int quality);	
	virtual void Release();
	virtual int Compress(const char *pUncompressedBytes, int nSamples, char *pCompressed, int maxCompressedBytes, bool bFinal);
	virtual int Decompress(const char *pCompressed, int compressedBytes, char *pUncompressed, int maxUncompressedBytes);
	virtual bool ResetState();
	virtual uint16_t SampleRate();
	virtual void SetSampleRate(uint16_t sampleRate)
	{
		m_BackendCodec->SetSampleRate(sampleRate);
	};
	virtual void SetBitRate(float bitRate)
	{
		m_BackendCodec->SetBitRate(bitRate);
	};
	virtual int CodecType()
	{
		return m_BackendCodec->CodecType();
	};
	void SetClient(IGameClient *client);
	void SetSteamid(uint64_t steamid)
	{
		m_Steamid = steamid;
	}
	IVoiceCodec *GetCodec() const { return m_BackendCodec; }

private:
	int StreamDecode(const char *pCompressed, int compressedBytes, char *pUncompressed, int maxUncompressedBytes) const;
	int StreamEncode(const char *pUncompressedBytes, int nSamples, char *pCompressed, int maxCompressedBytes, bool bFinal) const;

private:	
	uint64_t m_Steamid;
	IGameClient *m_Client;
	IVoiceCodec *m_BackendCodec;
};
