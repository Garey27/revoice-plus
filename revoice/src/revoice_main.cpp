#include "precompiled.h"
//#include "SKP_Silk_SigProc_FIX.h"
#include "soxr.h"
//std::unordered_map<size_t, std::unordered_map<size_t, bool>> sended_voice;

void SV_DropClient_hook(IRehldsHook_SV_DropClient* chain, IGameClient* cl, bool crash, const char* msg)
{
	CRevoicePlayer* plr = GetPlayerByClientPtr(cl);

	plr->OnDisconnected();

	chain->callNext(cl, crash, msg);
}

void CvarValue2_PreHook(const edict_t* pEnt, int requestID, const char* cvarName, const char* cvarValue)
{
	CRevoicePlayer* plr = GetPlayerByEdict(pEnt);
	if (plr->GetRequestId() != requestID) {
		RETURN_META(MRES_IGNORED);
	}

	const char* lastSeparator = strrchr(cvarValue, ',');
	if (lastSeparator)
	{
		int buildNumber = atoi(lastSeparator + 1);
		if (buildNumber > 4554) {
			plr->SetCodecType(vct_opus);
		}
	}

	RETURN_META(MRES_IGNORED);
}

bool TranscodeVoice(CRevoicePlayer* srcPlayer, std::vector<char> srcBuff, IVoiceCodec* srcCodec, std::vector<std::pair<IVoiceCodec*, std::vector<char>*>> codecBuffers)
{
	static char originalBuf[32768];
	static char resampledBuf[32768];
	static char transCodedBuf[32768];
	char* decodedBuf;
	int numDecodedSamples;
	size_t origDecodedSamples = srcCodec->Decompress(srcBuff.data(), srcBuff.size(), originalBuf, sizeof(originalBuf));
	if (origDecodedSamples <= 0) {
		return false;
	}
	g_OnDecompress(srcPlayer->GetClient()->GetId(), srcCodec->SampleRate(), reinterpret_cast<uint8_t*>(originalBuf), reinterpret_cast<size_t*>(&origDecodedSamples));
	for (auto& c : codecBuffers)
	{
		decodedBuf = originalBuf;
		numDecodedSamples = origDecodedSamples;
		if (c.first->SampleRate() != srcCodec->SampleRate())
		{
			size_t inSampleRate = srcCodec->SampleRate();
			size_t outSampleRate = c.first->SampleRate();
			size_t outSampleCount = size_t(ceil(double(numDecodedSamples) * (double(outSampleRate) / double(inSampleRate))));
			size_t odone;
			soxr_runtime_spec_t _soxrRuntimeSpec = soxr_runtime_spec(2);
			soxr_quality_spec_t _soxrQualitySpec = soxr_quality_spec(SOXR_MQ, 0);
			soxr_io_spec_t iospec = soxr_io_spec(SOXR_INT16, SOXR_INT16);
			soxr_oneshot(inSampleRate, outSampleRate, 1, decodedBuf, numDecodedSamples, NULL, resampledBuf, sizeof(resampledBuf), &odone,
				&iospec, &_soxrQualitySpec, &_soxrRuntimeSpec);
			numDecodedSamples = odone;
			decodedBuf = resampledBuf;
		}
		int writted = c.first->Compress(decodedBuf, numDecodedSamples, transCodedBuf, sizeof(transCodedBuf), false);
		if (writted <= 0)
		{
			return false;
		}
		c.second->insert(c.second->end(), transCodedBuf, transCodedBuf + writted);
	}
	return true;
}

int EncodeVoice(size_t senderId, char* srcBuf, int srcBufLen, IVoiceCodec* dstCodec, char* dstBuf, int dstBufSize, bool callCallback = true)
{
	if(callCallback)
		g_OnDecompress(senderId, dstCodec->SampleRate(), reinterpret_cast<uint8_t*>(srcBuf), reinterpret_cast<size_t*>(&srcBufLen));

	int compressedSize = dstCodec->Compress(srcBuf, srcBufLen, dstBuf, dstBufSize, false);
	if (compressedSize <= 0) {
		return 0;
	}
	return compressedSize;
}

