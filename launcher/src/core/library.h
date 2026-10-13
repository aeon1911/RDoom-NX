// RDoom launcher - IWAD and mod libraries, and resolution of a profile
// against what is actually installed.
#pragma once

#include <string>
#include <vector>

#include "content.h"
#include "paths.h"
#include "profile.h"

namespace rdoom {

// Case-insensitive file-name order with numbers compared by value ("2-x" < "10-x").
bool NaturalLess(const std::string &a, const std::string &b);


struct LibraryItem
{
	ContentInfo info;
	std::string ref;    // how profiles refer to it: path relative to its library folder
	std::string root;   // library folder it was found in
	bool external = false;  // found in an extra folder configured by the user
};

class Library
{
public:
	void Scan(const Paths &paths, const std::vector<std::string> &extraIwadDirs,
	          const std::vector<std::string> &extraModDirs, const std::vector<IwadDef> &defs);

	const std::vector<LibraryItem> &Iwads() const { return iwads_; }
	const std::vector<LibraryItem> &Mods() const { return mods_; }

	// ref: relative name (case-insensitive) or absolute path.
	const LibraryItem *FindIwad(const std::string &ref) const;
	const LibraryItem *FindMod(const std::string &ref) const;
	// Glob ('*' and '?') on the file name, case-insensitive, sorted by name.
	std::vector<const LibraryItem *> MatchMods(const std::string &pattern) const;

private:
	static const LibraryItem *Find(const std::vector<LibraryItem> &items, const std::string &ref);
	std::vector<LibraryItem> iwads_;
	std::vector<LibraryItem> mods_;
};

bool GlobMatch(const std::string &pattern, const std::string &name);  // case-insensitive

enum class IssueLevel { Info, Warning, Error };

struct Issue
{
	IssueLevel level;
	std::string text;
};

struct ResolvedProfile
{
	const LibraryItem *iwad = nullptr;
	std::vector<const LibraryItem *> mods;  // enabled mods in load order
	std::vector<Issue> issues;
	bool runnable = false;        // no Error issues
	bool pendingInstall = false;  // template whose files are not installed yet
	// The profile with template choices filled in (for saving after the user
	// confirms them in the UI).
	Profile filled;
};

ResolvedProfile ResolveProfile(const Profile &p, const Library &lib, const std::vector<IwadDef> &defs);

} // namespace rdoom
