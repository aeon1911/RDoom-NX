#include "paths.h"
#include "fsutil.h"
#include "log.h"

namespace rdoom {

std::string Paths::Iwads() const { return PathJoin(root, "iwads"); }
std::string Paths::Mods() const { return PathJoin(root, "mods"); }
std::string Paths::Profiles() const { return PathJoin(root, "profiles"); }
std::string Paths::Config() const { return PathJoin(root, "config"); }
std::string Paths::EngineConfigs() const { return PathJoin(Config(), "engine"); }
std::string Paths::Saves() const { return PathJoin(root, "saves"); }
std::string Paths::Screenshots() const { return PathJoin(root, "screenshots"); }
std::string Paths::Cache() const { return PathJoin(root, "cache"); }
std::string Paths::Thumbs() const { return PathJoin(Cache(), "thumbs"); }
std::string Paths::Logs() const { return PathJoin(root, "logs"); }
std::string Paths::Art() const { return PathJoin(root, "launcher"); }
std::string Paths::Backup() const { return PathJoin(root, "backup"); }
std::string Paths::Export() const { return PathJoin(root, "export"); }

std::string Paths::LauncherConfigFile() const { return PathJoin(Config(), "launcher.json"); }
std::string Paths::ProfileFile(const std::string &id) const { return PathJoin(Profiles(), id + ".json"); }
std::string Paths::EngineConfigFile(const std::string &id) const { return PathJoin(EngineConfigs(), id + ".ini"); }
std::string Paths::SaveDir(const std::string &id) const { return PathJoin(Saves(), id); }

bool Paths::EnsureLayout() const
{
	bool ok = MakeDirs(root);
	for (const std::string &d : { Iwads(), Mods(), Profiles(), Config(), EngineConfigs(), Saves(), Screenshots(),
	                              Cache(), Thumbs(), Logs(), Art(), Backup(), Export() })
	{
		if (!MakeDirs(d))
		{
			RLOG_E("fs", "cannot create %s", d.c_str());
			ok = false;
		}
	}
	return ok;
}

} // namespace rdoom