void SV_ParseVoiceData_emu(IGameClient* cl)
{
	std::vector<char> recvBuff;
	std::vector<char> speexBuf;
	std::vector<char> steamBuf;

	int silkDataLen = 0;
	int speexDataLen = 0;
	uint16_t nDataLength = g_RehldsFuncs->MSG_ReadShort();

	if (nDataLength > 8192 /* todo: make value tweakable */) {
		g_RehldsFuncs->DropClient(cl, FALSE, "Invalid voice data\n");
		return;
	}
	recvBuff.resize(nDataLength);
	g_RehldsFuncs->MSG_ReadBuf(nDataLength, recvBuff.data());

	if (g_pcv_sv_voiceenable->value == 0.0f) {
		return;
	}

	CRevoicePlayer* srcPlayer = GetPlayerByClientPtr(cl);
	if (srcPlayer->IsBlocked())
		return;
	if (!srcPlayer->IsSpeaking())
	{
		g_OnClientStartSpeak(cl->GetId() + 1);
	}
	srcPlayer->Speak();
	srcPlayer->SetLastVoiceTime(g_RehldsSv->GetTime());
	srcPlayer->IncreaseVoiceRate(nDataLength);


	switch (srcPlayer->GetCodecType())
	{
	case vct_silk:
	{
		if (!TranscodeVoice(srcPlayer, recvBuff, srcPlayer->GetSilkCodec(), { {srcPlayer->GetOpusCodec(), &steamBuf}, {srcPlayer->GetSpeexCodec(), &speexBuf} }))
		{
			return;
		}
		break;
	}
	case vct_opus:
	{
		if (!TranscodeVoice(srcPlayer, recvBuff, srcPlayer->GetOpusCodec(), { {srcPlayer->GetOpusCodec(), &steamBuf}, {srcPlayer->GetSpeexCodec(), &speexBuf} }))
		{
			return;
		}
		break;
	}
	case vct_speex:
	{
		if (!TranscodeVoice(srcPlayer, recvBuff, srcPlayer->GetSpeexCodec(), { {srcPlayer->GetOpusCodec(), &steamBuf}, {srcPlayer->GetSpeexCodec(), &speexBuf} }))
		{
			return;
		}
		break;
	}
	default:
		return;
	}

	if (srcPlayer->IsMuted())
		return;

	int maxclients = g_RehldsSvs->GetMaxClients();
	for (int i = 0; i < maxclients; i++)
	{
		CRevoicePlayer* dstPlayer = &g_Players[i];
		IGameClient* dstClient = dstPlayer->GetClient();

		if (!((1 << i) & cl->GetVoiceStream(0)))
			continue;

		if (!dstClient->IsActive() || !dstClient->IsConnected())
			continue;

		if (g_mute_map[i][cl->GetId()] || g_mute_map[i][-1])
			continue;

		char* sendBuf;
		int nSendLen;
		switch (dstPlayer->GetCodecType())
		{
		case vct_silk:
		case vct_opus:
			sendBuf = steamBuf.data();
			nSendLen = steamBuf.size();
			break;
		case vct_speex:
			sendBuf = speexBuf.data();
			nSendLen = speexBuf.size();
			break;
		default:
			sendBuf = nullptr;
			nSendLen = 0;
			break;
		}

		if (sendBuf == nullptr || nSendLen == 0)
			continue;

		if (dstPlayer == srcPlayer && !dstClient->GetLoopback())
			continue;

		sizebuf_t* dstDatagram = dstClient->GetDatagram();
		if (dstDatagram->cursize + nSendLen + 4 < dstDatagram->maxsize) {
			g_RehldsFuncs->MSG_WriteByte(dstDatagram, svc_voicedata);
			g_RehldsFuncs->MSG_WriteByte(dstDatagram, cl->GetId());
			g_RehldsFuncs->MSG_WriteShort(dstDatagram, nSendLen);
			g_RehldsFuncs->MSG_WriteBuf(dstDatagram, nSendLen, sendBuf);
		}
	}
}

void Rehlds_HandleNetCommand(IRehldsHook_HandleNetCommand* chain, IGameClient* cl, int8 opcode)
{
	const int clc_voicedata = 8;
	if (opcode == clc_voicedata) {
		SV_ParseVoiceData_emu(cl);
		return;
	}

	chain->callNext(cl, opcode);
}

qboolean ClientConnect_PostHook(edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128])
{
	CRevoicePlayer* plr = GetPlayerByEdict(pEntity);
	plr->OnConnected();

	RETURN_META_VALUE(MRES_IGNORED, TRUE);
}

