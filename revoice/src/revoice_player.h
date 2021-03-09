#pragma once

#include "revoice_shared.h"
#include "VoiceEncoder_Silk.h"
#include "SteamP2PCodec.h"
#include "VoiceEncoder_Speex.h"
#include "voice_codec_frame.h"

constexpr auto SPEAKING_TIMEOUT = std::chrono::milliseconds(200);
class CRevoicePlayer {
private:
	IGameClient* m_Client;
	CodecType m_CodecType;
	CSteamP2PCodec* m_SilkCodec;
	CSteamP2PCodec* m_OpusCodec;
	VoiceCodec_Frame* m_SpeexCodec;
	int m_Protocol;
	int m_VoiceRate;
	int m_RequestId;
	bool m_Connected;
	bool m_HLTV;
	bool m_IsMuted_Old;
	bool m_IsBlocked;
	bool m_IsSpeaking;
	std::chrono::steady_clock::time_point m_VoiceEndTime = std::chrono::steady_clock::now();

public:
	CRevoicePlayer();
	void Update();
	void Initialize(IGameClient* cl);
	void OnConnected();
	void OnDisconnected();

	void SetLastVoiceTime(double time);
	void UpdateVoiceRate(double delta);
	void IncreaseVoiceRate(int dataLength);
	CodecType GetCodecTypeByString(const char* codec);
	const char* GetCodecTypeToString();

	int GetProtocol()  const { return m_Protocol; }
	int GetVoiceRate() const { return m_VoiceRate; }
	int GetRequestId() const { return m_RequestId; }
	bool IsConnected() const { return m_Connected; }
	bool IsHLTV()      const { return m_HLTV; }

	static const char* m_szCodecType[];
	void SetCodecType(CodecType codecType) { m_CodecType = codecType; }

	CodecType GetCodecType() const { return m_CodecType; }
	CSteamP2PCodec* GetSilkCodec() const { return m_SilkCodec; }
	CSteamP2PCodec* GetOpusCodec() const { return m_OpusCodec; }
	VoiceCodec_Frame* GetSpeexCodec() const { return m_SpeexCodec; }
	IGameClient* GetClient() const { return m_Client; }
	void Mute() { m_IsMuted_Old = true; }
	void UnMute() { m_IsMuted_Old = false; }
	bool IsMuted() const { return m_IsMuted_Old; }
	bool IsBlocked() const { return m_IsBlocked; }
	bool IsSpeaking() const { return m_IsSpeaking; }
	bool CheckSpeaking() const { return std::chrono::steady_clock::now() < m_VoiceEndTime; }
	void Block() { m_IsBlocked = true; }
	void Unblock() { m_IsBlocked = false; }
	void Speak()
	{
		m_IsSpeaking = true;
		m_VoiceEndTime = std::chrono::steady_clock::now() + SPEAKING_TIMEOUT;
	}
	void SpeakDone()
	{
		m_IsSpeaking = false;
	}
};

extern CRevoicePlayer g_Players[MAX_PLAYERS];

CRevoicePlayer* GetPlayerByIndex(const size_t clientId);
CRevoicePlayer* GetPlayerByClientPtr(IGameClient* cl);
CRevoicePlayer* GetPlayerByEdict(const edict_t* ed);

void Revoice_Init_Players();
void Revoice_Update_Players(const char* pszNewValue);
void Revoice_Update_Hltv(const char* pszNewValue);
