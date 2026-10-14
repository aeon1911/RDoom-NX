#include "profile.h"
#include "fsutil.h"
#include "log.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>

#include "json.hpp"

using nlohmann::json;

namespace rdoom {

const char *RendererId(Renderer r)
{
	(void)r;
	return "vulkan";
}

const char *RendererLabel(Renderer r)
{
	(void)r;
	return "Vulkan";
}

bool ParseRenderer(const std::string &s, Renderer &out)
{
	std::string l = ToLower(s);
	if (l == "vulkan") { out = Renderer::Vulkan; return true; }
	return false;
}

bool IsRemovedRenderer(const std::string &s)
{
	std::string l = ToLower(s);
	return l == "opengl" || l == "gl" || l == "gles" || l == "opengles";
}

int RendererEngineValue(Renderer r)
{
	return (int)r;
}

bool IsValidProfileId(const std::string &id)
{
	if (id.empty() || id.size() > 48)
		return false;
	for (char c : id)
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
			return false;
	return id.front() != '-';
}

bool IsValidCvarName(const std::string &name)
{
	if (name.empty() || name.size() > 64)
		return false;
	if (!(isalpha((unsigned char)name[0]) || name[0] == '_'))
		return false;
	for (char c : name)
		if (!(isalnum((unsigned char)c) || c == '_'))
			return false;
	return true;
}

bool IsValidCvarValue(const std::string &value)
{
	// No spaces, quotes, ';' or '+': the value must stay a single argument and
	// can never start another console command.
	if (value.size() > 64)
		return false;
	for (char c : value)
		if (!(isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_'))
			return false;
	return true;
}

bool IsValidWarp(const std::string &warp)
{
	if (warp.empty())
		return true;
	if (warp.size() > 8)
		return false;
	for (char c : warp)
		if (!isalnum((unsigned char)c))
			return false;
	return true;
}

std::string MakeProfileId(const std::string &name)
{
	std::string id;
	for (unsigned char c : name)
	{
		if (isalnum(c))
			id += (char)tolower(c);
		else if (!id.empty() && id.back() != '-')
			id += '-';
		// non-ASCII bytes (accents, dashes) are dropped
	}
	while (!id.empty() && id.back() == '-')
		id.pop_back();
	if (id.size() > 40)
		id.resize(40);
	if (id.empty())
		id = "perfil";
	return id;
}

std::string NowIso8601()
{
	char buf[32];
	time_t now = time(nullptr);
	struct tm tmv;
	gmtime_r(&now, &tmv);
	strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
	return buf;
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

std::string ProfileToJson(const Profile &p)
{
	json j;
	j["format"] = "rdoom-profile";
	j["version"] = kProfileFormatVersion;
	j["id"] = p.id;
	j["name"] = p.name;
	j["description"] = p.description;
	j["template"] = p.templateId;
	j["iwad"] = p.iwad;
	j["iwad_candidates"] = p.iwadCandidates;
	json mods = json::array();
	for (const auto &m : p.mods)
		mods.push_back({ { "file", m.file }, { "enabled", m.enabled }, { "optional", m.optional } });
	j["mods"] = mods;
	j["mod_patterns"] = p.modPatterns;
	j["video"] = {
		{ "renderer", RendererId(p.video.renderer) },
		{ "internal_scale", p.video.internalScale },
		{ "fps_limit", p.video.fpsLimit },
		{ "vsync", p.video.vsync },
		{ "quality", p.video.quality },
		{ "dynamic_lights", p.video.dynamicLights },
		{ "texture_filter", p.video.textureFilter },
	};
	j["input"] = {
		{ "look_sensitivity", p.input.lookSensitivity },
		{ "deadzone", p.input.deadzone },
		{ "invert_look", p.input.invertLook },
	};
	j["gameplay"] = {
		{ "autoaim", p.gameplay.autoaim },
		{ "always_run", p.gameplay.alwaysRun },
		{ "crosshair", p.gameplay.crosshair },
		{ "skill", p.gameplay.skill },
		{ "warp", p.gameplay.warp },
		{ "fast", p.gameplay.fast },
		{ "no_monsters", p.gameplay.noMonsters },
		{ "respawn", p.gameplay.respawn },
	};
	j["cvars"] = p.cvars;
	j["created"] = p.created;
	j["modified"] = p.modified;
	j["app_version"] = p.appVersion;
	return j.dump(2) + "\n";
}

namespace {

struct Reader
{
	std::vector<std::string> &warnings;

	template <class T>
	void Get(const json &obj, const char *key, T &out, const char *where)
	{
		auto it = obj.find(key);
		if (it == obj.end())
		{
			warnings.push_back(std::string(where) + "." + key + " missing, default used");
			return;
		}
		try
		{
			out = it->get<T>();
		}
		catch (const std::exception &)
		{
			warnings.push_back(std::string(where) + "." + key + " has the wrong type, default used");
		}
	}
};

template <class T>
T Clamp(T v, T lo, T hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

bool ProfileFromJson(const std::string &text, Profile &p, std::vector<std::string> &warnings, std::string &error)
{
	json j;
	try
	{
		j = json::parse(text);
	}
	catch (const std::exception &e)
	{
		error = std::string("invalid JSON: ") + e.what();
		return false;
	}
	if (!j.is_object())
	{
		error = "profile is not a JSON object";
		return false;
	}
	Reader r{ warnings };
	p.version = 0;  // files without a version field are pre-release / legacy
	if (j.contains("version") && j["version"].is_number_integer())
		p.version = j["version"].get<int>();
	r.Get(j, "id", p.id, "profile");
	r.Get(j, "name", p.name, "profile");
	r.Get(j, "description", p.description, "profile");
	r.Get(j, "template", p.templateId, "profile");
	r.Get(j, "iwad", p.iwad, "profile");
	if (j.contains("iwad_candidates"))
		r.Get(j, "iwad_candidates", p.iwadCandidates, "profile");
	if (j.contains("mod_patterns"))
		r.Get(j, "mod_patterns", p.modPatterns, "profile");
	p.mods.clear();
	if (j.contains("mods") && j["mods"].is_array())
	{
		for (const auto &m : j["mods"])
		{
			ModRef ref;
			if (m.is_string())
				ref.file = m.get<std::string>();
			else if (m.is_object())
			{
				ref.file = m.value("file", std::string());
				ref.enabled = m.value("enabled", true);
				ref.optional = m.value("optional", false);
			}
			if (!ref.file.empty())
				p.mods.push_back(ref);
			else
				warnings.push_back("profile.mods: entry without a file ignored");
		}
	}
	if (j.contains("video") && j["video"].is_object())
	{
		const json &v = j["video"];
		std::string rend = "vulkan";
		r.Get(v, "renderer", rend, "video");
		if (!ParseRenderer(rend, p.video.renderer))
		{
			warnings.push_back(IsRemovedRenderer(rend) ? "video.renderer '" + rend + "': OpenGL was removed from RDoom, Vulkan used"
				: "video.renderer '" + rend + "' unknown, Vulkan used");
			p.video.renderer = Renderer::Vulkan;
		}
		r.Get(v, "internal_scale", p.video.internalScale, "video");
		r.Get(v, "fps_limit", p.video.fpsLimit, "video");
		r.Get(v, "vsync", p.video.vsync, "video");
		r.Get(v, "quality", p.video.quality, "video");
		r.Get(v, "dynamic_lights", p.video.dynamicLights, "video");
		r.Get(v, "texture_filter", p.video.textureFilter, "video");
	}
	if (j.contains("input") && j["input"].is_object())
	{
		const json &v = j["input"];
		r.Get(v, "look_sensitivity", p.input.lookSensitivity, "input");
		r.Get(v, "deadzone", p.input.deadzone, "input");
		r.Get(v, "invert_look", p.input.invertLook, "input");
	}
	if (j.contains("gameplay") && j["gameplay"].is_object())
	{
		const json &v = j["gameplay"];
		r.Get(v, "autoaim", p.gameplay.autoaim, "gameplay");
		r.Get(v, "always_run", p.gameplay.alwaysRun, "gameplay");
		r.Get(v, "crosshair", p.gameplay.crosshair, "gameplay");
		r.Get(v, "skill", p.gameplay.skill, "gameplay");
		r.Get(v, "warp", p.gameplay.warp, "gameplay");
		r.Get(v, "fast", p.gameplay.fast, "gameplay");
		r.Get(v, "no_monsters", p.gameplay.noMonsters, "gameplay");
		r.Get(v, "respawn", p.gameplay.respawn, "gameplay");
	}
	p.cvars.clear();
	if (j.contains("cvars") && j["cvars"].is_object())
	{
		for (auto it = j["cvars"].begin(); it != j["cvars"].end(); ++it)
		{
			std::string val = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
			if (IsValidCvarName(it.key()) && IsValidCvarValue(val))
				p.cvars[it.key()] = val;
			else
				warnings.push_back("cvars." + it.key() + " rejected (only plain names and values are allowed)");
		}
	}
	r.Get(j, "created", p.created, "profile");
	r.Get(j, "modified", p.modified, "profile");
	r.Get(j, "app_version", p.appVersion, "profile");

	// range checks
	p.video.internalScale = Clamp(p.video.internalScale, 0.5f, 1.0f);
	if (p.video.fpsLimit != 0)
		p.video.fpsLimit = Clamp(p.video.fpsLimit, 20, 60);
	if (p.video.quality != "low" && p.video.quality != "stock" && p.video.quality != "high")
	{
		warnings.push_back("video.quality '" + p.video.quality + "' unknown, stock used");
		p.video.quality = "stock";
	}
	p.video.textureFilter = Clamp(p.video.textureFilter, 0, 6);
	p.input.lookSensitivity = Clamp(p.input.lookSensitivity, 0.1f, 4.0f);
	p.input.deadzone = Clamp(p.input.deadzone, 0.0f, 0.6f);
	p.gameplay.autoaim = Clamp(p.gameplay.autoaim, 0.0f, 35.0f);
	p.gameplay.crosshair = Clamp(p.gameplay.crosshair, 0, 8);
	p.gameplay.skill = Clamp(p.gameplay.skill, 0, 5);
	if (!IsValidWarp(p.gameplay.warp))
	{
		warnings.push_back("gameplay.warp '" + p.gameplay.warp + "' rejected");
		p.gameplay.warp.clear();
	}
	return true;
}

// ---------------------------------------------------------------------------
// templates
// ---------------------------------------------------------------------------

std::vector<Profile> BuiltinTemplates()
{
	std::vector<Profile> t;
	{
		Profile p;
		p.id = "freedoom2-vulkan-stock";
		p.templateId = "freedoom2";
		p.name = "FreeDoom 2 — Vulkan Stock";
		p.description = "Freedoom: Phase 2 com Vulkan e a configuração RDoom — Stock Switch.";
		p.iwadCandidates = { "freedoom2.wad" };
		t.push_back(p);
	}
	{
		Profile p;
		p.id = "brutal-doom";
		p.templateId = "brutaldoom";
		p.name = "Brutal Doom";
		p.description = "Brutal Doom (pk3) sobre DOOM II. Requer o arquivo do mod em mods/ e a IWAD.";
		p.iwadCandidates = { "doom2.wad", "doom.wad", "freedoom2.wad" };
		p.modPatterns = { "brutal*.pk3" };
		t.push_back(p);
	}
	{
		Profile p;
		p.id = "wadfusion";
		p.templateId = "wadfusion";
		p.name = "WadFusion";
		p.description = "WadFusion (doom_fusion.ipk3 gerado pelo usuário) como IWAD.";
		p.iwadCandidates = { "doom_fusion.ipk3" };
		p.modPatterns = { "?doom_fusion_widescreen_gfx.pk3" };
		t.push_back(p);
	}
	{
		Profile p;
		p.id = "lexicon";
		p.templateId = "lexicon";
		p.name = "Lexicon";
		p.description = "Lexicon: núcleo e pacote de mapas, sobre DOOM II.";
		p.iwadCandidates = { "doom2.wad" };
		p.modPatterns = { "lexicon-core*.pk3", "lexicon-base*.pk3" };
		t.push_back(p);
	}
	{
		Profile p;
		p.id = "compendium";
		p.templateId = "compendium";
		p.name = "Compendium";
		p.description = "The Compendium: coleção de megawads com seletor, sobre DOOM II.";
		p.iwadCandidates = { "doom2.wad" };
		p.modPatterns = { "compendium*.pk3", "?compen-play*.pk3" };
		t.push_back(p);
	}
	{
		Profile p;
		p.id = "personalizado";
		p.templateId = "custom";
		p.name = "Configuração personalizada";
		p.description = "Escolha a IWAD e os mods livremente.";
		t.push_back(p);
	}
	return t;
}

Profile MakeDefaultProfile(const std::string &templateId)
{
	for (auto &p : BuiltinTemplates())
		if (p.templateId == templateId)
			return p;
	return BuiltinTemplates().back();
}

// ---------------------------------------------------------------------------
// store
// ---------------------------------------------------------------------------

ProfileStore::ProfileStore(Paths paths, std::string appVersion) : paths_(std::move(paths)), appVersion_(std::move(appVersion))
{
}

static std::string BackupName(const Paths &paths, const std::string &file, const char *why)
{
	char stamp[32];
	time_t now = time(nullptr);
	struct tm tmv;
	gmtime_r(&now, &tmv);
	strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tmv);
	return PathJoin(paths.Backup(), PathStem(file) + "." + why + "-" + stamp + ".json");
}

bool ProfileStore::Migrate(Profile &p, const std::string &file)
{
	if (p.version == kProfileFormatVersion)
		return true;
	if (p.version > kProfileFormatVersion)
	{
		problems_.push_back(PathFileName(file) + ": formato v" + std::to_string(p.version) +
		                    " é mais novo que este RDoom; aberto somente para leitura.");
		return false;
	}
	// v0 -> v1: same fields; the file is rewritten with the version stamp.
	std::string backup = BackupName(paths_, file, "pre-migration");
	if (!CopyFile(file, backup))
	{
		problems_.push_back(PathFileName(file) + ": não foi possível fazer backup antes da migração.");
		return false;
	}
	RLOG_I("profiles", "migrating %s from v%d (backup %s)", file.c_str(), p.version, backup.c_str());
	p.version = kProfileFormatVersion;
	std::string err;
	return Save(p, err);
}

void ProfileStore::Load()
{
	profiles_.clear();
	problems_.clear();
	for (const auto &e : ListDir(paths_.Profiles()))
	{
		if (e.isDir || PathExtLower(e.name) != ".json")
			continue;
		std::string text, error;
		std::vector<std::string> warnings;
		Profile p;
		if (!ReadFile(e.path, text) || !ProfileFromJson(text, p, warnings, error))
		{
			if (error.empty())
				error = "unreadable";
			std::string backup = BackupName(paths_, e.path, "corrupt");
			CopyFile(e.path, backup);
			RLOG_E("profiles", "%s: %s (copy kept in %s)", e.name.c_str(), error.c_str(), backup.c_str());
			problems_.push_back(e.name + ": arquivo inválido (" + error + "). Cópia em backup/.");
			continue;
		}
		for (const auto &w : warnings)
			RLOG_W("profiles", "%s: %s", e.name.c_str(), w.c_str());
		// the file name is authoritative for the id
		p.id = PathStem(e.name);
		if (!IsValidProfileId(p.id))
		{
			problems_.push_back(e.name + ": nome de arquivo inválido para um perfil.");
			continue;
		}
		if (p.name.empty())
			p.name = p.id;
		Migrate(p, e.path);
		profiles_.push_back(std::move(p));
	}
	if (profiles_.empty() && problems_.empty())
	{
		RLOG_I("profiles", "first run: creating the built-in profiles");
		for (auto &t : BuiltinTemplates())
		{
			std::string err;
			t.created = NowIso8601();
			if (Save(t, err))
				profiles_.push_back(t);
			else
				problems_.push_back(t.name + ": " + err);
		}
	}
	// creation order; the built-in profiles (same second) keep their own order
	auto rank = [](const Profile &p) {
		static const char *order[] = { "freedoom2", "brutaldoom", "wadfusion", "lexicon", "compendium", "custom" };
		for (int i = 0; i < 6; i++)
			if (p.templateId == order[i])
				return i;
		return 6;
	};
	std::stable_sort(profiles_.begin(), profiles_.end(), [&](const Profile &a, const Profile &b) {
		if (a.created != b.created)
			return a.created < b.created;
		return rank(a) < rank(b);
	});
}

const Profile *ProfileStore::Find(const std::string &id) const
{
	for (const auto &p : profiles_)
		if (p.id == id)
			return &p;
	return nullptr;
}

Profile *ProfileStore::FindMutable(const std::string &id)
{
	for (auto &p : profiles_)
		if (p.id == id)
			return &p;
	return nullptr;
}

std::string ProfileStore::UniqueId(const std::string &base) const
{
	std::string b = IsValidProfileId(base) ? base : MakeProfileId(base);
	std::string id = b;
	for (int n = 2; Find(id) || FileExists(paths_.ProfileFile(id)); n++)
		id = b + "-" + std::to_string(n);
	return id;
}

bool ProfileStore::Save(Profile &p, std::string &err)
{
	if (!IsValidProfileId(p.id))
	{
		err = "identificador de perfil inválido";
		return false;
	}
	p.version = kProfileFormatVersion;
	p.modified = NowIso8601();
	if (p.created.empty())
		p.created = p.modified;
	p.appVersion = appVersion_;
	WriteResult r = WriteFileAtomic(paths_.ProfileFile(p.id), ProfileToJson(p));
	if (r != WriteResult::Ok)
	{
		err = WriteResultName(r);
		return false;
	}
	if (Profile *existing = FindMutable(p.id))
		if (existing != &p)
			*existing = p;
	return true;
}

bool ProfileStore::Create(const std::string &templateId, const std::string &name, Profile &out, std::string &err)
{
	Profile p = MakeDefaultProfile(templateId);
	if (!name.empty())
		p.name = name;
	p.id = UniqueId(MakeProfileId(p.name));
	p.created.clear();
	if (!Save(p, err))
		return false;
	profiles_.push_back(p);
	out = p;
	return true;
}

bool ProfileStore::Duplicate(const std::string &id, Profile &out, std::string &err)
{
	const Profile *src = Find(id);
	if (!src)
	{
		err = "perfil não encontrado";
		return false;
	}
	Profile p = *src;
	p.name = src->name + " (cópia)";
	p.id = UniqueId(src->id);
	p.created.clear();
	if (!Save(p, err))
		return false;
	// the copy starts from the original's engine settings
	if (FileExists(paths_.EngineConfigFile(src->id)))
		CopyFile(paths_.EngineConfigFile(src->id), paths_.EngineConfigFile(p.id));
	profiles_.push_back(p);
	out = p;
	return true;
}

bool ProfileStore::Rename(const std::string &id, const std::string &newName, std::string &err)
{
	Profile *p = FindMutable(id);
	if (!p)
	{
		err = "perfil não encontrado";
		return false;
	}
	if (newName.empty() || newName.size() > 64)
	{
		err = "nome vazio ou longo demais";
		return false;
	}
	Profile copy = *p;
	copy.name = newName;
	if (!Save(copy, err))
		return false;
	*p = copy;
	return true;
}

bool ProfileStore::Remove(const std::string &id, std::string &err)
{
	auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const Profile &p) { return p.id == id; });
	if (it == profiles_.end())
	{
		err = "perfil não encontrado";
		return false;
	}
	const std::string file = paths_.ProfileFile(id);
	// keep a copy: deleting a profile must be recoverable
	CopyFile(file, BackupName(paths_, file, "deleted"));
	if (FileExists(file) && !RemoveFile(file))
	{
		err = "não foi possível apagar o arquivo do perfil";
		return false;
	}
	RemoveFile(paths_.EngineConfigFile(id));
	profiles_.erase(it);
	return true;
}

bool ProfileStore::RestoreDefaults(const std::string &id, std::string &err)
{
	Profile *p = FindMutable(id);
	if (!p)
	{
		err = "perfil não encontrado";
		return false;
	}
	CopyFile(paths_.ProfileFile(id), BackupName(paths_, paths_.ProfileFile(id), "before-reset"));
	Profile def = MakeDefaultProfile(p->templateId.empty() ? "custom" : p->templateId);
	// keep identity and the user's file choices; reset every option
	def.id = p->id;
	def.name = p->name;
	def.created = p->created;
	def.iwad = p->iwad;
	def.mods = p->mods;
	if (!Save(def, err))
		return false;
	*p = def;
	return true;
}

bool ProfileStore::Export(const std::string &id, std::string &outPath, std::string &err)
{
	const Profile *p = Find(id);
	if (!p)
	{
		err = "perfil não encontrado";
		return false;
	}
	outPath = PathJoin(paths_.Export(), id + ".rdoom-profile.json");
	WriteResult r = WriteFileAtomic(outPath, ProfileToJson(*p));
	if (r != WriteResult::Ok)
	{
		err = WriteResultName(r);
		return false;
	}
	return true;
}

bool ProfileStore::Import(const std::string &path, Profile &out, std::string &err)
{
	std::string text, error;
	std::vector<std::string> warnings;
	Profile p;
	if (!ReadFile(path, text))
	{
		err = "não foi possível ler " + PathFileName(path);
		return false;
	}
	if (!ProfileFromJson(text, p, warnings, error))
	{
		err = error;
		return false;
	}
	for (const auto &w : warnings)
		RLOG_W("profiles", "import %s: %s", path.c_str(), w.c_str());
	std::string base = p.id;
	if (!IsValidProfileId(base))
		base = MakeProfileId(p.name.empty() ? PathStem(path) : p.name);
	p.id = UniqueId(base);
	if (p.name.empty())
		p.name = p.id;
	p.created.clear();
	if (!Save(p, err))
		return false;
	profiles_.push_back(p);
	out = p;
	return true;
}

} // namespace rdoom
