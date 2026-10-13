#include "archive.h"
#include "fsutil.h"

#include <cstdio>
#include <cstring>
#include <zlib.h>

namespace rdoom {

static uint16_t Rd16(const uint8_t *p) { return uint16_t(p[0] | (p[1] << 8)); }
static uint32_t Rd32(const uint8_t *p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
static uint64_t Rd64(const uint8_t *p) { return uint64_t(Rd32(p)) | (uint64_t(Rd32(p + 4)) << 32); }

static bool ReadExact(FILE *f, uint64_t off, void *buf, size_t len)
{
	if (fseeko(f, (off_t)off, SEEK_SET) != 0)
		return false;
	return fread(buf, 1, len, f) == len;
}

bool Archive::Open(const std::string &path)
{
	path_ = path;
	entries_.clear();
	type_ = ArchiveType::None;
	iwadHeader_ = false;
	error_.clear();

	FILE *f = fopen(path.c_str(), "rb");
	if (!f)
	{
		error_ = "cannot open file";
		return false;
	}
	fseeko(f, 0, SEEK_END);
	uint64_t fileSize = (uint64_t)ftello(f);
	uint8_t magic[6] = {};
	bool ok = false;
	if (fileSize < 4 || !ReadExact(f, 0, magic, fileSize >= 6 ? 6 : 4))
	{
		error_ = "file too small";
	}
	else if (!memcmp(magic, "IWAD", 4) || !memcmp(magic, "PWAD", 4))
	{
		iwadHeader_ = magic[0] == 'I';
		ok = OpenWad(f, fileSize);
	}
	else if (!memcmp(magic, "PK\x03\x04", 4) || !memcmp(magic, "PK\x05\x06", 4))
	{
		ok = OpenZip(f, fileSize);
	}
	else if (fileSize >= 6 && !memcmp(magic, "7z\xBC\xAF\x27\x1C", 6))
	{
		type_ = ArchiveType::SevenZip;
		ok = true;
	}
	else
	{
		error_ = "unknown file signature";
	}
	fclose(f);
	return ok;
}

bool Archive::OpenWad(FILE *f, uint64_t fileSize)
{
	uint8_t hdr[12];
	if (!ReadExact(f, 0, hdr, 12))
	{
		error_ = "truncated WAD header";
		return false;
	}
	uint32_t num = Rd32(hdr + 4), dirOfs = Rd32(hdr + 8);
	if (num > 1000000 || (uint64_t)dirOfs + (uint64_t)num * 16 > fileSize)
	{
		error_ = "WAD directory outside the file (truncated or corrupt)";
		return false;
	}
	std::vector<uint8_t> dir((size_t)num * 16);
	if (num && !ReadExact(f, dirOfs, dir.data(), dir.size()))
	{
		error_ = "cannot read WAD directory";
		return false;
	}
	entries_.reserve(num);
	for (uint32_t i = 0; i < num; i++)
	{
		const uint8_t *d = &dir[(size_t)i * 16];
		ArchiveEntry e;
		e.offset = Rd32(d);
		e.size = Rd32(d + 4);
		char name[9] = {};
		memcpy(name, d + 8, 8);
		for (char &c : name)
			if (c >= 'a' && c <= 'z')
				c = char(c - 'a' + 'A');
		e.name = name;
		e.csize = e.size;
		entries_.push_back(std::move(e));
	}
	type_ = ArchiveType::Wad;
	return true;
}

bool Archive::OpenZip(FILE *f, uint64_t fileSize)
{
	// The end-of-central-directory record is in the last 64 KiB + 22 bytes.
	const uint64_t tail = fileSize < 65557 ? fileSize : 65557;
	std::vector<uint8_t> buf((size_t)tail);
	if (!ReadExact(f, fileSize - tail, buf.data(), buf.size()))
	{
		error_ = "cannot read ZIP trailer";
		return false;
	}
	int64_t eocd = -1;
	for (int64_t i = (int64_t)tail - 22; i >= 0; i--)
	{
		if (Rd32(&buf[(size_t)i]) == 0x06054b50)
		{
			eocd = i;
			break;
		}
	}
	if (eocd < 0)
	{
		error_ = "ZIP end record not found (truncated download?)";
		return false;
	}
	const uint8_t *e = &buf[(size_t)eocd];
	uint64_t count = Rd16(e + 10);
	uint64_t cdSize = Rd32(e + 12);
	uint64_t cdOfs = Rd32(e + 16);
	// ZIP64 locator sits right before the classic record
	if ((count == 0xFFFF || cdOfs == 0xFFFFFFFF || cdSize == 0xFFFFFFFF) && eocd >= 20 &&
	    Rd32(&buf[(size_t)eocd - 20]) == 0x07064b50)
	{
		uint64_t z64ofs = Rd64(&buf[(size_t)eocd - 20 + 8]);
		uint8_t z[56];
		if (!ReadExact(f, z64ofs, z, sizeof(z)) || Rd32(z) != 0x06064b50)
		{
			error_ = "bad ZIP64 end record";
			return false;
		}
		count = Rd64(z + 32);
		cdSize = Rd64(z + 40);
		cdOfs = Rd64(z + 48);
	}
	if (cdOfs + cdSize > fileSize || count > 2000000)
	{
		error_ = "ZIP directory outside the file (truncated or corrupt)";
		return false;
	}
	std::vector<uint8_t> cd((size_t)cdSize);
	if (cdSize && !ReadExact(f, cdOfs, cd.data(), cd.size()))
	{
		error_ = "cannot read ZIP directory";
		return false;
	}
	size_t p = 0;
	entries_.reserve((size_t)count);
	for (uint64_t i = 0; i < count; i++)
	{
		if (p + 46 > cd.size() || Rd32(&cd[p]) != 0x02014b50)
		{
			error_ = "corrupt ZIP directory";
			return false;
		}
		const uint8_t *h = &cd[p];
		ArchiveEntry en;
		en.method = Rd16(h + 10);
		en.csize = Rd32(h + 20);
		en.size = Rd32(h + 24);
		uint16_t nameLen = Rd16(h + 28), extraLen = Rd16(h + 30), commentLen = Rd16(h + 32);
		en.offset = Rd32(h + 42);
		if (p + 46 + nameLen + extraLen + commentLen > cd.size())
		{
			error_ = "corrupt ZIP directory";
			return false;
		}
		en.name.assign((const char *)h + 46, nameLen);
		for (char &c : en.name)
			if (c == '\\')
				c = '/';
		// ZIP64 extra field
		const uint8_t *x = h + 46 + nameLen;
		for (size_t xp = 0; xp + 4 <= extraLen;)
		{
			uint16_t id = Rd16(x + xp), len = Rd16(x + xp + 2);
			if (id == 0x0001)
			{
				const uint8_t *v = x + xp + 4;
				size_t vp = 0;
				if (en.size == 0xFFFFFFFF && vp + 8 <= len) { en.size = Rd64(v + vp); vp += 8; }
				if (en.csize == 0xFFFFFFFF && vp + 8 <= len) { en.csize = Rd64(v + vp); vp += 8; }
				if (en.offset == 0xFFFFFFFF && vp + 8 <= len) { en.offset = Rd64(v + vp); vp += 8; }
			}
			xp += 4 + len;
		}
		p += 46 + nameLen + extraLen + commentLen;
		if (!en.name.empty() && en.name.back() == '/')
			continue;  // directory
		entries_.push_back(std::move(en));
	}
	type_ = ArchiveType::Zip;
	return true;
}

const ArchiveEntry *Archive::Find(const std::string &name) const
{
	const std::string want = ToLower(name);
	const bool anyExt = type_ == ArchiveType::Zip && PathExtLower(want).empty();
	for (const auto &e : entries_)
	{
		std::string n = ToLower(e.name);
		if (n == want)
			return &e;
		if (anyExt)
		{
			size_t dot = n.find_last_of('.');
			if (dot != std::string::npos && n.find('/', dot) == std::string::npos && n.substr(0, dot) == want)
				return &e;
		}
	}
	return nullptr;
}

bool InflateRaw(const std::string &in, uint64_t expectedSize, std::string &out)
{
	out.assign((size_t)expectedSize, '\0');
	z_stream zs;
	memset(&zs, 0, sizeof(zs));
	if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)
		return false;
	zs.next_in = (Bytef *)in.data();
	zs.avail_in = (uInt)in.size();
	zs.next_out = (Bytef *)&out[0];
	zs.avail_out = (uInt)out.size();
	int r = inflate(&zs, Z_FINISH);
	bool ok = (r == Z_STREAM_END) && zs.total_out == expectedSize;
	inflateEnd(&zs);
	return ok;
}

bool Archive::Read(const ArchiveEntry &e, std::string &out, uint64_t maxBytes) const
{
	out.clear();
	if (e.size > maxBytes)
		return false;
	FILE *f = fopen(path_.c_str(), "rb");
	if (!f)
		return false;
	bool ok = false;
	if (type_ == ArchiveType::Wad)
	{
		out.resize((size_t)e.size);
		ok = e.size == 0 || ReadExact(f, e.offset, &out[0], out.size());
	}
	else if (type_ == ArchiveType::Zip)
	{
		uint8_t lh[30];
		if (ReadExact(f, e.offset, lh, 30) && Rd32(lh) == 0x04034b50)
		{
			uint64_t dataOfs = e.offset + 30 + Rd16(lh + 26) + Rd16(lh + 28);
			std::string raw((size_t)e.csize, '\0');
			if (e.csize == 0 || ReadExact(f, dataOfs, &raw[0], raw.size()))
			{
				if (e.method == 0)
				{
					out = std::move(raw);
					ok = out.size() == e.size;
				}
				else if (e.method == 8)
				{
					ok = InflateRaw(raw, e.size, out);
				}
			}
		}
	}
	fclose(f);
	if (!ok)
		out.clear();
	return ok;
}

} // namespace rdoom
