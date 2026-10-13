#include "fsutil.h"
#include "log.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace rdoom {

std::string ToLower(std::string s)
{
	for (auto &c : s)
		if (c >= 'A' && c <= 'Z')
			c = char(c - 'A' + 'a');
	return s;
}

std::string PathJoin(const std::string &a, const std::string &b)
{
	if (a.empty())
		return b;
	if (b.empty())
		return a;
	if (a.back() == '/')
		return a + (b.front() == '/' ? b.substr(1) : b);
	return a + (b.front() == '/' ? b : "/" + b);
}

std::string PathFileName(const std::string &path)
{
	size_t p = path.find_last_of('/');
	return p == std::string::npos ? path : path.substr(p + 1);
}

std::string PathStem(const std::string &path)
{
	std::string n = PathFileName(path);
	size_t d = n.find_last_of('.');
	return (d == std::string::npos || d == 0) ? n : n.substr(0, d);
}

std::string PathExtLower(const std::string &path)
{
	std::string n = PathFileName(path);
	size_t d = n.find_last_of('.');
	return (d == std::string::npos || d == 0) ? std::string() : ToLower(n.substr(d));
}

std::string PathParent(const std::string &path)
{
	size_t p = path.find_last_of('/');
	if (p == std::string::npos)
		return std::string();
	if (p > 0 && path[p - 1] == ':')  // "sdmc:/x" -> "sdmc:/"
		return path.substr(0, p + 1);
	return p == 0 ? "/" : path.substr(0, p);
}

bool FileExists(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool DirExists(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

uint64_t FileSize(const std::string &path)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return 0;
	return (uint64_t)st.st_size;
}

bool MakeDirs(const std::string &path)
{
	if (path.empty())
		return false;
	if (DirExists(path))
		return true;
	std::string cur;
	size_t start = 0;
	// keep a device prefix ("sdmc:/") or root "/" intact
	size_t colon = path.find(":/");
	if (colon != std::string::npos)
	{
		cur = path.substr(0, colon + 2);
		start = colon + 2;
	}
	else if (path[0] == '/')
	{
		cur = "/";
		start = 1;
	}
	while (start <= path.size())
	{
		size_t slash = path.find('/', start);
		std::string part = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
		if (!part.empty())
		{
			cur = (cur.empty() || cur.back() == '/') ? cur + part : cur + "/" + part;
			if (!DirExists(cur) && mkdir(cur.c_str(), 0777) != 0 && errno != EEXIST)
			{
				RLOG_W("fs", "mkdir %s failed: %s", cur.c_str(), strerror(errno));
				return false;
			}
		}
		if (slash == std::string::npos)
			break;
		start = slash + 1;
	}
	return DirExists(path);
}

std::vector<DirEntry> ListDir(const std::string &path)
{
	std::vector<DirEntry> out;
	DIR *d = opendir(path.c_str());
	if (!d)
		return out;
	while (struct dirent *e = readdir(d))
	{
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		DirEntry de;
		de.name = e->d_name;
		de.path = PathJoin(path, de.name);
		struct stat st;
		if (stat(de.path.c_str(), &st) == 0)
		{
			de.isDir = S_ISDIR(st.st_mode);
			de.size = de.isDir ? 0 : (uint64_t)st.st_size;
		}
		out.push_back(std::move(de));
	}
	closedir(d);
	std::sort(out.begin(), out.end(), [](const DirEntry &a, const DirEntry &b) { return ToLower(a.name) < ToLower(b.name); });
	return out;
}

static void ListRec(const std::string &path, int depth, int maxDepth, std::vector<DirEntry> &out)
{
	for (auto &e : ListDir(path))
	{
		if (e.isDir)
		{
			if (depth < maxDepth)
				ListRec(e.path, depth + 1, maxDepth, out);
		}
		else
			out.push_back(e);
	}
}

std::vector<DirEntry> ListFilesRecursive(const std::string &path, int maxDepth)
{
	std::vector<DirEntry> out;
	ListRec(path, 0, maxDepth, out);
	return out;
}

bool ReadFile(const std::string &path, std::string &out)
{
	out.clear();
	FILE *f = fopen(path.c_str(), "rb");
	if (!f)
		return false;
	char buf[16384];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	bool ok = !ferror(f);
	fclose(f);
	return ok;
}

size_t ReadAt(const std::string &path, uint64_t offset, void *buf, size_t len)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f)
		return 0;
	size_t n = 0;
	if (fseeko(f, (off_t)offset, SEEK_SET) == 0)
		n = fread(buf, 1, len, f);
	fclose(f);
	return n;
}