void ServerActivate_PostHook(edict_t* pEdictList, int edictCount, int clientMax)
{
	// It is bad because it sends to hltv
	MESSAGE_BEGIN(MSG_INIT, SVC_STUFFTEXT);
	WRITE_STRING("VTC_CheckStart\n");
	MESSAGE_END();
	MESSAGE_BEGIN(MSG_INIT, SVC_STUFFTEXT);
	WRITE_STRING("vgui_runscript\n");
	MESSAGE_END();
	MESSAGE_BEGIN(MSG_INIT, SVC_STUFFTEXT);
	WRITE_STRING("VTC_CheckEnd\n");
	MESSAGE_END();

	Revoice_Exec_Config();
	SET_META_RESULT(MRES_IGNORED);
}
void ServerDeactivate_PostHook()
{
	g_mute_map.clear();
	g_audio_waves.clear();
	SET_META_RESULT(MRES_IGNORED);
}

void SV_WriteVoiceCodec_hooked(IRehldsHook_SV_WriteVoiceCodec* chain, sizebuf_t* sb)
{
	IGameClient* cl = g_RehldsFuncs->GetHostClient();
	CRevoicePlayer* plr = GetPlayerByClientPtr(cl);

	switch (plr->GetCodecType())
	{
	case vct_silk:
	case vct_opus:
	case vct_speex:
	{
		g_RehldsFuncs->MSG_WriteByte(sb, svc_voiceinit);
		g_RehldsFuncs->MSG_WriteString(sb, "voice_speex");	// codec id
		g_RehldsFuncs->MSG_WriteByte(sb, 5);				// quality
		break;
	}
	default:
		LCPrintf(true, "SV_WriteVoiceCodec() called on client(%d) with unknown voice codec\n", cl->GetId());
		break;
	}
}

bool Revoice_Load()
{
	if (!Revoice_Utils_Init())
		return false;

	if (!Revoice_RehldsApi_Init()) {
		LCPrintf(true, "Failed to locate REHLDS API\n");
		return false;
	}

	if (!Revoice_ReunionApi_Init())
		return false;

	Revoice_Init_Cvars();
	Revoice_Init_Config();
	Revoice_Init_Players();

	if (!Revoice_Main_Init()) {
		LCPrintf(true, "Initialization failed\n");
		return false;
	}
	g_RehldsApi->GetFuncs()->RegisterPluginApi("VoiceTranscoder", &g_voiceTranscoderAPI);
	g_RehldsApi->GetFuncs()->RegisterPluginApi("RevoicePlus", &g_revoiceAPI);

	return true;
}

void PlayerPreThink(edict_t* pEntity)
{
	/*
	if (pEntity)
	{
		sended_voice[ENTINDEX(pEntity) - 1].clear();
	}
	*/
}
short int mix_sample(short int sample1, short int sample2) {
	const int32_t result(static_cast<int32_t>(sample1) + static_cast<int32_t>(sample2));
	typedef std::numeric_limits<short int> Range;
	if (Range::max() < result)
		return Range::max();
	else if (Range::min() > result)
		return Range::min();
	else
		return result;
}

int SV_CalcPing(client_t* cl)
{
	float ping;
	int i;
	int count;
	int back;
	client_frame_t* frame;
	int idx;

	if (cl->fakeclient)
	{
		return 0;
	}

	back = 16;
	ping = 0.0f;
	count = 0;
	for (i = 0; i < back; i++)
	{
		idx = cl->netchan.incoming_acknowledged + ~i;
		frame = &cl->frames[64 & idx];

		if (frame->ping_time > 0.0f)
		{
			ping += frame->ping_time;
			count++;
		}
	}

	if (count)
	{
		ping /= count;
		if (ping > 0.0f)
		{
			return ping * 1000.0f;
		}
	}
	return 0;
}


