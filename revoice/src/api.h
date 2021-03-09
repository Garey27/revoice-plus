#pragma once

const size_t MAX_CALLBACK_COUNT = 64;
template <typename ...T_ARGS>
class Event : public IEvent<T_ARGS...> {
	typedef void (* callback_t)(T_ARGS...);

	callback_t m_callbacks[MAX_CALLBACK_COUNT];
	size_t m_callbackCount;
public:
	Event() {
		m_callbackCount = 0;
	}
	virtual ~Event() {}

	void operator()(T_ARGS... args) {
		for (size_t i = 0; i < m_callbackCount; i++) {
			m_callbacks[i](args...);
		}
	}

	virtual void operator+=(callback_t callback) override final {
		if (m_callbackCount == MAX_CALLBACK_COUNT) {
			return;
		}

		m_callbacks[m_callbackCount++] = callback;
	};
	virtual void operator-=(callback_t callback) override final {
		for (size_t i = 0; i < m_callbackCount; i++) {
			if (m_callbacks[i] == callback) {
				memmove(&m_callbacks[i], &m_callbacks[i+1], (m_callbackCount - i - 1) * sizeof(callback_t));
				m_callbackCount--;

				return;
			}
		}
	};
};

class VoiceTranscoderAPI final : public IVoiceTranscoderAPI {
public:
	virtual ~VoiceTranscoderAPI() {}

	size_t MajorVersion() override;
	size_t MinorVersion() override;

	bool IsClientSpeaking(size_t clientIndex) override;

	IEvent<size_t>& OnClientStartSpeak() override;
	IEvent<size_t>& OnClientStopSpeak() override;

	void MuteClient(size_t clientIndex) override;
	void UnmuteClient(size_t clientIndex) override;
	bool IsClientMuted(size_t clientIndex) override;

	void PlaySound(size_t receiverClientIndex, const char *soundFilePath) override;

	void BlockClient(size_t clientIndex) override;
	void UnblockClient(size_t clientIndex) override;
	bool IsClientBlocked(size_t clientIndex) override;
};

extern Event<size_t> g_OnClientStartSpeak;
extern Event<size_t> g_OnClientStopSpeak;

class audio_wave_play
{
public:
	enum play_state
	{
		PLAY_STOP,
		PLAY_PAUSE,
		PLAY_PLAY,
		PLAY_DONE
	};
	play_state state;
	uint32_t senderClientIndex;
	std::unordered_set<size_t> receivers;
	bool auto_delete = false;
	float flPlayPos8k;
	float flPlayPos16k;
	std::unique_ptr<VoiceEncoder_Silk> SilkCodec;
	std::unique_ptr<VoiceEncoder_Speex> SpeexCodec;
	std::unique_ptr<CSteamP2PCodec> SteamCodec;
	std::unique_ptr<VoiceCodec_Frame> FrameCodec;
	std::shared_ptr<audio_wave> wave8k;
	std::shared_ptr<audio_wave> wave16k;
	std::chrono::high_resolution_clock::time_point nextSend8k;
	std::chrono::high_resolution_clock::time_point nextSend16k;
};

class RevoiceAPI final : public IRevoiceApi {
public:
	virtual ~RevoiceAPI() {}

	size_t MajorVersion() override;
	size_t MinorVersion() override;

	bool IsClientSpeaking(size_t clientIndex) override;

	IEvent<size_t>& OnClientStartSpeak() override;
	IEvent<size_t>& OnClientStopSpeak() override;

	IEvent<size_t, uint16_t, uint8_t*, size_t*>& OnDecompress() override;
	IEvent<uint32_t>& OnSoundComplete() override;

	void MuteClient(size_t clientIndex, size_t receiverIndex = 0) override;
	void UnmuteClient(size_t clientIndex, size_t receiverIndex = 0) override;
	bool IsClientMuted(size_t clientIndex, size_t receiverIndex = 0) override;

	uint32_t SoundAdd(std::shared_ptr<audio_wave> wave8k, std::shared_ptr<audio_wave> wave16k) override;
	void SoundPush(uint32_t wave_id, std::shared_ptr<audio_wave> wave8k, std::shared_ptr<audio_wave> wave16k) override;
	void SoundDelete(uint32_t wave_id) override;
	void SoundAutoDelete(uint32_t wave_id) override;
	void SoundPlay(size_t senderClientIndex, size_t receiverClientIndex, uint32_t wave_id) override;
	void SoundPause(uint32_t wave_id) override;
	void SoundStop(uint32_t wave_id) override;
	void SoundSeek(uint32_t wave_id, float seek, audio_wave::seekParam seekParam) override;
	float SoundTell(uint32_t wave_id) override;
	float SoundLength(uint32_t wave_id) override;

	void BlockClient(size_t clientIndex) override;
	void UnblockClient(size_t clientIndex) override;
	bool IsClientBlocked(size_t clientIndex) override;
};

extern Event<size_t> g_OnREClientStartSpeak;
extern Event<size_t> g_OnREClientStopSpeak;

extern Event<size_t, uint16_t, uint8_t*, size_t*> g_OnDecompress;
extern Event<uint32_t> g_OnSoundComplete;
extern VoiceTranscoderAPI g_voiceTranscoderAPI;
extern RevoiceAPI g_revoiceAPI;
extern std::unordered_map<uint8_t, std::unordered_map<uint8_t, bool>> g_mute_map;
extern std::unordered_map<uint32_t, audio_wave_play> g_audio_waves;