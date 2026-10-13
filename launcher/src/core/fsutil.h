// RDoom launcher - file system helpers that behave the same on Horizon (libnx
// devoptab, sdmc:/...) and on a POSIX host (tests).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rdoom {

struct DirEntry
{
	std::string name;   // file name only
	std::string path;   // full path
	bool isDir = false;
	uint64_t size = 0;
};

std::string PathJoin(const std::string &a, const std::string &b);
std::string PathFileName(const std::string &path);
std::string PathStem(const std::string &path);      // file name without extension
std::string PathExtLower(const std::string &path);  // ".wad", "" if none
std::string PathParent(const std::string &path);
std::string ToLower(std::string s);

bool FileExists(const std::string &path);
bool DirExists(const std::string &path);
uint64_t FileSize(const std::string &path);
// Creates every missing component. Returns true if the directory exists afterwards.
bool MakeDirs(const std::string &path);
// Lists a directory (no "." / ".."), sorted by name, case-insensitive.
std::vector<DirEntry> ListDir(const std::string &path);
// Recursive file listing up to maxDepth levels below path.
std::vector<DirEntry> ListFilesRecursive(const std::string &path, int maxDepth);

bool ReadFile(const std::string &path, std::string &out);
// Reads at most len bytes at offset. Returns bytes read.
size_t ReadAt(const std::string &path, uint64_t offset, void *buf, size_t len);

enum class WriteResult { Ok, OpenFailed, WriteFailed, NoSpace, RenameFailed };
const char *WriteResultName(WriteResult r);

// Writes to "<path>.tmp", flushes, keeps the previous file as "<path>.bak" and
// renames the temporary file over the target. The previous file stays intact
// if any step fails.
WriteResult WriteFileAtomic(const std::string &path, const std::string &data);

// Free bytes on the volume holding path, or UINT64_MAX if unknown.
uint64_t FreeSpace(const std::string &path);

bool RemoveFile(const std::string &path);
// Deletes a directory and everything below it (used only for cache folders the
// program or the graphics driver created). Returns the number of files removed,
// -1 if the directory could not be read.
int RemoveTree(const std::string &path);
bool CopyFile(const std::string &from, const std::string &to);

} // namespace rdoom
