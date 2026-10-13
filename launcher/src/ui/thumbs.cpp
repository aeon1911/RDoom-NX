#include "thumbs.h"
#include "content.h"
#include "fsutil.h"
#include "log.h"

namespace rdoom {

static std::string KeyFor(const LibraryItem &item, int w, int h)
{
	std::string stem = PathStem(item.info.path);
	for (char &c : stem)
		if (!isalnum((unsigned char)c) && c != '-' && c != '_')
			c = '_';
	return stem + "-" + std::to_string(item.info.size) + "-" + std::to_string(w) + "x" + std::to_string(h);
}

std::string ThumbCache::CacheFile(const std::string &key) const
{
	return PathJoin(paths_.Thumbs(), key + ".png");
}

const Image *ThumbCache::Get(const LibraryItem &item, int w, int h)
{
	if (item.info.status != FileStatus::Ok || !item.info.hasTitlePic)
		return nullptr;
	const std::string key = KeyFor(item, w, h);
	auto it = ready_.find(key);
	if (it != ready_.end())
		return &it->second;
	if (failed_.count(key))
		return nullptr;
	for (auto &j : queue_)
		if (j.key == key)
			return nullptr;
	queue_.push_back({ item.info.path, key, w, h });
	return nullptr;
}

void ThumbCache::Step()
{
	if (queue_.empty())
		return;
	Job j = queue_.front();
	queue_.erase(queue_.begin());
	Image img;
	const std::string file = CacheFile(j.key);
	if (FileExists(file) && LoadImageFile(file, img) && img.w == j.w && img.h == j.h)
	{
		ready_[j.key] = std::move(img);
		return;
	}
	std::vector<uint8_t> rgba;
	int w = 0, h = 0;
	if (!ExtractTitlePic(j.path, "", rgba, w, h))
	{
		failed_[j.key] = true;
		return;
	}
	Image full;
	full.w = w;
	full.h = h;
	full.px.assign((const uint32_t *)rgba.data(), (const uint32_t *)rgba.data() + (size_t)w * h);
	// Doom pictures are 320x200 shown at 4:3; widescreen title pictures
	// (e.g. 854x200) keep their 4:3 centre, like the engine on a 4:3 screen.
	if (full.h > 0 && full.w > full.h * 2)
	{
		const int cw = full.h * 16 / 10;
		Image crop;
		crop.w = cw;
		crop.h = full.h;
		crop.px.resize((size_t)cw * full.h);
		const int ox = (full.w - cw) / 2;
		for (int y = 0; y < full.h; y++)
			for (int x = 0; x < cw; x++)
				crop.px[(size_t)y * cw + x] = full.px[(size_t)y * full.w + ox + x];
		full = std::move(crop);
	}
	Image t = ScaleImage(full, j.w, j.h);
	for (auto &p : t.px)
		p |= 0xFF000000u;
	t.opaque = true;
	if (!SavePng(file, t))
		RLOG_W("thumbs", "cannot write %s", file.c_str());
	ready_[j.key] = std::move(t);
}

void ThumbCache::Clear()
{
	ready_.clear();
	failed_.clear();
	queue_.clear();
}

} // namespace rdoom
