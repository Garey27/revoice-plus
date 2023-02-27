#include <random>

#include "precompiled.h"

Event<size_t> g_OnClientStartSpeak;
Event<size_t> g_OnClientStopSpeak;

Event<size_t, uint16_t, uint8_t*, size_t*> g_OnDecompress;
Event<uint32_t, uint32_t> g_OnSoundComplete;

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
IEvent<uint32_t, uint32_t>& RevoiceAPI::OnSoundComplete()
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
	g_audio_waves[g_numAudioWaves].wave16k = std::make_unique< audio_wave>(*wave16k);
	g_audio_waves[g_numAudioWaves].wave8k = std::make_unique< audio_wave>(*wave8k);
	
	return g_numAudioWaves++;
};

void RevoiceAPI::SoundDelete(uint32_t wave_id)
{
	g_audio_waves.erase(wave_id);
};

void RevoiceAPI::SoundPlay(size_t senderClientIndex, size_t receiverClientIndex, uint32_t wave_id)
{
	auto found_sound = g_audio_waves.find(wave_id);
	if(found_sound != g_audio_waves.end())
	{
		if (senderClientIndex == 0)
		{
			found_sound->second.senderClientIndex = 32;
		}
		else
			found_sound->second.senderClientIndex = senderClientIndex - 1;

		if (receiverClientIndex)
		{
			found_sound->second.receivers[receiverClientIndex - 1].nextSend = std::chrono::high_resolution_clock::now();
			found_sound->second.receivers[receiverClientIndex - 1].state = PLAY_PLAY;
			if (!found_sound->second.receivers[receiverClientIndex - 1].SpeexCodec)
			{
				found_sound->second.receivers[receiverClientIndex - 1].SpeexCodec = std::make_unique<VoiceEncoder_Speex>();
				found_sound->second.receivers[receiverClientIndex - 1].SilkCodec = std::make_unique<VoiceEncoder_Opus>();
				found_sound->second.receivers[receiverClientIndex - 1].SteamCodec = std::make_unique<CSteamP2PCodec>(found_sound->second.receivers[receiverClientIndex - 1].SilkCodec.get());
				found_sound->second.receivers[receiverClientIndex - 1].FrameCodec = std::make_unique<VoiceCodec_Frame>(found_sound->second.receivers[receiverClientIndex - 1].SpeexCodec.get());
				found_sound->second.receivers[receiverClientIndex - 1].SteamCodec->Init(5);
				found_sound->second.receivers[receiverClientIndex - 1].SteamCodec->SetSteamid(76561197960265728ull + dis(gen));
				found_sound->second.receivers[receiverClientIndex - 1].SteamCodec->SetSampleRate(found_sound->second.wave16k->sample_rate());
				found_sound->second.receivers[receiverClientIndex - 1].FrameCodec->Init(5);
			}
		}
		else
		{
			for (int i = 0, maxclients = g_RehldsSvs->GetMaxClients(); i < maxclients; i++)
			{
				CRevoicePlayer* pClient = &g_Players[i];
				if (!pClient || !pClient->GetClient()->IsActive() || !pClient->GetClient()->IsConnected())
				{
					found_sound->second.receivers.erase(i);
					continue;
				}
				found_sound->second.receivers[i].nextSend = std::chrono::high_resolution_clock::now();
				found_sound->second.receivers[i].state = PLAY_PLAY;
				if (!found_sound->second.receivers[i].SpeexCodec)
				{
					found_sound->second.receivers[i].SpeexCodec = std::make_unique<VoiceEncoder_Speex>();
					found_sound->second.receivers[i].SilkCodec = std::make_unique<VoiceEncoder_Opus>();
					found_sound->second.receivers[i].SteamCodec = std::make_unique<CSteamP2PCodec>(found_sound->second.receivers[i].SilkCodec.get());
					found_sound->second.receivers[i].FrameCodec = std::make_unique<VoiceCodec_Frame>(found_sound->second.receivers[i].SpeexCodec.get());
					found_sound->second.receivers[i].FrameCodec = std::make_unique<VoiceCodec_Frame>(found_sound->second.receivers[i].SpeexCodec.get());
					found_sound->second.receivers[i].SteamCodec->Init(5);
					found_sound->second.receivers[i].SteamCodec->SetSteamid(76561197960265728ull + dis(gen));
					found_sound->second.receivers[i].SteamCodec->SetSampleRate(found_sound->second.wave16k->sample_rate());												
					found_sound->second.receivers[i].SteamCodec->SetBitRate(g_pcv_rev_bitrate->value);
					found_sound->second.receivers[i].FrameCodec->Init(5);
				}
			}
		}
	}
};
void RevoiceAPI::SoundPause(uint8_t receiverClientIndex, uint32_t wave_id)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end() && found->second.receivers.find(receiverClientIndex - 1) != found->second.receivers.end())
	{
		found->second.receivers[receiverClientIndex - 1].state = PLAY_PAUSE;
	}
};
void RevoiceAPI::SoundStop(uint8_t receiverClientIndex, uint32_t wave_id)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end() && found->second.receivers.find(receiverClientIndex - 1) != found->second.receivers.end())
	{
		found->second.receivers[receiverClientIndex - 1].state = PLAY_STOP;
		found->second.receivers[receiverClientIndex - 1].current_pos = 0;
	}
};
void RevoiceAPI::SoundSeek(uint8_t receiverClientIndex, uint32_t wave_id, float seek, seekParam seekParam)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end() && found->second.receivers.find(receiverClientIndex - 1) != found->second.receivers.end())
	{
		CRevoicePlayer* pClient = GetPlayerByIndex(receiverClientIndex);
		if (!pClient)
			return;

		auto& wave = (pClient->GetCodecType() == vct_speex) ? found->second.wave8k : found->second.wave16k;
		auto totalLen = wave->length();
		size_t pos = wave->num_frames() * std::min(seek, totalLen) / totalLen;

		found->second.receivers[receiverClientIndex - 1].seek(pos, wave->num_frames(), seekParam);
	}
};
float RevoiceAPI::SoundTell(uint8_t receiverClientIndex, uint32_t wave_id)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end() && found->second.receivers.find(receiverClientIndex - 1) != found->second.receivers.end())
	{
		CRevoicePlayer* pClient = GetPlayerByIndex(receiverClientIndex);
		if (!pClient || !pClient->GetClient()->IsActive() || !pClient->GetClient()->IsConnected())
			return -1.0f;

		auto& wave = (pClient->GetCodecType() == vct_speex) ? found->second.wave8k : found->second.wave16k;
		float pos =  std::min(found->second.receivers[receiverClientIndex - 1].current_pos, wave->num_frames()) / wave->num_frames();
		
		return pos * wave->length();
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
		n_frames = wave8k->get_frames(&data);
		found->second.wave8k->push_frames(data, n_frames);
	}

}
void RevoiceAPI::SoundAutoClear(uint32_t wave_id)
{
	auto found = g_audio_waves.find(wave_id);
	if (found != g_audio_waves.end())
	{
		found->second.auto_clear = true;
	}	
}