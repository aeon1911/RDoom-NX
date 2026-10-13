// RDoom launcher - identifies IWADs and mods the way UZDoom will see them.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "archive.h"

namespace rdoom {

// One "IWad" block from UZDoom's iwadinfo.txt (shipped in game_support.pk3).
struct IwadDef
{
	std::string name;        // "DOOM 2: Hell on Earth"
	std::string game;        // "Doom", "Heretic", ...
	std::string autoname;    // "doom.id.doom2.commercial"
	std::string iwadName;    // default file name
	std::vector<std::string> mustContain;
	std::string required;    // add-on IWADs: the base game they need
};

// Parses iwadinfo.txt text. Order is preserved (it is the identification order).
std::vector<IwadDef> ParseIwadInfo(const std::string &text);
// Loads the definitions from game_support.pk3 (or a loose iwadinfo.txt).
std::vector<IwadDef> LoadIwadDefs(const std::string &enginePk3OrTxt);

enum class FileStatus
{
	Ok,           // readable and of a kind the engine loads
	Missing,      // file not found
	Invalid,      // unreadable / truncated / wrong signature
	Unsupported,  // extension the engine does not load
};

enum class ModCategory { Unknown, MapPack, Gameplay, GameplayAndMaps, Resources, Dehacked };

struct ContentInfo
{
	std::string path;
	std::string fileName;
	uint64_t size = 0;
	FileStatus status = FileStatus::Missing;
	std::string error;
	ArchiveType archive = ArchiveType::None;

	bool isIwad = false;        // IWAD header, or a zip carrying IWADINFO
	int iwadDef = -1;           // index into the IwadDef list, -1 if not identified
	std::string title;          // display name (IWAD name, GAMEINFO title or file stem)

	int mapCount = 0;
	bool hasZScript = false;
	bool hasDecorate = false;
	bool hasDehacked = false;
	bool hasMapinfo = false;
	bool hasTitlePic = false;
	ModCategory category = ModCategory::Unknown;
	// GAMEINFO: IWAD = "..." and LOAD = "...", "..."
	std::string gameinfoIwad;
	std::vector<std::string> gameinfoLoad;
	std::string startupTitle;   // GAMEINFO STARTUPTITLE
	// Known engine-version mismatch detected from the file's own contents
	// (non-empty = this build of the content cannot run on UZDoom 5.0.3).
	std::string incompatible;
};

const char *FileStatusName(FileStatus s);
const char *ModCategoryName(ModCategory c);

// Extensions UZDoom accepts as game data.
bool IsSupportedIwadExtension(const std::string &extLower);
bool IsSupportedModExtension(const std::string &extLower);

ContentInfo InspectFile(const std::string &path, const std::vector<IwadDef> &defs);

// TITLEPIC (Doom picture or PNG) to RGBA. palette768: PLAYPAL of the file or
// of its IWAD; may be empty for PNG pictures.
bool ExtractTitlePic(const std::string &path, const std::string &palette768,
                     std::vector<uint8_t> &rgba, int &w, int &h);
// First 768 bytes of PLAYPAL, or empty.
std::string ExtractPalette(const std::string &path);
// Decodes a Doom-format picture lump to RGBA (transparent where no post).
bool DecodeDoomPicture(const std::string &lump, const std::string &palette768,
                       std::vector<uint8_t> &rgba, int &w, int &h);

} // namespace rdoom
