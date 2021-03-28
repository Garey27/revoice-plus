#include <random>

#include "precompiled.h"

Event<size_t> g_OnClientStartSpeak;
Event<size_t> g_OnClientStopSpeak;

Event<size_t, uint16_t, uint8_t*, size_t*> g_OnDecompress;
Event<uint32_t> g_OnSoundComplete;

VoiceTranscoderAPI g_voiceTranscoderAPI;
RevoiceAPI g_revoiceAPI;

std::unordered_map<uint8_t, std::unordered_map<uint8_t, bool>> g_mute_map;
uint32_t g_numAudioWaves = 1;
std::unordered_map<uint32_t, audio_wave_play> g_audio_waves;


std::random_device rd;
std::mt19937_64 gen(rd());
std::uniform_int_distribution<uint32_t> dis;

size_t VoiceTranscoderAPI::MajorVersion() {
	return VOICETRANSCODER_API_VERSION_MAJOR;
}

size_t VoiceTranscoderAPI::MinorVersion() {
	return VOICETRANSCODER_API_VERSION_MINOR;
}

bool VoiceTranscoderAPI::IsClientSpeaking(size_t clientIndex) {
	CRevoicePlayer *pClient = GetPlayerByIndex(clientIndex);

	if (!pClient) {
		return false;
	}
	return pClient->IsSpeaking();
}

IEvent<size_t> &VoiceTranscoderAPI::OnClientStartSpeak() {
	return g_OnClientStartSpeak;
}

IEvent<size_t> &VoiceTranscoderAPI::OnClientStopSpeak() {
	return g_OnClientStopSpeak;
}

void VoiceTranscoderAPI::MuteClient(size_t clientIndex) {
	CRevoicePlayer *pClient = GetPlayerByIndex(clientIndex);

	if (!pClient) {
		return;
	}
	pClient->Mute();
}

void VoiceTranscoderAPI::UnmuteClient(size_t clientIndex) {
	CRevoicePlayer *pClient = GetPlayerByIndex(clientIndex);

	if (!pClient) {
		return;
	}
	pClient->UnMute();
}

bool VoiceTranscoderAPI::IsClientMuted(size_t clientIndex) {
	CRevoicePlayer *pClient = GetPlayerByIndex(clientIndex);

	if (!pClient) {
		return false;
	}

	return pClient->IsMuted();
}

void VoiceTranscoderAPI::PlaySound(size_t receiverClientIndex, const char *soundFilePath) {
	CRevoicePlayer *pClient = GetPlayerByIndex(receiverClientIndex);

	if (!pClient)
    {
		return;
	}
}

void VoiceTranscoderAPI::BlockClient(size_t clientIndex)
{
	CRevoicePlayer *pClient = GetPlayerByIndex(clientIndex);

	if (!pClient)
    {
		return;
	}
	pClient->Block();

}

void VoiceTranscoderAPI::UnblockClient(size_t clientIndex) {
	CRevoicePlayer *pClient = GetPlayerByIndex(clientIndex);

	if (!pClient)
    {
		return;
	}
	pClient->Unblock();
}

bool VoiceTranscoderAPI::IsClientBlocked(size_t clientIndex) {
	CRevoicePlayer *pClient = GetPlayerByIndex(clientIndex);

	if (!pClient)
    {
		return false;
	}

	return pClient->IsBlocked();
}

size_t RevoiceAPI::MajorVersion()
{
	return REVOICE_API_VERSION_MAJOR;	
};
size_t RevoiceAPI::MinorVersion()
{
	return REVOICE_API_VERSION_MINOR;
};

bool RevoiceAPI::IsClientSpeaking(size_t clientIndex)
{
	CRevoicePlayer* pClient = GetPlayerByIndex(clientIndex);

	if (!pClient) {
		return false;
	}

	return pClient->IsSpeaking();
};

IEvent<size_t>& RevoiceAPI::OnClientStartSpeak()
{
	return g_OnClientStartSpeak;
};
IEvent<size_t>& RevoiceAPI::OnClientStopSpeak()
{
	return g_OnClientStopSpeak;
};

IEvent<size_t, uint16_t, uint8_t*, size_t*>& RevoiceAPI::OnDecompress()
{
	return g_OnDecompress;
};
IEvent<uint32_t>& RevoiceAPI::OnSoundComplete()
{
	return g_OnSoundComplete;
};

void RevoiceAPI::MuteClient(size_t clientIndex, size_t receiverIndex)
{
	CRevoicePlayer* pClient = GetPlayerByIndex(clientIndex);

	if (!pClient) {
		return;
	}
	g_mute_map[clientIndex - 1][receiverIndex - 1] = true;
};
void RevoiceAPI::UnmuteClient(size_t clientIndex, size_t receiverIndex)
{
	CRevoicePlayer* pClient = GetPlayerByIndex(clientIndex);
	
	if (!pClient) {
		return;
	}
	g_mute_map[clientIndex - 1][receiverIndex - 1] = false;
};
bool RevoiceAPI::IsClientMuted(size_t clientIndex, size_t receiverIndex)
{
	CRevoicePlayer* pClient = GetPlayerByIndex(clientIndex);

	if (!pClient) {
		return false;
	}
	if (g_mute_map.find(clientIndex - 1) == g_mute_map.end())
		return false;

	if (g_mute_map[clientIndex - 1].find(receiverIndex - 1) == g_mute_map[receiverIndex - 1].end())
		return false;
	
	return g_mute_map[clientIndex - 1][receiverIndex - 1];
};