const char *WriteResultName(WriteResult r)
{
	switch (r)
	{
	case WriteResult::Ok: return "ok";
	case WriteResult::OpenFailed: return "cannot open file for writing (read-only or missing folder?)";
	case WriteResult::WriteFailed: return "write failed";
	case WriteResult::NoSpace: return "not enough free space on the SD card";
	case WriteResult::RenameFailed: return "could not replace the old file";
	}
	return "?";
}

WriteResult WriteFileAtomic(const std::string &path, const std::string &data)
{
	uint64_t free = FreeSpace(PathParent(path));
	if (free != UINT64_MAX && free < data.size() + 64 * 1024)
	{
		RLOG_E("fs", "no space to write %s (%llu bytes free)", path.c_str(), (unsigned long long)free);
		return WriteResult::NoSpace;
	}
	const std::string tmp = path + ".tmp";
	const std::string bak = path + ".bak";
	FILE *f = fopen(tmp.c_str(), "wb");
	if (!f)
	{
		RLOG_E("fs", "cannot open %s: %s", tmp.c_str(), strerror(errno));
		return WriteResult::OpenFailed;
	}
	size_t w = fwrite(data.data(), 1, data.size(), f);
	bool ok = (w == data.size()) && fflush(f) == 0;
#if !defined(__SWITCH__)
	if (ok)
		ok = fsync(fileno(f)) == 0;
#endif
	ok = (fclose(f) == 0) && ok;
	if (!ok)
	{
		int err = errno;
		RLOG_E("fs", "writing %s failed: %s", tmp.c_str(), strerror(err));
		remove(tmp.c_str());
		return err == ENOSPC ? WriteResult::NoSpace : WriteResult::WriteFailed;
	}
	// Horizon's rename() does not replace an existing file, so the old file is
	// moved aside first and restored if the final rename fails.
	bool hadOld = FileExists(path);
	if (hadOld)
	{
		remove(bak.c_str());
		if (rename(path.c_str(), bak.c_str()) != 0)
		{
			RLOG_E("fs", "cannot move %s aside: %s", path.c_str(), strerror(errno));
			remove(tmp.c_str());
			return WriteResult::RenameFailed;
		}
	}
	if (rename(tmp.c_str(), path.c_str()) != 0)
	{
		RLOG_E("fs", "cannot rename %s: %s", tmp.c_str(), strerror(errno));
		if (hadOld)
			rename(bak.c_str(), path.c_str());
		remove(tmp.c_str());
		return WriteResult::RenameFailed;
	}
	return WriteResult::Ok;
}

uint64_t FreeSpace(const std::string &path)
{
	struct statvfs sv;
	if (statvfs(path.empty() ? "." : path.c_str(), &sv) != 0)
		return UINT64_MAX;
	return (uint64_t)sv.f_bavail * (uint64_t)sv.f_frsize;
}

bool RemoveFile(const std::string &path)
{
	return remove(path.c_str()) == 0;
}

int RemoveTree(const std::string &path)
{
	DIR *d = opendir(path.c_str());
	if (!d)
		return -1;
	int removed = 0;
	std::vector<std::string> dirs;
	while (dirent *e = readdir(d))
	{
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		const std::string full = path + "/" + e->d_name;
		struct stat st;
		if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
			dirs.push_back(full);
		else if (remove(full.c_str()) == 0)
			removed++;
	}
	closedir(d);
	for (const auto &sub : dirs)
	{
		const int n = RemoveTree(sub);
		if (n > 0)
			removed += n;
	}
	rmdir(path.c_str());
	return removed;
}

bool CopyFile(const std::string &from, const std::string &to)
{
	std::string data;
	if (!ReadFile(from, data))
		return false;
	return WriteFileAtomic(to, data) == WriteResult::Ok;
}

} // namespace rdoom
