// RDoom launcher - SD card layout.
//
//   <root>/                  sdmc:/switch/rdoom on the console
//     RDoom.nro, uzdoom.pk3, game_support.pk3, ... (engine files, names fixed by UZDoom)
//     soundfonts/ fm_banks/  (engine resources)
//     iwads/                 user IWADs (never shipped)
//     mods/                  user mods / map packs (sub-folders allowed)
//     profiles/              <id>.json, one per profile
//     config/                launcher.json (global settings)
//     config/engine/         <id>.ini, one UZDoom config per profile
//     saves/<id>/            save games per profile
//     screenshots/           engine screenshots
//     cache/                 engine cache; cache/thumbs/ launcher thumbnails
//     logs/                  rdoom.log (launcher), engine.log, crash logs
//     launcher/              launcher artwork (background, logo, card art)
//     backup/                copies made before migrations or imports
//     export/                exported profiles
#pragma once

#include <string>

namespace rdoom {

struct Paths
{
	std::string root;

	std::string Iwads() const;
	std::string Mods() const;
	std::string Profiles() const;
	std::string Config() const;
	std::string EngineConfigs() const;
	std::string Saves() const;
	std::string Screenshots() const;
	std::string Cache() const;
	std::string Thumbs() const;
	std::string Logs() const;
	std::string Art() const;
	std::string Backup() const;
	std::string Export() const;

	std::string LauncherConfigFile() const;
	std::string ProfileFile(const std::string &id) const;
	std::string EngineConfigFile(const std::string &id) const;
	std::string SaveDir(const std::string &id) const;

	// Creates every directory above. Returns false if the root is not writable.
	bool EnsureLayout() const;
};

#if defined(__SWITCH__)
constexpr const char *kDefaultRoot = "sdmc:/switch/rdoom";
#else
constexpr const char *kDefaultRoot = "./rdoom-sd";
#endif

} // namespace rdoom
