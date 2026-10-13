// RDoom launcher - leveled, categorized log.
#pragma once

#include <cstdio>

#include <string>

namespace rdoom {

enum class LogLevel { Debug = 0, Info, Warn, Error };

// Opens (appends to) the log file. Without a file, messages go to stderr only.
void LogOpen(const std::string &path, bool truncate);
void LogClose();
void LogSetLevel(LogLevel minLevel);
// Off: lines stay in the stdio buffer until LogFlush (a background thread
// flushes them, so a game frame never waits for an SD card write).
void LogSetFlushEachLine(bool on);
void LogFlush();
// the open log file (nullptr when none): the Switch build hands it to its background writer
FILE *LogFileHandle();
// category: short tag such as "profiles", "scan", "vulkan", "fs".
void Log(LogLevel level, const char *category, const char *fmt, ...)
#if defined(__GNUC__)
	__attribute__((format(printf, 3, 4)))
#endif
	;

#define RLOG_D(cat, ...) ::rdoom::Log(::rdoom::LogLevel::Debug, cat, __VA_ARGS__)
#define RLOG_I(cat, ...) ::rdoom::Log(::rdoom::LogLevel::Info, cat, __VA_ARGS__)
#define RLOG_W(cat, ...) ::rdoom::Log(::rdoom::LogLevel::Warn, cat, __VA_ARGS__)
#define RLOG_E(cat, ...) ::rdoom::Log(::rdoom::LogLevel::Error, cat, __VA_ARGS__)

} // namespace rdoom
