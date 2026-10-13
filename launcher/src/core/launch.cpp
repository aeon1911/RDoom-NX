#include "launch.h"
#include "fsutil.h"
#include "log.h"

#include <cmath>
#include <cstdio>

#include "json.hpp"

using nlohmann::json;

namespace rdoom {

static std::string Num(float v)
{
	char b[32];
	snprintf(b, sizeof(b), "%.3g", v);
	return b;
}

std::vector<std::pair<std::string, std::string>> ProfileCvars(const Profile &p)
{
	std::vector<std::pair<std::string, std::string>> c;
	const VideoOptions &v = p.video;

	// "RDoom - Stock Switch": no full-screen post effects, single sample,
	// dynamic lights as chosen; "low" also drops lights and caps the scale.
	float scale = v.internalScale;
	bool lights = v.dynamicLights;
	if (v.quality == "low")
	{
		scale = std::min(scale, 0.75f);
		lights = false;
	}
	c.push_back({ "vid_scalemode", "0" });
	c.push_back({ "vid_scalefactor", Num(scale) });
	c.push_back({ "vid_maxfps", std::to_string(v.fpsLimit) });
	c.push_back({ "vid_vsync", v.vsync ? "1" : "0" });
	c.push_back({ "r_dynlights", lights ? "1" : "0" });
	c.push_back({ "gl_texture_filter", std::to_string(v.textureFilter) });
	c.push_back({ "gl_multisample", "1" });
	c.push_back({ "gl_fxaa", "0" });
	c.push_back({ "gl_ssao", "0" });
	c.push_back({ "gl_lens", "0" });
	c.push_back({ "gl_tonemap", "0" });
	c.push_back({ "gl_bloom", v.quality == "high" ? "1" : "0" });
	c.push_back({ "gl_light_shadowmap", v.quality == "high" ? "1" : "0" });
	c.push_back({ "gl_texture_filter_anisotropic", v.quality == "high" ? "4" : "1" });

	c.push_back({ "rd_joy_sensitivity", Num(p.input.lookSensitivity) });
	c.push_back({ "rd_joy_deadzone", Num(p.input.deadzone) });
	c.push_back({ "rd_joy_invert_look", p.input.invertLook ? "1" : "0" });

	c.push_back({ "autoaim", Num(p.gameplay.autoaim) });
	c.push_back({ "cl_run", p.gameplay.alwaysRun ? "1" : "0" });
	c.push_back({ "crosshair", std::to_string(p.gameplay.crosshair) });

	for (const auto &kv : p.cvars)
		if (IsValidCvarName(kv.first) && IsValidCvarValue(kv.second))
			c.push_back(kv);
	return c;
}

LaunchPlan BuildLaunchPlan(const Profile &p, const ResolvedProfile &r, const Paths &paths)
{
	LaunchPlan plan;
	plan.engineIni = paths.EngineConfigFile(p.id);
	plan.saveDir = paths.SaveDir(p.id);
	auto &a = plan.argv;
	a.push_back("RDoom");
	if (r.iwad)
	{
		a.push_back("-iwad");
		a.push_back(r.iwad->info.path);
	}
	std::vector<std::string> files, dehs;
	for (const LibraryItem *m : r.mods)
	{
		const std::string ext = PathExtLower(m->info.path);
		if (ext == ".deh" || ext == ".bex")
			dehs.push_back(m->info.path);
		else
			files.push_back(m->info.path);
	}
	if (!files.empty())
	{
		a.push_back("-file");
		for (const auto &f : files)
			a.push_back(f);
	}
	for (const auto &d : dehs)
	{
		a.push_back(PathExtLower(d) == ".bex" ? "-bex" : "-deh");
		a.push_back(d);
	}
	a.push_back("-config");
	a.push_back(plan.engineIni);
	a.push_back("-savedir");
	a.push_back(plan.saveDir);
	if (p.gameplay.skill > 0)
	{
		a.push_back("-skill");
		a.push_back(std::to_string(p.gameplay.skill));
	}
	if (p.gameplay.fast) a.push_back("-fast");
	if (p.gameplay.noMonsters) a.push_back("-nomonsters");
	if (p.gameplay.respawn) a.push_back("-respawn");
	for (const auto &kv : ProfileCvars(p))
	{
		a.push_back("+set");
		a.push_back(kv.first);
		a.push_back(kv.second);
	}
	if (!p.gameplay.warp.empty() && IsValidWarp(p.gameplay.warp))
	{
		a.push_back("+map");
		a.push_back(p.gameplay.warp);
	}
	return plan;
}

// ---------------------------------------------------------------------------
// INI
// ---------------------------------------------------------------------------

static std::string Trim(const std::string &s)
{
	size_t b = s.find_first_not_of(" \t\r");
	size_t e = s.find_last_not_of(" \t\r");
	return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}

std::string IniSet(const std::string &ini, const std::string &section, const std::string &key, const std::string &value)
{
	std::vector<std::string> lines;
	size_t start = 0;
	while (start < ini.size())
	{
		size_t nl = ini.find('\n', start);
		lines.push_back(ini.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
		if (nl == std::string::npos)
			break;
		start = nl + 1;
	}
	const std::string header = "[" + section + "]";
	int secStart = -1, secEnd = (int)lines.size();
	for (int i = 0; i < (int)lines.size(); i++)
	{
		std::string t = Trim(lines[i]);
		if (secStart < 0)
		{
			if (ToLower(t) == ToLower(header))
				secStart = i;
		}
		else if (!t.empty() && t[0] == '[')
		{
			secEnd = i;
			break;
		}
	}
	const std::string kv = key + "=" + value;
	if (secStart < 0)
	{
		if (!lines.empty() && !Trim(lines.back()).empty())
			lines.push_back("");
		lines.push_back(header);
		lines.push_back(kv);
	}
	else
	{
		bool done = false;
		for (int i = secStart + 1; i < secEnd; i++)
		{
			std::string t = Trim(lines[i]);
			size_t eq = t.find('=');
			if (eq != std::string::npos && ToLower(Trim(t.substr(0, eq))) == ToLower(key))
			{
				lines[i] = kv;
				done = true;
				break;
			}
		}
		if (!done)
		{
			// insert after the last non-empty line of the section
			int at = secEnd;
			while (at - 1 > secStart && Trim(lines[at - 1]).empty())
				at--;
			lines.insert(lines.begin() + at, kv);
		}
	}
	std::string out;
	for (size_t i = 0; i < lines.size(); i++)
	{
		out += lines[i];
		if (i + 1 < lines.size() || (!ini.empty() && ini.back() == '\n') || ini.empty())
			out += "\n";
	}
	return out;
}

std::string IniGet(const std::string &ini, const std::string &section, const std::string &key)
{
	bool in = false;
	size_t start = 0;
	while (start <= ini.size())
	{
		size_t nl = ini.find('\n', start);
		std::string t = Trim(ini.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
		if (!t.empty() && t[0] == '[')
			in = ToLower(t) == ToLower("[" + section + "]");
		else if (in)
		{
			size_t eq = t.find('=');
			if (eq != std::string::npos && ToLower(Trim(t.substr(0, eq))) == ToLower(key))
				return Trim(t.substr(eq + 1));
		}
		if (nl == std::string::npos)
			break;
		start = nl + 1;
	}
	return std::string();
}

bool PrepareEngineIni(const Profile &p, const Paths &paths, std::string &err)
{
	const std::string file = paths.EngineConfigFile(p.id);
	std::string ini;
	ReadFile(file, ini);  // a missing file is fine: the engine fills in the rest
	const std::string want = std::to_string(RendererEngineValue(p.video.renderer));
	if (IniGet(ini, "GlobalSettings", "vid_preferbackend") == want && !ini.empty())
		return true;
	std::string out = IniSet(ini, "GlobalSettings", "vid_preferbackend", want);
	WriteResult r = WriteFileAtomic(file, out);
	if (r != WriteResult::Ok)
	{
		err = std::string("config do motor: ") + WriteResultName(r);
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// launcher.json
// ---------------------------------------------------------------------------

bool LoadLauncherSettings(const Paths &paths, LauncherSettings &s, std::string &warning)
{
	s = LauncherSettings();
	std::string text;
	if (!ReadFile(paths.LauncherConfigFile(), text))
		return true;  // first run
	try
	{
		json j = json::parse(text);
		s.version = j.value("version", 1);
		s.lastProfile = j.value("last_profile", std::string());
		if (j.contains("extra_iwad_dirs") && j["extra_iwad_dirs"].is_array())
			s.extraIwadDirs = j["extra_iwad_dirs"].get<std::vector<std::string>>();
		if (j.contains("extra_mod_dirs") && j["extra_mod_dirs"].is_array())
			s.extraModDirs = j["extra_mod_dirs"].get<std::vector<std::string>>();
		s.uiSounds = j.value("ui_sounds", true);
		s.animations = j.value("animations", true);
	}
	catch (const std::exception &e)
	{
		const std::string backup = PathJoin(paths.Backup(), "launcher.corrupt.json");
		CopyFile(paths.LauncherConfigFile(), backup);
		warning = std::string("launcher.json inválido (") + e.what() + "); padrões restaurados, cópia em backup/";
		RLOG_E("settings", "%s", warning.c_str());
		s = LauncherSettings();
		return false;
	}
	return true;
}

bool SaveLauncherSettings(const Paths &paths, const LauncherSettings &s, std::string &err)
{
	json j;
	j["format"] = "rdoom-launcher";
	j["version"] = s.version;
	j["last_profile"] = s.lastProfile;
	j["extra_iwad_dirs"] = s.extraIwadDirs;
	j["extra_mod_dirs"] = s.extraModDirs;
	j["ui_sounds"] = s.uiSounds;
	j["animations"] = s.animations;
	WriteResult r = WriteFileAtomic(paths.LauncherConfigFile(), j.dump(2) + "\n");
	if (r != WriteResult::Ok)
	{
		err = WriteResultName(r);
		return false;
	}
	return true;
}

} // namespace rdoom
