#include "log.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace rdoom {

static FILE *g_logFile = nullptr;
static LogLevel g_minLevel = LogLevel::Info;
static std::mutex g_logMutex;
static bool g_flushEachLine = true;

void LogOpen(const std::string &path, bool truncate)
{
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile)
		fclose(g_logFile);
	g_logFile = fopen(path.c_str(), truncate ? "w" : "a");
}

void LogClose()
{
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile)
		fclose(g_logFile);
	g_logFile = nullptr;
}

void LogSetLevel(LogLevel minLevel)
{
	g_minLevel = minLevel;
}

void LogSetFlushEachLine(bool on)
{
	std::lock_guard<std::mutex> lock(g_logMutex);
	g_flushEachLine = on;
}

void LogFlush()
{
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile)
		fflush(g_logFile);
}

FILE *LogFileHandle()
{
	std::lock_guard<std::mutex> lock(g_logMutex);
	return g_logFile;
}

void Log(LogLevel level, const char *category, const char *fmt, ...)
{
	if (level < g_minLevel)
		return;
	static const char *const names[] = { "DEBUG", "INFO", "WARN", "ERROR" };
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	char stamp[32];
	time_t now = time(nullptr);
	struct tm tmv;
#if defined(_WIN32)
	localtime_s(&tmv, &now);
#else
	localtime_r(&now, &tmv);
#endif
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);

	std::lock_guard<std::mutex> lock(g_logMutex);
	FILE *outs[2] = { g_logFile, g_logFile ? nullptr : stderr };
	for (FILE *f : outs)
	{
		if (!f)
			continue;
		fprintf(f, "%s [%s] %s: %s\n", stamp, names[(int)level], category, msg);
		if (g_flushEachLine || f == stderr)
			fflush(f);
	}
}

} // namespace rdoom
