// RDoom launcher - read-only access to the archive formats UZDoom loads:
// WAD (IWAD/PWAD directories) and ZIP (pk3/ipk3/zip, including ZIP64).
// 7z (pk7) is only recognised by its signature; its directory is not read.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rdoom {

enum class ArchiveType { None, Wad, Zip, SevenZip };

struct ArchiveEntry
{
	std::string name;      // WAD: 8-char lump name, upper case. ZIP: full path, '/' separated
	uint64_t offset = 0;   // WAD: data offset. ZIP: local header offset
	uint64_t size = 0;     // uncompressed size
	uint64_t csize = 0;    // ZIP: compressed size
	uint16_t method = 0;   // ZIP: 0 stored, 8 deflate
};

class Archive
{
public:
	bool Open(const std::string &path);
	ArchiveType Type() const { return type_; }
	bool IsIwadHeader() const { return iwadHeader_; }
	const std::string &Error() const { return error_; }
	const std::vector<ArchiveEntry> &Entries() const { return entries_; }

	// WAD: exact lump name (case-insensitive). ZIP: path relative to the root,
	// case-insensitive; if no extension is given, any extension matches.
	const ArchiveEntry *Find(const std::string &name) const;
	// Reads (and inflates) an entry. maxBytes guards against huge entries.
	bool Read(const ArchiveEntry &e, std::string &out, uint64_t maxBytes = 16u << 20) const;

private:
	bool OpenWad(FILE *f, uint64_t fileSize);
	bool OpenZip(FILE *f, uint64_t fileSize);

	std::string path_;
	ArchiveType type_ = ArchiveType::None;
	bool iwadHeader_ = false;
	std::string error_;
	std::vector<ArchiveEntry> entries_;
};

// Inflates raw deflate data (no zlib header). Returns false on corrupt data.
bool InflateRaw(const std::string &in, uint64_t expectedSize, std::string &out);

} // namespace rdoom
