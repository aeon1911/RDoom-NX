// RDoom launcher - IWAD/mod thumbnails from the files' own TITLEPIC, cached
// as small PNGs in <root>/cache/thumbs so nothing large lives in the NRO.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "canvas.h"
#include "library.h"
#include "paths.h"

namespace rdoom {

class ThumbCache
{
public:
	explicit ThumbCache(Paths paths) : paths_(std::move(paths)) {}
	// Returns the thumbnail if ready; otherwise queues it and returns nullptr.
	const Image *Get(const LibraryItem &item, int w, int h);
	// Builds at most one queued thumbnail (keeps the UI responsive).
	void Step();
	void Clear();
	bool Busy() const { return !queue_.empty(); }

private:
	struct Job { std::string path, key; int w, h; };
	std::string CacheFile(const std::string &key) const;
	Paths paths_;
	std::map<std::string, Image> ready_;
	std::map<std::string, bool> failed_;
	std::vector<Job> queue_;
};

} // namespace rdoom
