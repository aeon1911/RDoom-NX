// RDoom launcher - game profiles.
//
// A profile is one independent game setup: IWAD, ordered mod list, renderer,
// resolution, frame limit, input and gameplay options, plus its own engine
// config file and save directory. Stored as versioned JSON, one file per
// profile in <root>/profiles/<id>.json. Format: docs/PROFILES.md.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "paths.h"

namespace rdoom {

constexpr int kProfileFormatVersion = 1;

// RDoom renders with Vulkan (Mesa NVK) only. Profiles written by early
// builds may still say "opengl" or "gles": they load as Vulkan.
enum class Renderer { Vulkan = 1 };
const char *RendererId(Renderer r);        // "vulkan"
const char *RendererLabel(Renderer r);     // "Vulkan"
bool ParseRenderer(const std::string &s, Renderer &out);
bool IsRemovedRenderer(const std::string &s);	// "opengl", "gles", ...
int RendererEngineValue(Renderer r);       // vid_preferbackend value

struct ModRef
{
	std::string file;       // relative to a mod library folder, or absolute
	bool enabled = true;
	bool optional = false;  // template entries the profile can run without
};

struct VideoOptions
{
	Renderer renderer = Renderer::Vulkan;
	float internalScale = 1.0f;     // vid_scalefactor (0.5 .. 1.0)
	int fpsLimit = 60;              // vid_maxfps; 0 = no limit
	bool vsync = true;
	std::string quality = "stock";  // "low", "stock", "high"
	bool dynamicLights = true;
	int textureFilter = 0;          // gl_texture_filter (0 = none)
};

struct InputOptions
{
	float lookSensitivity = 1.0f;   // rd_joy_sensitivity
	float deadzone = 0.15f;         // rd_joy_deadzone
	bool invertLook = false;        // rd_joy_invert_look
};

struct GameplayOptions
{
	float autoaim = 35.0f;          // autoaim (degrees, 0 = off)
	bool alwaysRun = true;          // cl_run
	int crosshair = 1;              // crosshair (0 = off)
	int skill = 0;                  // -skill (0 = ask in the menu)
	std::string warp;               // -warp / +map target, empty = title screen
	bool fast = false, noMonsters = false, respawn = false;
};

struct Profile
{
	int version = kProfileFormatVersion;
	std::string id;                 // file name stem: [a-z0-9-]
	std::string name;
	std::string description;
	std::string templateId;         // "freedoom2", "brutaldoom", "wadfusion", "lexicon", "compendium", "custom"
	std::string iwad;               // file name in iwads/ (or absolute path)
	std::vector<std::string> iwadCandidates;  // template: acceptable IWAD files, in preference order
	std::vector<ModRef> mods;       // load order
	std::vector<std::string> modPatterns;     // template: mod files to look for ("brutal*.pk3")
	VideoOptions video;
	InputOptions input;
	GameplayOptions gameplay;
	std::map<std::string, std::string> cvars;  // extra "+set name value" pairs (validated)
	std::string created;            // ISO 8601, UTC
	std::string modified;
	std::string appVersion;         // RDoom version that last wrote the file
};

// Validation of user-editable fields shared by load, import and launch.
bool IsValidProfileId(const std::string &id);
bool IsValidCvarName(const std::string &name);
bool IsValidCvarValue(const std::string &value);
bool IsValidWarp(const std::string &warp);
std::string MakeProfileId(const std::string &name);  // slug, not unique

// JSON (nlohmann) <-> Profile. FromJson never throws: missing or wrong-typed
// fields keep their defaults and are reported in warnings.
std::string ProfileToJson(const Profile &p);
bool ProfileFromJson(const std::string &text, Profile &out, std::vector<std::string> &warnings, std::string &error);

// Built-in starting points. All but "custom" are templates whose files the
// user has to install; ResolveTemplate fills in what is present.
std::vector<Profile> BuiltinTemplates();
Profile MakeDefaultProfile(const std::string &templateId);

std::string NowIso8601();

class ProfileStore
{
public:
	explicit ProfileStore(Paths paths, std::string appVersion);

	// Loads every profiles/*.json. Corrupt files are copied to backup/ and
	// reported, never deleted. Creates the templates on first run.
	void Load();
	const std::vector<Profile> &All() const { return profiles_; }
	const std::vector<std::string> &Problems() const { return problems_; }

	const Profile *Find(const std::string &id) const;
	Profile *FindMutable(const std::string &id);

	// Each of these writes the profile file (atomically) and returns false with
	// a reason on failure. Mod and IWAD files are never touched.
	bool Save(Profile &p, std::string &err);
	bool Create(const std::string &templateId, const std::string &name, Profile &out, std::string &err);
	bool Duplicate(const std::string &id, Profile &out, std::string &err);
	bool Rename(const std::string &id, const std::string &newName, std::string &err);
	bool Remove(const std::string &id, std::string &err);    // profile file + engine ini; saves stay
	bool RestoreDefaults(const std::string &id, std::string &err);
	bool Export(const std::string &id, std::string &outPath, std::string &err);
	bool Import(const std::string &path, Profile &out, std::string &err);

	std::string UniqueId(const std::string &base) const;

private:
	bool Migrate(Profile &p, const std::string &file);

	Paths paths_;
	std::string appVersion_;
	std::vector<Profile> profiles_;
	std::vector<std::string> problems_;
};

} // namespace rdoom
