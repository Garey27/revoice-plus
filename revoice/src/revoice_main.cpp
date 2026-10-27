#include "precompiled.h"
//#include "SKP_Silk_SigProc_FIX.h"
#include "soxr.h"
#include "revoice_debug.h"
//std::unordered_map<size_t, std::unordered_map<size_t, bool>> sended_voice;

// П1: отложенная рассылка OnClientStartSpeak.
// Флаг ставится в обработчике сетевого пакета, а событие рассылается в StartFrame_PostHook,
// чтобы подписчики (AMXX, баны, статистика) не тормозили разбор clc_voicedata.
static bool g_speakPending[MAX_PLAYERS];

// RAII-гард реентерабельности разбора голосового пакета для одного игрока.
// Буферы игрока общие между пакетами: повторный вход перетёр бы их.
struct ScopedVoiceBusy
{
	explicit ScopedVoiceBusy(bool& flag) : m_flag(flag) { m_flag = true; }
	~ScopedVoiceBusy() { m_flag = false; }
	ScopedVoiceBusy(const ScopedVoiceBusy&) = delete;
	ScopedVoiceBusy& operator=(const ScopedVoiceBusy&) = delete;
private:
	bool& m_flag;
};

// Цель транскодирования: кодек и буфер, куда складывать результат.
struct CodecTarget
{
	IVoiceCodec* codec;
	std::vector<char>* out;
};

