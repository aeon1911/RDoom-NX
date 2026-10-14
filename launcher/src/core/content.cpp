#include "content.h"
#include <algorithm>
#include "fsutil.h"
#include "log.h"

#include <cctype>
#include <cstring>
#include <set>

#include "rd_image.h"

namespace rdoom {

// ---------------------------------------------------------------------------
// iwadinfo.txt
// ---------------------------------------------------------------------------

namespace {

struct Tokenizer
{
	const std::string &s;
	size_t p = 0;
	explicit Tokenizer(const std::string &text) : s(text) {}

	void SkipSpace()
	{
		for (;;)
		{
			while (p < s.size() && isspace((unsigned char)s[p]))
				p++;
			if (p + 1 < s.size() && s[p] == '/' && s[p + 1] == '/')
			{
				while (p < s.size() && s[p] != '\n')
					p++;
				continue;
			}
			if (p + 1 < s.size() && s[p] == '/' && s[p + 1] == '*')
			{
				size_t e = s.find("*/", p + 2);
				p = e == std::string::npos ? s.size() : e + 2;
				continue;
			}
			break;
		}
	}
	// Returns false at end. quoted is set for "string" tokens.
	bool Next(std::string &tok, bool &quoted)
	{
		SkipSpace();
		quoted = false;
		tok.clear();
		if (p >= s.size())
			return false;
		char c = s[p];
		if (c == '"')
		{
			quoted = true;
			p++;
			while (p < s.size() && s[p] != '"')
			{
				if (s[p] == '\\' && p + 1 < s.size())
					p++;
				tok += s[p++];
			}
			p++;
			return true;
		}
		if (c == '{' || c == '}' || c == '=' || c == ',')
		{
			tok = c;
			p++;
			return true;
		}
		while (p < s.size() && !isspace((unsigned char)s[p]) && !strchr("{}=,\"", s[p]))
			tok += s[p++];
		return true;
	}
};

} // namespace

std::vector<IwadDef> ParseIwadInfo(const std::string &text)
{
	std::vector<IwadDef> defs;
	Tokenizer tz(text);
	std::string tok;
	bool q;
	while (tz.Next(tok, q))
	{
		std::string block = ToLower(tok);
		if (!tz.Next(tok, q) || tok != "{")
			break;
		const bool isIwad = block == "iwad";
		IwadDef def;
		std::string key;
		std::vector<std::string> values;
		auto flush = [&]() {
			if (!isIwad || key.empty())
				return;
			std::string k = ToLower(key);
			const std::string v = values.empty() ? std::string() : values[0];
			if (k == "name") def.name = v;
			else if (k == "game") def.game = v;
			else if (k == "autoname") def.autoname = v;
			else if (k == "iwadname") def.iwadName = v;
			else if (k == "mustcontain") def.mustContain = values;
			else if (k == "required") def.required = v;
		};
		// key = value[, value...]   (Names { "ab" } blocks have bare values)
		int depth = 1;
		bool afterEq = false;
		while (depth > 0 && tz.Next(tok, q))
		{
			if (!q && tok == "{") { depth++; continue; }
			if (!q && tok == "}") { flush(); key.clear(); values.clear(); depth--; continue; }
			if (!q && tok == "=") { afterEq = true; continue; }
			if (!q && tok == ",") continue;
			if (afterEq)
			{
				values.push_back(tok);
				// the value list ends when the next token is not a comma
				size_t save = tz.p;
				std::string t2;
				bool q2;
				if (tz.Next(t2, q2) && !q2 && t2 == ",")
					continue;
				tz.p = save;
				flush();
				key.clear();
				values.clear();
				afterEq = false;
			}
			else
			{
				key = tok;
			}
		}
		if (isIwad && !def.name.empty())
			defs.push_back(std::move(def));
	}
	return defs;
}

std::vector<IwadDef> LoadIwadDefs(const std::string &enginePk3OrTxt)
{
	std::string text;
	if (PathExtLower(enginePk3OrTxt) == ".txt")
	{
		ReadFile(enginePk3OrTxt, text);
	}
	else
	{
		Archive a;
		if (a.Open(enginePk3OrTxt))
			if (const ArchiveEntry *e = a.Find("iwadinfo.txt"))
				a.Read(*e, text);
	}
	if (text.empty())
	{
		RLOG_W("scan", "no iwadinfo.txt in %s; IWADs will be identified by file name only", enginePk3OrTxt.c_str());
		return {};
	}
	auto defs = ParseIwadInfo(text);
	RLOG_I("scan", "loaded %zu IWAD definitions from %s", defs.size(), enginePk3OrTxt.c_str());
	return defs;
}

// ---------------------------------------------------------------------------
// classification
// ---------------------------------------------------------------------------

const char *FileStatusName(FileStatus s)
{
	switch (s)
	{
	case FileStatus::Ok: return "ok";
	case FileStatus::Missing: return "missing";
	case FileStatus::Invalid: return "invalid";
	case FileStatus::Unsupported: return "unsupported";
	}
	return "?";
}

const char *ModCategoryName(ModCategory c)
{
	switch (c)
	{
	case ModCategory::Unknown: return "Unknown";
	case ModCategory::MapPack: return "Map pack";
	case ModCategory::Gameplay: return "Gameplay";
	case ModCategory::GameplayAndMaps: return "Gameplay + maps";
	case ModCategory::Resources: return "Resources";
	case ModCategory::Dehacked: return "DeHackEd patch";
	}
	return "?";
}

bool IsSupportedIwadExtension(const std::string &e)
{
	return e == ".wad" || e == ".iwad" || e == ".ipk3" || e == ".ipk7" || e == ".pk3" || e == ".pk7" || e == ".zip";
}

bool IsSupportedModExtension(const std::string &e)
{
	return e == ".wad" || e == ".pk3" || e == ".pk7" || e == ".zip" || e == ".7z" || e == ".pke" ||
	       e == ".deh" || e == ".bex" || e == ".lmp";
}

static bool IsMapMarker(const std::string &n)
{
	if (n.size() == 5 && n.compare(0, 3, "MAP") == 0 && isdigit((unsigned char)n[3]) && isdigit((unsigned char)n[4]))
		return true;
	if (n.size() == 4 && n[0] == 'E' && isdigit((unsigned char)n[1]) && n[2] == 'M' && isdigit((unsigned char)n[3]))
		return true;
	return false;
}

static std::string ShortLumpName(const std::string &zipPath)
{
	std::string n = PathFileName(zipPath);
	size_t d = n.find('.');
	if (d != std::string::npos)
		n = n.substr(0, d);
	if (n.size() > 8)
		n = n.substr(0, 8);
	for (char &c : n)
		c = (char)toupper((unsigned char)c);
	return n;
}

static void ParseGameInfo(const std::string &text, ContentInfo &ci)
{
	Tokenizer tz(text);
	std::string tok, key;
	bool q;
	bool afterEq = false;
	std::vector<std::string> values;
	auto flush = [&]() {
		std::string k = ToLower(key);
		if (k == "iwad" && !values.empty()) ci.gameinfoIwad = values[0];
		else if (k == "load") ci.gameinfoLoad = values;
		else if (k == "startuptitle" && !values.empty()) ci.startupTitle = values[0];
		key.clear();
		values.clear();
		afterEq = false;
	};
	while (tz.Next(tok, q))
	{
		if (!q && tok == "=") { afterEq = true; continue; }
		if (!q && tok == ",") continue;
		if (afterEq)
		{
			values.push_back(tok);
			size_t save = tz.p;
			std::string t2;
			bool q2;
			if (tz.Next(t2, q2) && !q2 && t2 == ",")
				continue;
			tz.p = save;
			flush();
		}
		else
			key = tok;
	}
}

static std::string PrettyStem(const std::string &path)
{
	std::string s = PathStem(path);
	for (char &c : s)
		if (c == '_')
			c = ' ';
	return s;
}

ContentInfo InspectFile(const std::string &path, const std::vector<IwadDef> &defs)
{
	ContentInfo ci;
	ci.path = path;
	ci.fileName = PathFileName(path);
	ci.title = PrettyStem(path);
	if (!FileExists(path))
	{
		ci.status = FileStatus::Missing;
		ci.error = "file not found";
		return ci;
	}
	ci.size = FileSize(path);
	const std::string ext = PathExtLower(path);
	if (!IsSupportedIwadExtension(ext) && !IsSupportedModExtension(ext))
	{
		ci.status = FileStatus::Unsupported;
		ci.error = "the engine does not load " + (ext.empty() ? std::string("files without extension") : ext + " files");
		return ci;
	}
	if (ext == ".deh" || ext == ".bex")
	{
		std::string head(64, '\0');
		size_t n = ReadAt(path, 0, &head[0], head.size());
		head.resize(n);
		if (n == 0)
		{
			ci.status = FileStatus::Invalid;
			ci.error = "empty file";
			return ci;
		}
		ci.status = FileStatus::Ok;
		ci.hasDehacked = true;
		ci.category = ModCategory::Dehacked;
		return ci;
	}

	Archive a;
	if (!a.Open(path))
	{
		ci.status = FileStatus::Invalid;
		ci.error = a.Error();
		return ci;
	}
	ci.archive = a.Type();
	ci.status = FileStatus::Ok;
	if (a.Type() == ArchiveType::SevenZip)
	{
		// The engine reads 7z itself; its directory is not inspected here.
		ci.category = ModCategory::Unknown;
		return ci;
	}

	std::set<std::string> lumps;
	if (a.Type() == ArchiveType::Wad)
	{
		const auto &es = a.Entries();
		for (size_t i = 0; i < es.size(); i++)
		{
			const std::string &n = es[i].name;
			lumps.insert(n);
			if (IsMapMarker(n) && i + 1 < es.size() && (es[i + 1].name == "THINGS" || es[i + 1].name == "TEXTMAP"))
				ci.mapCount++;
		}
		ci.isIwad = a.IsIwadHeader();
	}
	else
	{
		for (const auto &e : a.Entries())
		{
			std::string lower = ToLower(e.name);
			lumps.insert(ShortLumpName(e.name));
			if (lower.compare(0, 5, "maps/") == 0 && PathExtLower(lower) == ".wad")
				ci.mapCount++;
			if (lower.find('/') == std::string::npos || lower.compare(0, 8, "zscript/") == 0)
			{
				std::string top = ShortLumpName(lower);
				if (top == "ZSCRIPT") ci.hasZScript = true;
				if (top == "DECORATE") ci.hasDecorate = true;
			}
		}
		ci.isIwad = a.Find("iwadinfo") != nullptr;
	}
	if (lumps.count("ZSCRIPT")) ci.hasZScript = true;
	if (lumps.count("DECORATE")) ci.hasDecorate = true;
	if (lumps.count("DEHACKED")) ci.hasDehacked = true;
	ci.hasMapinfo = lumps.count("MAPINFO") || lumps.count("ZMAPINFO") || lumps.count("UMAPINFO");
	ci.hasTitlePic = lumps.count("TITLEPIC") > 0;

	// GAMEINFO / IWADINFO text
	if (const ArchiveEntry *g = a.Find(a.Type() == ArchiveType::Wad ? "GAMEINFO" : "gameinfo"))
	{
		std::string t;
		if (a.Read(*g, t, 1 << 20))
			ParseGameInfo(t, ci);
	}
	if (a.Type() == ArchiveType::Zip && ci.isIwad)
	{
		std::string t;
		if (a.Read(*a.Find("iwadinfo"), t, 1 << 20))
		{
			auto own = ParseIwadInfo(t);
			if (!own.empty())
				ci.title = own[0].name;
		}
	}

	if (a.Type() == ArchiveType::Wad || !ci.isIwad)
	{
		// Same rule as the engine: first definition whose lumps are all present.
		for (size_t d = 0; d < defs.size(); d++)
		{
			const auto &mc = defs[d].mustContain;
			if (mc.empty())
				continue;
			bool all = true;
			for (const auto &m : mc)
				if (!lumps.count(ShortLumpName(m)))
				{
					all = false;
					break;
				}
			if (all)
			{
				ci.iwadDef = (int)d;
				if (ci.isIwad)
					ci.title = defs[d].name;
				break;
			}
		}
	}
	if (!ci.isIwad && !ci.startupTitle.empty())
		ci.title = ci.startupTitle;

	// WadFusion builds made for GZDoom / UZDoom 4.14 define the ID24 actors
	// that UZDoom 5 now ships, and the engine stops with "Class name ...
	// already exists" (same result on UZDoom 5.0.3 for PC).
	if (a.Type() == ArchiveType::Zip && a.Find("zscript/id24gore.zs") && a.Find("zscript/id24nature.zs"))
		ci.incompatible = "gerado para GZDoom/UZDoom 4.14 (atores ID24 duplicados); gere de novo com o WadFusion 1.6 ou mais novo, versão para UZDoom 5 (wadfusion_py.zip)";

	const bool code = ci.hasZScript || ci.hasDecorate || ci.hasDehacked;
	if (ci.mapCount > 0)
		ci.category = code ? ModCategory::GameplayAndMaps : ModCategory::MapPack;
	else
		ci.category = code ? ModCategory::Gameplay : ModCategory::Resources;
	return ci;
}

// ---------------------------------------------------------------------------
// title pictures
// ---------------------------------------------------------------------------

std::string ExtractPalette(const std::string &path)
{
	Archive a;
	if (!a.Open(path))
		return {};
	const ArchiveEntry *e = a.Find(a.Type() == ArchiveType::Wad ? "PLAYPAL" : "playpal");
	std::string data;
	if (!e || !a.Read(*e, data, 1 << 20) || data.size() < 768)
		return {};
	return data.substr(0, 768);
}

bool DecodeDoomPicture(const std::string &lump, const std::string &pal, std::vector<uint8_t> &rgba, int &w, int &h)
{
	if (lump.size() < 8 || pal.size() < 768)
		return false;
	const uint8_t *d = (const uint8_t *)lump.data();
	w = int16_t(d[0] | (d[1] << 8));
	h = int16_t(d[2] | (d[3] << 8));
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || lump.size() < 8 + (size_t)w * 4)
		return false;
	rgba.assign((size_t)w * h * 4, 0);
	for (int x = 0; x < w; x++)
	{
		const uint8_t *co = d + 8 + x * 4;
		uint32_t ofs = uint32_t(co[0]) | (uint32_t(co[1]) << 8) | (uint32_t(co[2]) << 16) | (uint32_t(co[3]) << 24);
		int lastTop = -1;
		while (ofs < lump.size())
		{
			int top = d[ofs];
			if (top == 0xFF)
				break;
			if (top <= lastTop)  // DeePsea tall patches
				top += lastTop;
			lastTop = top;
			if (ofs + 3 > lump.size())
				return false;
			int len = d[ofs + 1];
			if (ofs + 3 + len > lump.size())
				return false;
			for (int i = 0; i < len; i++)
			{
				int y = top + i;
				if (y < 0 || y >= h)
					continue;
				uint8_t idx = d[ofs + 3 + i];
				uint8_t *px = &rgba[((size_t)y * w + x) * 4];
				px[0] = (uint8_t)pal[idx * 3];
				px[1] = (uint8_t)pal[idx * 3 + 1];
				px[2] = (uint8_t)pal[idx * 3 + 2];
				px[3] = 255;
			}
			ofs += 4 + len;
		}
	}
	return true;
}

