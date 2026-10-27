#pragma once

// Отладочный инструментарий revoice-plus.
//
// НАЗНАЧЕНИЕ: найти источник фриза при нажатии микрофона. Раньше источник искали
// по чтению кода и ошиблись: правки (отложенное событие, переиспользование буферов)
// фриз не убрали. Здесь — замеры фактом, по этапам.
//
// ВКЛЮЧЕНИЕ (cvar `REV_Debug`):
//   0 — выключено (по умолчанию, без накладных расходов)
//   1 — только медленные пакеты (порог `REV_DebugMs`) + сводка по кадрам раз в секунду
//   2 — каждый пакет + сводка
//
// ЛОГ: <gamedir>/rev_debug.log; если недоступно — rev_debug.log в рабочем каталоге.
//
// ВАЖНО: файл открывается один раз, буфер сбрасывается не чаще раза в секунду —
// чтобы само логирование не стало источником фризов.

#include <chrono>
#include <cstdio>

#define REVDBG_DEFAULT_SLOW_MS 5.0

struct RevDbgPacket
{
	int    player      = -1;    // индекс клиента-источника
	int    codec       = 0;     // тип кодека источника
	int    size        = 0;     // размер входящего пакета, байт
	bool   first       = false; // первый пакет после тишины = нажатие микрофона
	double total_us    = 0.0;   // весь разбор пакета
	double decomp_us   = 0.0;   // декодирование источника
	double callback_us = 0.0;   // g_OnDecompress (подписчики AMXX)
	double soxr_us     = 0.0;   // ресемплинг soxr (создание + работа)
	double comp_us     = 0.0;   // кодирование в целевые кодеки
	double send_us     = 0.0;   // рассылка получателям
	int    listeners   = 0;     // сколько получателей получило пакет
	int    overflow    = 0;     // сколько пропущено: датаграм не хватило
	int    steam_bytes = 0;     // размер steam-потока на выходе
	int    speex_bytes = 0;     // размер speex-потока на выходе
	double gap_us      = 0.0;   // пауза с предыдущего пакета этого игрока
	double dgram_pct   = 0.0;   // максимальная заполненность датаграма, %
	int    burst       = 0;     // номер пакета в серии (<10 мс с предыдущего)
	int    frames      = 0;     // сколько кадров пришло в пакете (sz / 20мс-кадр)
	int    stream_mask = 0;     // GetVoiceStream(0) — кому слать
	int    active_dst  = 0;     // сколько получателей было активно/подключено
};

// Монотонное время в микросекундах.
double RevDbgNowUs();

bool   RevDbgEnabled();
int    RevDbgLevel();
double RevDbgSlowMs();

// Текущий разбираемый пакет (nullptr вне разбора) — для замеров внутри TranscodeVoice.
RevDbgPacket *RevDbgCur();

// RAII: замер начинается в конструкторе, запись в лог — в деструкторе.
// Гарантирует запись при любом return (в SV_ParseVoiceData_emu их девять).
class RevDbgScope
{
public:
	RevDbgScope(int player, int codec, int size, bool first);
	~RevDbgScope();
	RevDbgPacket *get() { return &m_pkt; }
	bool active() const { return m_active; }
private:
	RevDbgPacket m_pkt;
	double       m_t0;
	bool         m_active;
	RevDbgScope(const RevDbgScope &) = delete;
	RevDbgScope &operator=(const RevDbgScope &) = delete;
};

// Вызывать раз в кадр: копит сводку, раз в секунду сбрасывает буфер на диск.
void RevDbgFrameTick();

// Закрыть лог (при выгрузке плагина).
void RevDbgClose();

// Произвольная строка в лог (события вне пакетов).
void RevDbgLine(const char *fmt, ...);

// Отброшенные голосовые пакеты: причина и счётчик.
// Причины: busy, big, voiceoff, blocked, decode, muted, codec, nomem, notok.
void RevDbgDrop(int player, const char *reason, int size);

// Замер одного сетевого опкода (все опкоды, не только голос).
void RevDbgNetOp(int player, int opcode, double us, bool voice);

// Время с момента загрузки плагина в мс — для совмещения с наблюдением по секундомеру.
double RevDbgUptimeMs();
