// RDoom launcher - turns a resolved profile into the engine command line.
//
// Nothing from a profile reaches a shell or the console unchecked: arguments
// are built from typed fields, and the only free-form part (extra cvars) is
// restricted to "+set <name> <value>" with plain names and values.
#pragma once

#include <string>
#include <vector>

#include "library.h"
#include "paths.h"
#include "profile.h"

namespace rdoom {

struct LaunchPlan
{
	std::vector<std::string> argv;  // argv[0] included
	std::string engineIni;
	std::string saveDir;
};

// The cvars a profile sets (quality preset, resolution, frame limit, input,
// gameplay, extras), in a stable order.
std::vector<std::pair<std::string, std::string>> ProfileCvars(const Profile &p);

LaunchPlan BuildLaunchPlan(const Profile &p, const ResolvedProfile &r, const Paths &paths);

// Sets key=value inside [section] of an INI text (adds the section or key if
// missing, leaves everything else byte-for-byte). Returns the new text.
std::string IniSet(const std::string &ini, const std::string &section, const std::string &key, const std::string &value);
// Reads a value from [section], empty if absent.
std::string IniGet(const std::string &ini, const std::string &section, const std::string &key);

// Writes the renderer choice into the profile's engine INI before start-up:
// UZDoom picks the backend when the video system starts, before command-line
// cvars are applied.
bool PrepareEngineIni(const Profile &p, const Paths &paths, std::string &err);

// Global launcher settings (config/launcher.json).
struct LauncherSettings
{
	int version = 1;
	std::string lastProfile;
	std::vector<std::string> extraIwadDirs;
	std::vector<std::string> extraModDirs;
	bool uiSounds = true;
	bool animations = true;
};

bool LoadLauncherSettings(const Paths &paths, LauncherSettings &s, std::string &warning);
bool SaveLauncherSettings(const Paths &paths, const LauncherSettings &s, std::string &err);

} // namespace rdoom