void StartFrame_PostHook()
{
	static char voiceBuff[4096];

	int maxclients = g_RehldsSvs->GetMaxClients();
	for (int i = 0; i < maxclients; i++)
	{
		CRevoicePlayer* player = &g_Players[i];
		IGameClient* client = player->GetClient();


		if (!client->IsActive() || !client->IsConnected())
			continue;

		if (!player->CheckSpeaking() && player->IsSpeaking())
		{
			player->SpeakDone();
			g_OnClientStopSpeak(client->GetId() + 1);
		}
	}
	auto end = g_audio_waves.end();
	auto now = std::chrono::high_resolution_clock::now();
	
	for (int i = 0; i < maxclients; i++)
	{
		CRevoicePlayer* player = &g_Players[i];
		IGameClient* client = player->GetClient();
		
		if (!client->IsActive() || !client->IsConnected())
			continue;

		size_t n_samples = 0;
		uint8_t* sample_buffer;
		size_t data_length = 0;
		size_t full_length;
		std::vector<uint16_t> mix;
		std::unordered_set<uint32_t> handled_sounds;
		for (auto sound = g_audio_waves.begin(); sound != end; sound++)
		{
			if (handled_sounds.find(sound->first) != handled_sounds.end())
			{
				continue;
			}

			if (g_mute_map[i][sound->second.senderClientIndex] || g_mute_map[i][-1])
				continue;

			if (sound->second.receivers.find(i) == sound->second.receivers.end())
			{
				continue;
			}

			auto& receiver = sound->second.receivers[i];
			auto& wave = (player->GetCodecType() == vct_speex) ? sound->second.wave8k : sound->second.wave16k;
			IVoiceCodec* codec = (player->GetCodecType() == vct_speex) ? (IVoiceCodec*)receiver.FrameCodec.get() : (IVoiceCodec*)receiver.SteamCodec.get();

			if (receiver.state != play_state::PLAY_PLAY)
			{
				continue;
			}

			if (receiver.current_pos >= wave->num_frames())
			{
				auto index = sound->first;
				handled_sounds.insert(index);
				if (!sound->second.auto_clear)
				{
					g_OnSoundComplete(i + 1, index);


					end = g_audio_waves.end();
					sound = g_audio_waves.begin();
				}
				continue;
			}

			if (now >= receiver.nextSend && receiver.current_pos < wave->num_frames())
			{

				std::unordered_set<uint32_t> need_mix;
				need_mix.insert(sound->first);
				for (auto sound2 = g_audio_waves.begin(); sound2 != g_audio_waves.end(); sound2++)
				{
					if (receiver.state != play_state::PLAY_PLAY)
					{
						continue;
					}

					if (sound != sound2 && sound->second.senderClientIndex == sound2->second.senderClientIndex && sound2->second.receivers.find(i) != sound2->second.receivers.end())
					{
						need_mix.insert(sound2->first);
					}
				}

				if (need_mix.size() > 1)
				{

					for (auto& j : need_mix)
					{
						auto& s = g_audio_waves[j];
						auto& receiver = s.receivers[i];
						auto& wave = (player->GetCodecType() == vct_speex) ? s.wave8k : s.wave16k;
						s.check_clear = true;
						handled_sounds.insert(i);
						size_t mix_samples = 0;
						full_length = wave->sample_rate() / 10;
						auto ptr = (uint16_t*)wave->get_samples(receiver.current_pos, full_length, &mix_samples);

						auto offset = std::chrono::milliseconds((size_t)(mix_samples / (double)wave->sample_rate() * 1000));					// idk
						auto latency = std::chrono::high_resolution_clock::now() - now;
						receiver.nextSend = std::chrono::high_resolution_clock::now() + offset - latency;
						if (mix.empty())
						{
							mix = { ptr , ptr + mix_samples };
						}
						else
						{
							std::vector<uint16_t> vec = { ptr , ptr + mix_samples };
							for (size_t k = 0; k < mix.size(); k++)
							{
								if (k >= vec.size())
								{
									break;
								}
								mix[k] = mix_sample(mix[k], vec[k]);
							}
							if (vec.size() > mix.size())
							{
								mix.insert(mix.end(), vec.begin() + mix.size(), vec.end());
							}
						}						
						receiver.current_pos += mix_samples;
						
					}
					data_length = EncodeVoice(sound->second.senderClientIndex, reinterpret_cast<char*>(mix.data()), mix.size(), receiver.SteamCodec.get(), voiceBuff, sizeof(voiceBuff), true);
				}
				else
				{
					sound->second.check_clear = true;
					handled_sounds.insert(sound->first);
					full_length = wave->sample_rate() / 10;
					sample_buffer = wave->get_samples(receiver.current_pos, full_length, &n_samples);
					auto offset = std::chrono::microseconds((uint64_t)((n_samples / (double)wave->sample_rate()) * 1000000ull));
					auto cl = g_RehldsSvs->GetClient_t(i);	
					
					data_length = EncodeVoice(sound->second.senderClientIndex, reinterpret_cast<char*>(sample_buffer), n_samples, codec, voiceBuff, sizeof(voiceBuff), true);
					// idk
					auto latency = std::chrono::high_resolution_clock::now() - now;
					receiver.nextSend = std::chrono::high_resolution_clock::now() + offset-latency;
					receiver.current_pos += n_samples;
				}


				char* sendBuf = voiceBuff;
				int nSendLen = data_length;
				if (nSendLen == 0 || sendBuf == nullptr)
					continue;
				sizebuf_t* dstDatagram = client->GetDatagram();
				if (dstDatagram->cursize + nSendLen + 4 < dstDatagram->maxsize) {
					g_RehldsFuncs->MSG_WriteByte(dstDatagram, svc_voicedata);
					g_RehldsFuncs->MSG_WriteByte(dstDatagram, sound->second.senderClientIndex);
					g_RehldsFuncs->MSG_WriteShort(dstDatagram, nSendLen);
					g_RehldsFuncs->MSG_WriteBuf(dstDatagram, nSendLen, sendBuf);
				}
			}


		}

	}
	for (auto sound = g_audio_waves.begin(); sound != end; sound++)
	{
		if (!sound->second.auto_clear || !sound->second.check_clear)
		{
			continue;
		}

		sound->second.check_clear = false;
		std::vector<unsigned int> staleReceivers;
		for (auto& receiver : sound->second.receivers)
		{
			CRevoicePlayer* player = &g_Players[receiver.first];
			IGameClient* client = player->GetClient();

			if (!client->IsActive() || !client->IsConnected())
			{
				staleReceivers.push_back(receiver.first);
			}
		}

		for (auto& key : staleReceivers)
		{
			sound->second.receivers.erase(key);
		}

		size_t min_pos = sound->second.wave8k->num_frames();

		for (auto& receiver : sound->second.receivers)
		{
			if (g_Players[receiver.first].GetCodecType() != vct_speex)
			{
				continue;
			}
			min_pos = std::min(receiver.second.current_pos, min_pos);
		}
		sound->second.wave8k->clear_frames(min_pos);

		for (auto& receiver : sound->second.receivers)
		{
			if (g_Players[receiver.first].GetCodecType() != vct_speex)
			{
				continue;
			}
			receiver.second.current_pos -= min_pos;
		}

		min_pos = sound->second.wave16k->num_frames();
		for (auto& receiver : sound->second.receivers)
		{
			if (g_Players[receiver.first].GetCodecType() == vct_speex)
			{
				continue;
			}
			min_pos = std::min(receiver.second.current_pos, min_pos);
		}
		sound->second.wave16k->clear_frames(min_pos);

		for (auto& receiver : sound->second.receivers)
		{
			if (g_Players[receiver.first].GetCodecType() == vct_speex)
			{
				continue;
			}
			receiver.second.current_pos -= min_pos;
		}
	}
}


