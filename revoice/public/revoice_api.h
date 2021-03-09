#pragma once
#include <memory>
#include <vector>
const char VOICETRANSCODER_VERSION[] = "2017RC5";

const size_t VOICETRANSCODER_API_VERSION_MAJOR = 3;
const size_t VOICETRANSCODER_API_VERSION_MINOR = 1;

const size_t REVOICE_API_VERSION_MAJOR = 1;
const size_t REVOICE_API_VERSION_MINOR = 0;
template <typename ...T>
class IEvent {
public:
	virtual ~IEvent() {}

	typedef void (*handler_t)(T...);

	virtual void operator+=(handler_t callback) = 0;
	virtual void operator-=(handler_t callback) = 0;
};

class audio_wave
{
public:
	enum seekParam
	{
		seekHead,
		seekCurr,
		seekTail
	};
private:
	unsigned channels;
	unsigned sample_rate_;
	size_t current_pos;
	float wav_length;
	std::vector<uint16_t> data_;
public:
	
	audio_wave(uint16_t* data, size_t samples_count, unsigned  channels, unsigned sampleRate)
		: data_(data, data + samples_count), channels(channels), sample_rate_(sampleRate)
	{
		
		wav_length = static_cast<float>(samples_count) / sampleRate / channels;
		current_pos = 0;
	}
	void seek(size_t pos, seekParam seekType)
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
				current_pos = wav_length * sample_rate_;
			else
				current_pos -= pos;

			break;
		}
	}

	float tell() const
	{
		return (float)current_pos / sample_rate_ / channels;
	}

	unsigned sample_rate() const
	{
		return sample_rate_;
	}

	unsigned num_channels() const
	{
		return channels;
	}
	float length() const
	{
		return wav_length;
	}
	uint8_t* get_samples(uint32_t length, size_t* n_samples)
	{
		*n_samples = length * channels;
		if (current_pos + *n_samples > data_.size())
		{
			*n_samples = (data_.size() - current_pos);
		}

		return (uint8_t*)(data_.data() + current_pos);
	}
	size_t get_frames(uint16_t** data)
	{
		*data = data_.data();
		return data_.size();
	}
	void push_frames(uint16_t* data, size_t samples_count)
	{
		data_.insert(data_.end(), data, data + samples_count);
		wav_length = static_cast<float>(data_.size()) / sample_rate_ / channels;
	}
	void clear_frames(size_t samples_count)
	{
		data_.erase(data_.begin(), data_.begin() + samples_count);
		wav_length = static_cast<float>(data_.size()) / sample_rate_ / channels;
	}
};

class IRevoiceApi
{
public:
	virtual ~IRevoiceApi() {}

	virtual size_t MajorVersion() = 0;
	virtual size_t MinorVersion() = 0;

	virtual bool IsClientSpeaking(size_t clientIndex) = 0;

	virtual IEvent<size_t>& OnClientStartSpeak() = 0;
	virtual IEvent<size_t>& OnClientStopSpeak() = 0;
	virtual IEvent<size_t, uint16_t, uint8_t*, size_t*>& OnDecompress() = 0;
	virtual IEvent<uint32_t>& OnSoundComplete() = 0;

	virtual void MuteClient(size_t clientIndex, size_t receiverIndex = 0) = 0;
	virtual void UnmuteClient(size_t clientIndex, size_t receiverIndex = 0) = 0;
	virtual bool IsClientMuted(size_t clientIndex, size_t receiverIndex = 0) = 0;
	virtual uint32_t SoundAdd(std::shared_ptr<audio_wave> wave8k, std::shared_ptr<audio_wave> wave16k) = 0;
	virtual void SoundPush(uint32_t wave_id, std::shared_ptr<audio_wave> wave8k, std::shared_ptr<audio_wave> wave16k) = 0;
	virtual void SoundDelete(uint32_t wave_id) = 0;
	virtual void SoundAutoDelete(uint32_t wave_id) = 0;
	virtual void SoundPlay(size_t senderClientIndex, size_t receiverClientIndex, uint32_t wave_id) = 0;
	virtual void SoundPause(uint32_t wave_id) = 0;
	virtual void SoundStop(uint32_t wave_id) = 0;
	virtual void SoundSeek(uint32_t wave_id, float seek, audio_wave::seekParam seekParam) = 0;
	virtual float SoundTell(uint32_t wave_id) = 0;
	virtual float SoundLength(uint32_t wave_id) = 0;

	virtual void BlockClient(size_t clientIndex) = 0;
	virtual void UnblockClient(size_t clientIndex) = 0;
	virtual bool IsClientBlocked(size_t clientIndex) = 0;
};



class IVoiceTranscoderAPI {
public:
	virtual ~IVoiceTranscoderAPI() {}

	virtual size_t MajorVersion() = 0;
	virtual size_t MinorVersion() = 0;

	virtual bool IsClientSpeaking(size_t clientIndex) = 0;

	virtual IEvent<size_t>& OnClientStartSpeak() = 0;
	virtual IEvent<size_t>& OnClientStopSpeak() = 0;

	virtual void MuteClient(size_t clientIndex) = 0;
	virtual void UnmuteClient(size_t clientIndex) = 0;
	virtual bool IsClientMuted(size_t clientIndex) = 0;

	virtual void PlaySound(size_t receiverClientIndex, const char* soundFilePath) = 0;

	virtual void BlockClient(size_t clientIndex) = 0;
	virtual void UnblockClient(size_t clientIndex) = 0;
	virtual bool IsClientBlocked(size_t clientIndex) = 0;
};