uint32_t RevoiceAPI::SoundAdd(std::shared_ptr<audio_wave> wave8k, std::shared_ptr<audio_wave> wave16k)
{
	g_audio_waves[g_numAudioWaves].wave16k = wave16k;
	g_audio_waves[g_numAudioWaves].wave8k = wave8k;
	g_audio_waves[g_numAudioWaves].state		= audio_wave_play::PLAY_STOP;
	g_audio_waves[g_numAudioWaves].SpeexCodec = std::make_unique<VoiceEncoder_Speex>();
	g_audio_waves[g_numAudioWaves].SilkCodec	= std::make_unique<VoiceEncoder_Silk>();
	g_audio_waves[g_numAudioWaves].SteamCodec	= std::make_unique<CSteamP2PCodec>(g_audio_waves[g_numAudioWaves].SilkCodec.get());
	g_audio_waves[g_numAudioWaves].FrameCodec = std::make_unique<VoiceCodec_Frame>(g_audio_waves[g_numAudioWaves].SpeexCodec.get());
	g_audio_waves[g_numAudioWaves].SteamCodec->Init(5);	
	g_audio_waves[g_numAudioWaves].SteamCodec->SetSteamid(76561197960265728ull + dis(gen));
	g_audio_waves[g_numAudioWaves].SteamCodec->SetSampleRate(wave16k->sample_rate());
	g_audio_waves[g_numAudioWaves].FrameCodec->Init(5);
	
	return g_numAudioWaves++;
};

void RevoiceAPI::SoundDelete(uint32_t wave_id)
{
	g_audio_waves.erase(wave_id);
};

void RevoiceAPI::SoundPlay(size_t senderClientIndex, size_t receiverClientIndex, uint32_t wave_id)
{
	auto found = g_audio_waves.find(wave_id);
	if(found != g_audio_waves.end())
	{
		if(senderClientIndex == 0)
			found->second.senderClientIndex = 32;
		else
			found->second.senderClientIndex = senderClientIndex - 1;
		found->second.receivers.insert(receiverClientIndex - 1);
		found->second.state = audio_wave_play::PLAY_PLAY;
	}
};
void RevoiceAPI::SoundPause(uint32_t wave_id)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end())
	{
		found->second.state = audio_wave_play::PLAY_PAUSE;
	}
};
void RevoiceAPI::SoundStop(uint32_t wave_id)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end())
	{
		found->second.state = audio_wave_play::PLAY_STOP;
		found->second.flPlayPos8k = 0.f;
		found->second.flPlayPos16k = 0.f;
	}
};
void RevoiceAPI::SoundSeek(uint32_t wave_id, float seek, audio_wave::seekParam seekParam)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end())
	{
		found->second.wave8k->seek(seek, seekParam);
		found->second.flPlayPos8k = found->second.wave8k->tell();
		found->second.wave16k->seek(seek, seekParam);
		found->second.flPlayPos16k = found->second.wave16k->tell();
	}
};
float RevoiceAPI::SoundTell(uint32_t wave_id)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end())
	{
		return found->second.flPlayPos16k;
	}
	return -1.0f;
};

float RevoiceAPI::SoundLength(uint32_t wave_id)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end())
	{
		return found->second.wave16k->length();
	}
	return -1.0f;
};

void RevoiceAPI::BlockClient(size_t clientIndex)
{
	CRevoicePlayer* pClient = GetPlayerByIndex(clientIndex);

	if (!pClient)
	{
		return;
	}
	pClient->Block();
};
void RevoiceAPI::UnblockClient(size_t clientIndex)
{
	CRevoicePlayer* pClient = GetPlayerByIndex(clientIndex);

	if (!pClient)
	{
		return;
	}
	pClient->Unblock();
};

bool RevoiceAPI::IsClientBlocked(size_t clientIndex)
{
	CRevoicePlayer* pClient = GetPlayerByIndex(clientIndex);

	if (!pClient)
	{
		return false;
	}

	return pClient->IsBlocked();
};


void RevoiceAPI::SoundPush(uint32_t wave_id, std::shared_ptr<audio_wave> wave8k , std::shared_ptr<audio_wave> wave16k)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end())
	{
		uint16_t* data;
		auto n_frames = wave16k->get_frames(&data);
		found->second.wave16k->push_frames(data, n_frames);
		found->second.flPlayPos16k =  found->second.wave16k->tell();
		n_frames = wave8k->get_frames(&data);
		found->second.wave8k->push_frames(data, n_frames);
		found->second.flPlayPos8k = found->second.wave8k->tell();
	}

}
void RevoiceAPI::SoundAutoDelete(uint32_t wave_id)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end())
	{
		found->second.auto_delete = true;
	}
	
}