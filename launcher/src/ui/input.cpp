#include "platform.h"

namespace rdoom {

uint32_t RepeatTracker::Update(uint32_t held, uint32_t down, uint64_t nowMs)
{
	// directions and shoulder buttons repeat; face buttons never do
	const uint32_t repeatable = BtnUp | BtnDown | BtnLeft | BtnRight | BtnL | BtnR | BtnZL | BtnZR;
	uint32_t out = down;
	for (int b = 0; b < 16; b++)
	{
		const uint32_t m = 1u << b;
		if (!(m & repeatable))
			continue;
		if (down & m)
		{
			since_[b] = nowMs;
			last_[b] = nowMs;
		}
		else if ((held & m) && (lastHeld_ & m))
		{
			if (nowMs - since_[b] >= 380 && nowMs - last_[b] >= 85)
			{
				out |= m;
				last_[b] = nowMs;
			}
		}
	}
	lastHeld_ = held;
	return out;
}

} // namespace rdoom