bool ExtractTitlePic(const std::string &path, const std::string &palette768, std::vector<uint8_t> &rgba, int &w, int &h)
{
	Archive a;
	if (!a.Open(path))
		return false;
	const ArchiveEntry *e = a.Type() == ArchiveType::Wad ? a.Find("TITLEPIC") : a.Find("graphics/titlepic");
	if (!e && a.Type() == ArchiveType::Zip)
	{
		// any TITLEPIC lump, e.g. in a namespace folder or an embedded path
		for (const auto &en : a.Entries())
		{
			std::string n = ToLower(PathFileName(en.name));
			size_t dot = n.find('.');
			if ((dot == std::string::npos ? n : n.substr(0, dot)) == "titlepic")
			{
				e = &en;
				break;
			}
		}
	}
	if (!e)
		return false;
	std::string data;
	if (!a.Read(*e, data, 4 << 20))
		return false;
	if (data.size() > 8 && !memcmp(data.data(), "\x89PNG", 4))
	{
		unsigned char *px = RdDecodeImage(data.data(), data.size(), &w, &h);
		if (!px)
			return false;
		rgba.assign(px, px + (size_t)w * h * 4);
		RdFreeImage(px);
		return true;
	}
	std::string pal = palette768;
	if (pal.size() < 768)
		pal = ExtractPalette(path);
	return DecodeDoomPicture(data, pal, rgba, w, h);
}

