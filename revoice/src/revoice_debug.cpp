#include "precompiled.h"

// Отладочный инструментарий: замеры по этапам разбора голосового пакета.
// Подробности и уровни — в revoice_debug.h.

#include "revoice_debug.h"

#include <cstdarg>
#include <ctime>

namespace
{
	FILE   *g_file        = nullptr;
	bool    g_triedOpen   = false;
	double  g_lastFlushUs = 0.0;

	// Сводка по кадрам
	double  g_frameMaxUs  = 0.0;   // худший пакет за интервал
	double  g_frameSumUs  = 0.0;   // суммарное время всех пакетов
	int     g_pktCount    = 0;     // сколько пакетов разобрано
	int     g_slowCount   = 0;     // сколько превысило порог
	int     g_firstCount  = 0;     // сколько было «первым пакетом» (нажатие)
	double  g_dgramMaxPct = 0.0;   // худшая заполненность датаграма
	int     g_overflowCnt = 0;     // сколько пакетов не влезло в датаграм

	RevDbgPacket *g_cur = nullptr;

	// Открыть лог: сначала в каталоге игры, иначе — в текущем каталоге.
	void ensureOpen()
	{
		if (g_triedOpen)
			return;

		g_triedOpen = true;

		char gamedir[512] = {0};
		GET_GAME_DIR(gamedir);

		char path[600];
		if (gamedir[0] != '\0')
		{
			snprintf(path, sizeof(path), "%s/rev_debug.log", gamedir);
			g_file = fopen(path, "a");
			if (g_file)
				return;
		}

		g_file = fopen("rev_debug.log", "a");
	}

	// Сброс буфера не чаще раза в секунду: иначе flush сам станет фризом.
	void flushMaybe(double now_us)
	{
		if (!g_file)
			return;
		if (now_us - g_lastFlushUs < 1000000.0)
			return;
		g_lastFlushUs = now_us;
		fflush(g_file);
	}

	const char *codecName(int codec)
	{
		switch (codec)
		{
		case 1: return "silk";
		case 2: return "opus";
		case 3: return "speex";
		default: return "none";
		}
	}
}