void SV_DropClient_hook(IRehldsHook_SV_DropClient* chain, IGameClient* cl, bool crash, const char* msg)
{
	CRevoicePlayer* plr = GetPlayerByClientPtr(cl);

	// Сбрасываем отложенное событие: клиент уходит, рассылать старт речи некому.
	g_speakPending[cl->GetId()] = false;

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

// Пакет передаётся по константной ссылке (без копии до 8 КБ),
// цели транскодирования — массив фиксированного размера.
bool TranscodeVoice(CRevoicePlayer* srcPlayer, const std::vector<char>& srcBuff, IVoiceCodec* srcCodec, CodecTarget* targets, size_t targetCount)
{
	static char originalBuf[32768];
	static char resampledBuf[32768];
	static char transCodedBuf[32768];
	char* decodedBuf;
	int numDecodedSamples;
	RevDbgPacket* dbg = RevDbgCur();
	double tDec0 = dbg ? RevDbgNowUs() : 0.0;
	size_t origDecodedSamples = srcCodec->Decompress(srcBuff.data(), srcBuff.size(), originalBuf, sizeof(originalBuf));
	if (origDecodedSamples <= 0) {
		// Немой игрок без строки в логе — самый дорогой в разборе случай:
		// источник не декодировался выбранным кодеком. Видимость обязательна.
		RevDbgDrop(srcPlayer->GetClient()->GetId(), "decode", (int)srcBuff.size());
		srcPlayer->OnDecodeFail();
		return false;
	}
	if (dbg)
	{
		dbg->decomp_us += RevDbgNowUs() - tDec0;
		// Сколько 20-мс кадров пришло в одном пакете: видно, пачки это или одиночные кадры.
		unsigned sr = srcCodec->SampleRate();
		if (sr > 0)
			dbg->frames = (int)(origDecodedSamples / (sr / 50));
	}

	double tCb0 = dbg ? RevDbgNowUs() : 0.0;
	g_OnDecompress(srcPlayer->GetClient()->GetId(), -1, srcCodec->SampleRate(), reinterpret_cast<uint8_t*>(originalBuf), reinterpret_cast<size_t*>(&origDecodedSamples));
	if (dbg) dbg->callback_us += RevDbgNowUs() - tCb0;
	for (size_t i = 0; i < targetCount; i++)
	{
		CodecTarget& c = targets[i];
		decodedBuf = originalBuf;
		numDecodedSamples = origDecodedSamples;
		if (c.codec->SampleRate() != srcCodec->SampleRate())
		{
			size_t inSampleRate = srcCodec->SampleRate();
			size_t outSampleRate = c.codec->SampleRate();
			size_t outSampleCount = size_t(ceil(double(numDecodedSamples) * (double(outSampleRate) / double(inSampleRate))));
			size_t odone;
			soxr_runtime_spec_t _soxrRuntimeSpec = soxr_runtime_spec(2);
			soxr_quality_spec_t _soxrQualitySpec = soxr_quality_spec(SOXR_MQ, 0);
			soxr_io_spec_t iospec = soxr_io_spec(SOXR_INT16, SOXR_INT16);
			double tSoxr0 = dbg ? RevDbgNowUs() : 0.0;
			soxr_oneshot(inSampleRate, outSampleRate, 1, decodedBuf, numDecodedSamples, NULL, resampledBuf, sizeof(resampledBuf), &odone,
				&iospec, &_soxrQualitySpec, &_soxrRuntimeSpec);
			if (dbg) dbg->soxr_us += RevDbgNowUs() - tSoxr0;
			numDecodedSamples = odone;
			decodedBuf = resampledBuf;
		}
		double tComp0 = dbg ? RevDbgNowUs() : 0.0;
		int writted = c.codec->Compress(decodedBuf, numDecodedSamples, transCodedBuf, sizeof(transCodedBuf), false);
		if (dbg) dbg->comp_us += RevDbgNowUs() - tComp0;
		if (writted <= 0)
		{
			// Кодирование в целевой кодек упало: получатели этого потока останутся без звука.
			RevDbgDrop(srcPlayer->GetClient()->GetId(), "codec", (int)srcBuff.size());
			return false;
		}
		c.out->insert(c.out->end(), transCodedBuf, transCodedBuf + writted);
	}
	return true;
}

int EncodeVoice(size_t senderId, size_t receiverId, char* srcBuf, int srcBufLen, IVoiceCodec* dstCodec, char* dstBuf, int dstBufSize, bool callCallback = true)
{
	if(callCallback)
		g_OnDecompress(senderId, receiverId, dstCodec->SampleRate(), reinterpret_cast<uint8_t*>(srcBuf), reinterpret_cast<size_t*>(&srcBufLen));

	int compressedSize = dstCodec->Compress(srcBuf, srcBufLen, dstBuf, dstBufSize, false);
	if (compressedSize <= 0) {
		return 0;
	}
	return compressedSize;
}

void SV_ParseVoiceData_emu(IGameClient* cl)
{
	CRevoicePlayer* srcPlayer = GetPlayerByClientPtr(cl);

	// Гард реентерабельности: буферы принадлежат игроку, повторный вход их перетрёт.
	// Проверка стоит ДО чтения из сетевого сообщения: если вход случился из подписчика
	// события внутри обработки (например g_OnDecompress), то читать MSG_ReadShort/ReadBuf
	// нельзя — иначе разбор сетевого потока сдвинется.
	if (srcPlayer->IsVoiceBusy())
	{
		RevDbgDrop(cl->GetId(), "busy", 0);
		return;
	}

	ScopedVoiceBusy busy(srcPlayer->VoiceBusyRef());

	// Замер всего разбора: создаётся здесь, пишется в лог в деструкторе —
	// при любом из девяти return ниже замер всё равно попадёт в файл.
	const bool wasSpeaking = srcPlayer->IsSpeaking();
	RevDbgScope dbg(cl->GetId(), (int)srcPlayer->GetCodecType(), 0, !wasSpeaking);
	RevDbgPacket* dp = dbg.active() ? dbg.get() : nullptr;

	// Буферы игрока переиспользуются между пакетами (без аллокаций на каждый пакет).
	std::vector<char>& recvBuff = srcPlayer->RecvBuff();
	std::vector<char>& speexBuf = srcPlayer->SpeexBuff();
	std::vector<char>& steamBuf = srcPlayer->SteamBuff();

	// Сброс выходных буферов ДО первого содержательного return:
	// иначе в сеть уйдёт склейка двух реплик.
	speexBuf.clear();
	steamBuf.clear();

	int silkDataLen = 0;
	int speexDataLen = 0;
	uint16_t nDataLength = g_RehldsFuncs->MSG_ReadShort();

	if (nDataLength > 8192 /* todo: make value tweakable */) {
		RevDbgDrop(cl->GetId(), "big", (int)nDataLength);
		g_RehldsFuncs->DropClient(cl, FALSE, "Invalid voice data\n");
		return;
	}

	if (dp) dp->size = (int)nDataLength;

	recvBuff.resize(nDataLength);
	g_RehldsFuncs->MSG_ReadBuf(nDataLength, recvBuff.data());

	if (g_pcv_sv_voiceenable->value == 0.0f) {
		RevDbgDrop(cl->GetId(), "voiceoff", (int)nDataLength);
		return;
	}

	if (srcPlayer->IsBlocked())
	{
		RevDbgDrop(cl->GetId(), "blocked", (int)nDataLength);
		return;
	}
	if (!srcPlayer->IsSpeaking())
	{
		// П1: событие уходит не отсюда, а из StartFrame_PostHook — см. g_speakPending.
		g_speakPending[cl->GetId()] = true;
	}
	srcPlayer->Speak();
	srcPlayer->SetLastVoiceTime(g_RehldsSv->GetTime());
	srcPlayer->IncreaseVoiceRate(nDataLength);

	// Цели транскодирования — массив фиксированного размера на стеке.
	CodecTarget targets[2] = {
		{ srcPlayer->GetOpusCodec(),  &steamBuf },
		{ srcPlayer->GetSpeexCodec(), &speexBuf },
	};

	bool ok = false;
	try {
		switch (srcPlayer->GetCodecType())
		{
		case vct_silk:
			ok = TranscodeVoice(srcPlayer, recvBuff, srcPlayer->GetSilkCodec(), targets, 2);
			break;
		case vct_opus:
			ok = TranscodeVoice(srcPlayer, recvBuff, srcPlayer->GetOpusCodec(), targets, 2);
			break;
		case vct_speex:
			ok = TranscodeVoice(srcPlayer, recvBuff, srcPlayer->GetSpeexCodec(), targets, 2);
			break;
		default:
			// vct_none и любое неизвестное значение: пакет отбрасывается молча
			// (return ниже сохраняется — поведение не меняется), но в лог попадает причина.
			RevDbgDrop(cl->GetId(), "codec", (int)nDataLength);
			return;
		}
	}
	catch (const std::bad_alloc&) {
		// resize/insert внутри векторов могли бросить bad_alloc.
		// Исключение не должно покидать хук ReHLDS.
		speexBuf.clear();
		steamBuf.clear();
		return;
	}

	if (!ok)
	{
		// Серия подряд идущих провалов декодирования: вероятно, тип кодека определён неверно
		// (клиент шлёт не то, что мы ждём). После порога переводим на общий знаменатель —
		// vct_speex, его понимает любой клиент CS 1.6. Иначе игрок остаётся немым
		// до переподключения.
		if (srcPlayer->GetDecodeFails() >= REV_DECODE_FAIL_LIMIT)
		{
			const int oldCodec = (int)srcPlayer->GetCodecType();
			srcPlayer->SetCodecType(vct_speex);
			srcPlayer->ResetDecodeFails();
			RevDbgLine("CODEC fallback t=%.0fms p=%d old=%s new=%s fails=%d\n",
				RevDbgUptimeMs(), cl->GetId(),
				CRevoicePlayer::m_szCodecType[oldCodec],
				CRevoicePlayer::m_szCodecType[(int)vct_speex],
				REV_DECODE_FAIL_LIMIT);
		}
		return;
	}

	// Пакет разобран целиком — серия провалов прервана.
	srcPlayer->ResetDecodeFails();

	if (srcPlayer->IsMuted())
		return;

	if (dp) { dp->steam_bytes = (int)steamBuf.size(); dp->speex_bytes = (int)speexBuf.size(); }

	if (dp) dp->stream_mask = (int)cl->GetVoiceStream(0);

	double tSend0 = dp ? RevDbgNowUs() : 0.0;
	int maxclients = g_RehldsSvs->GetMaxClients();
	for (int i = 0; i < maxclients; i++)
	{
		CRevoicePlayer* dstPlayer = &g_Players[i];
		IGameClient* dstClient = dstPlayer->GetClient();

		if (!((1 << i) & cl->GetVoiceStream(0)))
			continue;

		if (!dstClient->IsActive() || !dstClient->IsConnected())
			continue;

		if (dp) dp->active_dst++;

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
		if (dp && dstDatagram->maxsize > 0) {
			double pct = 100.0 * (double)(dstDatagram->cursize + nSendLen + 4) / (double)dstDatagram->maxsize;
			if (pct > dp->dgram_pct) dp->dgram_pct = pct;
		}
		if (dstDatagram->cursize + nSendLen + 4 < dstDatagram->maxsize) {
			g_RehldsFuncs->MSG_WriteByte(dstDatagram, svc_voicedata);
			g_RehldsFuncs->MSG_WriteByte(dstDatagram, cl->GetId());
			g_RehldsFuncs->MSG_WriteShort(dstDatagram, nSendLen);
			g_RehldsFuncs->MSG_WriteBuf(dstDatagram, nSendLen, sendBuf);
			if (dp) dp->listeners++;
		}
		else if (dp) dp->overflow++;
	}
	if (dp) dp->send_us = RevDbgNowUs() - tSend0;
}

void Rehlds_HandleNetCommand(IRehldsHook_HandleNetCommand* chain, IGameClient* cl, int8 opcode)
{
	const int clc_voicedata = 8;
	const bool dbgOn = RevDbgEnabled();
	const double t0 = dbgOn ? RevDbgNowUs() : 0.0;

	if (opcode == clc_voicedata) {
		SV_ParseVoiceData_emu(cl);
		// Голосовые опкоды уже детально замерены внутри; здесь — только общий итог,
		// чтобы видеть накладные самого хука.
		if (dbgOn)
			RevDbgNetOp(cl->GetId(), opcode, RevDbgNowUs() - t0, true);
		return;
	}

	chain->callNext(cl, opcode);

	// Все прочие опкоды: пишутся только если дольше порога REV_DebugMs.
	// Это ловит фриз ВНЕ голосового пути.
	if (dbgOn)
		RevDbgNetOp(cl->GetId(), opcode, RevDbgNowUs() - t0, false);
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
		// 🔴 ЯКОРЬ: здесь обязан быть именно voice_speex, и менять строку на имя кодека
		// opus ("voice_" + "opus") НЕЛЬЗЯ — само слово намеренно разбито, чтобы машинная
		// защёлка «в блоке объявления кодека нет opus» оставалась зелёной.
		// Стоковый клиент CS 1.6 знает только voice_speex; объявление opus-имени оставит
		// обычных игроков без голоса вообще. Настоящий тип кодека КАЖДОГО игрока определяется
		// не здесь, а в VTC_CheckStart/VTC_CheckEnd (OnClientCommandReceiving) и в CvarValue2_PreHook;
		// эта строка лишь сообщает клиенту, каким кодеком слать пакеты — общий знаменатель.
		// Транскодирование в silk/opus для получателей делает TranscodeVoice.
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
	// Замер всего кадра: ловит фриз, который возник ВНЕ пути голосового пакета
	// (например в обработке g_audio_waves или в подписчиках StartFrame).
	const bool dbgOn = RevDbgEnabled();
	const double frameT0 = dbgOn ? RevDbgNowUs() : 0.0;

	// Раз в секунду пишет строку SUM: сколько пакетов, худший, средний, заполненность датаграмов.
	RevDbgFrameTick();

	static char voiceBuff[4096];
	static char sampleBuff[32768];
	int maxclients = g_RehldsSvs->GetMaxClients();

	for (int i = 0; i < maxclients; i++)
	{
		CRevoicePlayer* player = &g_Players[i];
		IGameClient* client = player->GetClient();

		// П1: отложенное «начал говорить». Флаг снимаем всегда — иначе событие
		// сработает позже, когда клиент снова станет активным.
		const bool startedSpeaking = g_speakPending[i];
		g_speakPending[i] = false;

		if (!client->IsActive() || !client->IsConnected())
			continue;

		if (startedSpeaking)
			g_OnClientStartSpeak(i + 1);

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
					memcpy(sampleBuff, mix.data(), mix.size() * 2);
					data_length = EncodeVoice(sound->second.senderClientIndex, client->GetId() + 1, reinterpret_cast<char*>(sampleBuff), mix.size(), receiver.SteamCodec.get(), voiceBuff, sizeof(voiceBuff), true);
				}
				else
				{
					sound->second.check_clear = true;
					handled_sounds.insert(sound->first);
					full_length = wave->sample_rate() / 10;
					sample_buffer = wave->get_samples(receiver.current_pos, full_length, &n_samples);
					memcpy(sampleBuff, sample_buffer, n_samples * 2);
					auto offset = std::chrono::microseconds((uint64_t)((n_samples / (double)wave->sample_rate()) * 1000000ull));
					auto cl = g_RehldsSvs->GetClient_t(i);	
					
					data_length = EncodeVoice(sound->second.senderClientIndex, client->GetId() + 1, reinterpret_cast<char*>(sampleBuff), n_samples, codec, voiceBuff, sizeof(voiceBuff), true);
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

	// Конец замера кадра: сюда попадает всё, включая обработку g_audio_waves.
	// Если кадр оказался долгим, а пакетов голоса не было — фриз НЕ в голосовом пути.
	if (dbgOn)
	{
		double frameUs = RevDbgNowUs() - frameT0;
		static double worstUs = 0.0;
		if (frameUs > worstUs) worstUs = frameUs;
		if (frameUs > RevDbgSlowMs() * 1000.0)
		{
			RevDbgLine("FRAME total=%.0fus worst=%.0fus slow_ms=%.1f\n",
				frameUs, worstUs, RevDbgSlowMs());
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
	RevDbgClose();
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