// ---------------------------------------------------------------------------
// CVARINFO clashes
// ---------------------------------------------------------------------------

std::vector<std::string> ParseCvarInfo(const std::string &text)
{
	// strip // and /* */ comments and string literals
	std::string t;
	t.reserve(text.size());
	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/')
		{
			while (i < text.size() && text[i] != '\n')
				i++;
			t += '\n';
		}
		else if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*')
		{
			i += 2;
			while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/'))
				i++;
			i++;
			t += ' ';
		}
		else if (text[i] == '"')
		{
			i++;
			while (i < text.size() && text[i] != '"')
				i++;
			t += " \"\" ";
		}
		else
			t += text[i];
	}
	std::vector<std::string> names;
	size_t start = 0;
	while (start < t.size())
	{
		size_t end = t.find(';', start);
		if (end == std::string::npos)
			end = t.size();
		// words of one declaration: [scope] [flags] type name [= value]
		std::vector<std::string> words;
		std::string w;
		for (size_t i = start; i <= end && i <= t.size(); i++)
		{
			const char ch = i < end ? t[i] : ' ';
			if (isspace((unsigned char)ch) || ch == '=')
			{
				if (!w.empty())
					words.push_back(ToLower(w));
				w.clear();
				if (ch == '=')
					break;
			}
			else
				w += ch;
		}
		for (size_t k = 0; k + 1 < words.size(); k++)
		{
			const std::string &ty = words[k];
			if (ty == "int" || ty == "float" || ty == "bool" || ty == "color" || ty == "string")
			{
				names.push_back(words[k + 1]);
				break;
			}
		}
		start = end + 1;
	}
	return names;
}