double RevDbgNowUs()
{
	using namespace std::chrono;
	return (double)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

bool RevDbgEnabled()    { return g_pcv_rev_debug && g_pcv_rev_debug->value > 0.0f; }
int  RevDbgLevel()      { return (g_pcv_rev_debug && g_pcv_rev_debug->value > 0.0f) ? (int)g_pcv_rev_debug->value : 0; }
double RevDbgSlowMs()   { return (g_pcv_rev_debug_ms && g_pcv_rev_debug_ms->value > 0.0f) ? (double)g_pcv_rev_debug_ms->value : REVDBG_DEFAULT_SLOW_MS; }

RevDbgPacket *RevDbgCur() { return g_cur; }

RevDbgScope::RevDbgScope(int player, int codec, int size, bool first)
	: m_t0(0.0), m_active(false)
{
	if (!RevDbgEnabled())
		return;

	m_active = true;
	m_pkt.player = player;
	m_pkt.codec  = codec;
	m_pkt.size   = size;
	m_pkt.first  = first;

	// Пауза с предыдущего пакета этого игрока: признак «нажатия» и серий пакетов.
	if (player >= 0 && player < MAX_PLAYERS)
	{
		static double lastUs[MAX_PLAYERS] = {0.0};
		double now = RevDbgNowUs();
		m_pkt.gap_us = (lastUs[player] > 0.0) ? (now - lastUs[player]) : 0.0;
		lastUs[player] = now;
		// Серия: пакеты подряд с интервалом меньше 10 мс.
		static int burstCnt[MAX_PLAYERS] = {0};
		burstCnt[player] = (m_pkt.gap_us > 0.0 && m_pkt.gap_us < 10000.0) ? (burstCnt[player] + 1) : 1;
		m_pkt.burst = burstCnt[player];
	}
	else if (player >= 0)
	{
		// Индекс вне диапазона — не наш клиент, но замер всё равно нужен.
		m_pkt.burst = 0;
	}

	m_t0 = RevDbgNowUs();
	g_cur = &m_pkt;
}

RevDbgScope::~RevDbgScope()
{
	if (!m_active)
		return;

	g_cur = nullptr;
	ensureOpen();
	if (!g_file)
		return;

	m_pkt.total_us = RevDbgNowUs() - m_t0;

	double slow_ms = RevDbgSlowMs();
	int level = RevDbgLevel();

	// Сводка за интервал
	g_frameSumUs += m_pkt.total_us;
	g_pktCount++;
	if (m_pkt.total_us > g_frameMaxUs)
		g_frameMaxUs = m_pkt.total_us;
	if (m_pkt.total_us > slow_ms * 1000.0)
		g_slowCount++;
	if (m_pkt.first)
		g_firstCount++;
	if (m_pkt.dgram_pct > g_dgramMaxPct)
		g_dgramMaxPct = m_pkt.dgram_pct;
	g_overflowCnt += m_pkt.overflow;

	bool write = (level >= 2) || (m_pkt.total_us > slow_ms * 1000.0) || m_pkt.first;
	if (write)
	{
		fprintf(g_file,
			"PKT t=%.0fms p=%d codec=%s sz=%d first=%d burst=%d frames=%d gap=%.0fus total=%.0fus "
			"decomp=%.0f cb=%.0f soxr=%.0f comp=%.0f send=%.0f "
			"dst=%d act=%d mask=0x%x ovf=%d steam=%d speex=%d dgram=%.0f%%\n",
			RevDbgUptimeMs(),
			m_pkt.player, codecName(m_pkt.codec), m_pkt.size, m_pkt.first ? 1 : 0,
			m_pkt.burst, m_pkt.frames, m_pkt.gap_us, m_pkt.total_us,
			m_pkt.decomp_us, m_pkt.callback_us, m_pkt.soxr_us, m_pkt.comp_us, m_pkt.send_us,
			m_pkt.listeners, m_pkt.active_dst, m_pkt.stream_mask,
			m_pkt.overflow, m_pkt.steam_bytes, m_pkt.speex_bytes,
			m_pkt.dgram_pct);
	}

	flushMaybe(RevDbgNowUs());
}

void RevDbgFrameTick()
{
	if (!RevDbgEnabled())
		return;

	ensureOpen();
	if (!g_file)
		return;

	static double lastSumUs = 0.0;
	double now = RevDbgNowUs();
	if (now - lastSumUs < 1000000.0)
		return;
	lastSumUs = now;

	if (g_pktCount == 0 && RevDbgLevel() < 2)
	{
		flushMaybe(now);
		return;
	}

	double avg = (g_pktCount > 0) ? (g_frameSumUs / (double)g_pktCount) : 0.0;
	fprintf(g_file,
		"SUM pkts=%d first=%d slow=%d max=%.0fus avg=%.0fus dgram_max=%.0f%% ovf=%d\n",
		g_pktCount, g_firstCount, g_slowCount, g_frameMaxUs, avg, g_dgramMaxPct, g_overflowCnt);

	g_pktCount = 0;
	g_slowCount = 0;
	g_firstCount = 0;
	g_frameSumUs = 0.0;
	g_frameMaxUs = 0.0;
	g_dgramMaxPct = 0.0;
	g_overflowCnt = 0;

	fflush(g_file);
	g_lastFlushUs = now;
}

void RevDbgLine(const char *fmt, ...)
{
	if (!RevDbgEnabled())
		return;

	ensureOpen();
	if (!g_file)
		return;

	va_list ap;
	va_start(ap, fmt);
	vfprintf(g_file, fmt, ap);
	va_end(ap);
	fflush(g_file);
}

void RevDbgClose()
{
	g_cur = nullptr;
	if (g_file)
	{
		fflush(g_file);
		fclose(g_file);
		g_file = nullptr;
	}
	g_triedOpen = false;
}

// Время с загрузки плагина, мс: позволяет совместить строку лога
// с моментом фриза, замеченным по секундомеру.
double RevDbgUptimeMs()
{
	static double startUs = 0.0;
	double now = RevDbgNowUs();
	if (startUs == 0.0)
		startUs = now;
	return (now - startUs) / 1000.0;
}

// Отброшенный голосовой пакет: пишем причину и размер.
// Нужно, чтобы понять, теряется ли голос молча.
void RevDbgDrop(int player, const char *reason, int size)
{
	if (!RevDbgEnabled())
		return;
	ensureOpen();
	if (!g_file)
		return;
	fprintf(g_file, "DROP t=%.0fms p=%d reason=%s sz=%d\n",
		RevDbgUptimeMs(), player, reason, size);
	fflush(g_file);
}

// Замер любого сетевого опкода: ловит фриз ВНЕ голосового пути.
// voice=true — это наш clc_voicedata (пишется по порогу), false — остальные
// (пишутся только если дольше порога, чтобы не завалить лог).
void RevDbgNetOp(int player, int opcode, double us, bool voice)
{
	if (!RevDbgEnabled())
		return;

	// Остальные опкоды фильтруем по порогу.
	if (!voice && us < RevDbgSlowMs() * 1000.0)
		return;

	ensureOpen();
	if (!g_file)
		return;
	fprintf(g_file, "NET t=%.0fms p=%d opcode=%d voice=%d us=%.0f\n",
		RevDbgUptimeMs(), player, opcode, voice ? 1 : 0, us);
	fflush(g_file);
}