bool Revoice_Main_Init()
{
	g_RehldsHookchains->SV_DropClient()->registerHook(&SV_DropClient_hook, HC_PRIORITY_DEFAULT + 1);
	g_RehldsHookchains->HandleNetCommand()->registerHook(&Rehlds_HandleNetCommand, HC_PRIORITY_DEFAULT + 1);
	g_RehldsHookchains->SV_WriteVoiceCodec()->registerHook(&SV_WriteVoiceCodec_hooked, HC_PRIORITY_DEFAULT + 1);
	return true;
}

void Revoice_Main_DeInit()
{
	g_RehldsHookchains->SV_DropClient()->unregisterHook(&SV_DropClient_hook);
	g_RehldsHookchains->HandleNetCommand()->unregisterHook(&Rehlds_HandleNetCommand);
	g_RehldsHookchains->SV_WriteVoiceCodec()->unregisterHook(&SV_WriteVoiceCodec_hooked);

	Revoice_DeInit_Cvars();
}

// Entity API
void OnClientCommandReceiving(edict_t* pClient) {
	CRevoicePlayer* plr = GetPlayerByEdict(pClient);
	auto command = CMD_ARGV(0);

	if (FStrEq(command, "VTC_CheckStart")) {
		plr->SetCheckingState(1);
		plr->SetCodecType(CodecType::vct_speex);
		RETURN_META(MRES_SUPERCEDE);
	}
	else if (plr->GetCheckingState()) {
		if (FStrEq(command, "vgui_runscript")) {
			plr->SetCheckingState(2);
			RETURN_META(MRES_SUPERCEDE);
		}
		else if (FStrEq(command, "VTC_CheckEnd")) {
			plr->SetCodecType(plr->GetCheckingState() == 2 ? vct_opus : vct_speex);
			plr->SetCheckingState(0);
			RETURN_META(MRES_SUPERCEDE);
		}
	}

	RETURN_META(MRES_IGNORED);
}
