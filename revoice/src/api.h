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


struct receiver_state
{
	std::unique_ptr<VoiceEncoder_Opus> SilkCodec;
	std::unique_ptr<VoiceEncoder_Speex> SpeexCodec;
	std::unique_ptr<CSteamP2PCodec> SteamCodec;
	std::unique_ptr<VoiceCodec_Frame> FrameCodec;
	play_state state;
	std::chrono::high_resolution_clock::time_point nextSend;
	size_t current_pos;
	void seek(size_t pos, size_t totalLength, seekParam seekType)
	{
		switch (seekType)
		{
		case seekHead:
			if (pos > 0)
				current_pos = pos;
			else
				current_pos = 0;

			break;
		case seekCurr:
			if (pos > 0)
				current_pos += pos;
			else
				current_pos -= pos;

			break;
		case seekTail:
			if (pos > 0)
				current_pos = totalLength;
			else
				current_pos -= pos;

			break;
		}
	}
};
class audio_wave_play
{
public:
	uint8_t senderClientIndex;
	std::unordered_map<uint8_t, receiver_state> receivers;
	bool auto_clear = false;
	bool check_clear = true;
	std::unique_ptr<audio_wave> wave8k;
	std::unique_ptr<audio_wave> wave16k;
};

class RevoiceAPI final : public IRevoiceApi {
public:
	virtual ~RevoiceAPI() {}

	size_t MajorVersion() override;
	size_t MinorVersion() override;

	bool IsClientSpeaking(size_t clientIndex) override;

	IEvent<size_t>& OnClientStartSpeak() override;
	IEvent<size_t>& OnClientStopSpeak() override;

	IEvent<size_t, size_t, uint16_t, uint8_t*, size_t*>& OnDecompress() override;
	IEvent<uint32_t, uint32_t>& OnSoundComplete() override;

	void MuteClient(size_t clientIndex, size_t receiverIndex = 0) override;
	void UnmuteClient(size_t clientIndex, size_t receiverIndex = 0) override;
	bool IsClientMuted(size_t clientIndex, size_t receiverIndex = 0) override;

	uint32_t SoundAdd(std::shared_ptr<audio_wave> wave8k, std::shared_ptr<audio_wave> wave16k) override;
	void SoundPush(uint32_t wave_id, std::shared_ptr<audio_wave> wave8k, std::shared_ptr<audio_wave> wave16k) override;
	void SoundDelete(uint32_t wave_id) override;
	void SoundAutoClear(uint32_t wave_id) override;
	void SoundPlay(size_t senderClientIndex, size_t receiverClientIndex, uint32_t wave_id) override;
	void SoundPause(uint8_t receiverClientIndex, uint32_t wave_id) override;
	void SoundStop(uint8_t receiverClientIndex, uint32_t wave_id) override;
	void SoundSeek(uint8_t receiverClientIndex, uint32_t wave_id, float seek, seekParam seekParam) override;
	float SoundTell(uint8_t receiverClientIndex, uint32_t wave_id) override;
	float SoundLength(uint32_t wave_id) override;

	void BlockClient(size_t clientIndex) override;
	void UnblockClient(size_t clientIndex) override;
	bool IsClientBlocked(size_t clientIndex) override;
};

extern Event<size_t> g_OnREClientStartSpeak;
extern Event<size_t> g_OnREClientStopSpeak;

extern Event<size_t, size_t, uint16_t, uint8_t*, size_t*> g_OnDecompress;
extern Event<uint32_t, uint32_t> g_OnSoundComplete;
extern VoiceTranscoderAPI g_voiceTranscoderAPI;
extern RevoiceAPI g_revoiceAPI;
extern std::unordered_map<uint8_t, std::unordered_map<uint8_t, bool>> g_mute_map;
extern std::unordered_map<uint32_t, audio_wave_play> g_audio_waves;
const size_t SVC_STUFFTEXT = 9;