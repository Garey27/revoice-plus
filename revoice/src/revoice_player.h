#pragma once

#include <vector>

#include "revoice_shared.h"
#include "VoiceEncoder_Silk.h"
#include "SteamP2PCodec.h"
#include "VoiceEncoder_Speex.h"
#include "voice_codec_frame.h"

constexpr auto SPEAKING_TIMEOUT = std::chrono::milliseconds(200);

// Порог автоперевода на vct_speex при серии провалов декодирования.
// Почему 8: кадр голоса — 20 мс, значит 8 подряд нераспознанных кадров это примерно
// 0,16 с чистой речи; с учётом пауз между пакетами это порядка 0,5 с — достаточно,
// чтобы отличить неверно определённый кодек от случайного битого пакета, и мало,
// чтобы игрок успел заметить, что его не слышно.
constexpr int REV_DECODE_FAIL_LIMIT = 8;

class CRevoicePlayer {
private:
	IGameClient* m_Client;
	// Буферы голосового пакета. Живут между пакетами: создавать их на каждый пакет дорого.
	std::vector<char> m_recvBuff;
	std::vector<char> m_speexBuff;
	std::vector<char> m_steamBuff;
	// Флаг занятости: защита от повторного входа в разбор пакета для того же игрока.
	bool m_VoiceBusy = false;
	// Счётчик подряд идущих провалов декодирования. Если у игрока объявлен кодек,
	// которым его пакеты не разбираются, он иначе остаётся немым до переподключения.
	int m_iDecodeFails = 0;
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
	int m_iCheckingState;
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

	// Доступ к переиспользуемым буферам
	std::vector<char>& RecvBuff()  { return m_recvBuff; }
	std::vector<char>& SpeexBuff() { return m_speexBuff; }
	std::vector<char>& SteamBuff() { return m_steamBuff; }

	// Флаг занятости (RAII-гард реентерабельности)
	bool IsVoiceBusy() const { return m_VoiceBusy; }
	bool& VoiceBusyRef() { return m_VoiceBusy; }

	// Счётчик подряд идущих провалов декодирования (см. REV_DECODE_FAIL_LIMIT)
	int  GetDecodeFails() const { return m_iDecodeFails; }
	int  OnDecodeFail() { return ++m_iDecodeFails; }
	void ResetDecodeFails() { m_iDecodeFails = 0; }
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

	int GetCheckingState()
	{
		return m_iCheckingState;
	}

	void SetCheckingState(int check)
	{
		m_iCheckingState = check;
	}
};

extern CRevoicePlayer g_Players[MAX_PLAYERS];

CRevoicePlayer* GetPlayerByIndex(const size_t clientId);
CRevoicePlayer* GetPlayerByClientPtr(IGameClient* cl);
CRevoicePlayer* GetPlayerByEdict(const edict_t* ed);

void Revoice_Init_Players();
void Revoice_Update_Bitrate(const char* pszNewValue);
void Revoice_Update_Players(const char* pszNewValue);
void Revoice_Update_Hltv(const char* pszNewValue);
