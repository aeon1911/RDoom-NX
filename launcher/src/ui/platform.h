// RDoom launcher - what the UI needs from the system. Implemented with libnx
// (platform_nx.cpp) and, for screenshots/tests on a PC, by tests/ui_preview.cpp.
#pragma once

#include <cstdint>
#include <string>
#include "canvas.h"

namespace rdoom {

enum Button : uint32_t
{
	BtnA = 1u << 0, BtnB = 1u << 1, BtnX = 1u << 2, BtnY = 1u << 3,
	BtnL = 1u << 4, BtnR = 1u << 5, BtnZL = 1u << 6, BtnZR = 1u << 7,
	BtnPlus = 1u << 8, BtnMinus = 1u << 9,
	BtnUp = 1u << 10, BtnDown = 1u << 11, BtnLeft = 1u << 12, BtnRight = 1u << 13,
};

struct InputFrame
{
	uint32_t held = 0;
	uint32_t down = 0;    // pressed this frame
	uint32_t repeat = 0;  // down + auto-repeat of held directions/shoulders
	bool touch = false;
	int touchX = 0, touchY = 0;
};

// Auto-repeat for held buttons: first repeat after 380 ms, then every 85 ms.
class RepeatTracker
{
public:
	uint32_t Update(uint32_t held, uint32_t down, uint64_t nowMs);
private:
	uint32_t lastHeld_ = 0;
	uint64_t since_[16] = {};
	uint64_t last_[16] = {};
};

struct SystemInfo
{
	bool appletMode = false;        // homebrew started as an applet (little memory)
	uint64_t memTotal = 0, memUsed = 0;
	uint64_t sdFree = UINT64_MAX;
	bool docked = false;
	std::string firmware;
	std::string emulator;           // non-empty if a known emulator is detected
};

class Platform
{
public:
	virtual ~Platform() = default;
	// Returns false when the system asks the app to close.
	virtual bool Poll(InputFrame &in) = 0;
	virtual Canvas &BeginDraw() = 0;
	virtual void EndDraw() = 0;
	virtual uint64_t NowMs() = 0;
	// Software keyboard; false if cancelled or unavailable.
	virtual bool TextInput(const std::string &title, const std::string &initial, size_t maxLen, std::string &out) = 0;
	virtual SystemInfo Info() = 0;
	// Called before the engine starts: free the display.
	virtual void Shutdown() = 0;
};

} // namespace rdoom