std::vector<std::string> ModCvarNames(const std::string &path)
{
	std::vector<std::string> names;
	Archive a;
	if (!a.Open(path))
		return names;
	for (const auto &e : a.Entries())
	{
		bool cvarinfo = false;
		if (a.Type() == ArchiveType::Wad)
			cvarinfo = e.name == "CVARINFO";
		else if (e.name.find('/') == std::string::npos)
		{
			const std::string base = ToLower(e.name.substr(0, e.name.find('.')));
			cvarinfo = base == "cvarinfo";
		}
		std::string text;
		if (cvarinfo && a.Read(e, text, 1u << 20))
			for (auto &n : ParseCvarInfo(text))
				names.push_back(n);
	}
	return names;
}

std::vector<std::string> FindCvarClashes(const std::vector<std::pair<std::string, std::string>> &mods)
{
	std::vector<std::vector<std::string>> cvars;
	for (const auto &m : mods)
		cvars.push_back(ModCvarNames(m.second));
	std::vector<std::string> out;
	for (size_t i = 0; i < mods.size(); i++)
		for (size_t j = i + 1; j < mods.size(); j++)
		{
			std::vector<std::string> both;
			for (const auto &n : cvars[j])
				if (std::find(cvars[i].begin(), cvars[i].end(), n) != cvars[i].end() &&
				    std::find(both.begin(), both.end(), n) == both.end())
					both.push_back(n);
			if (both.empty())
				continue;
			std::string list;
			for (size_t k = 0; k < both.size() && k < 3; k++)
				list += (k ? ", " : "") + both[k];
			if (both.size() > 3)
				list += ", +" + std::to_string(both.size() - 3);
			out.push_back(mods[i].first + " e " + mods[j].first + " declaram as mesmas variáveis (" + list + ")");
		}
	return out;
}

} // namespace rdoom
