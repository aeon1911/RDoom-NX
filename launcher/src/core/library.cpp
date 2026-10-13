#include "library.h"
#include "fsutil.h"
#include "log.h"

#include <algorithm>
#include <cctype>

namespace rdoom {

bool GlobMatch(const std::string &pattern, const std::string &name)
{
	const std::string p = ToLower(pattern), n = ToLower(name);
	size_t pi = 0, ni = 0, star = std::string::npos, mark = 0;
	while (ni < n.size())
	{
		if (pi < p.size() && (p[pi] == '?' || p[pi] == n[ni]))
		{
			pi++;
			ni++;
		}
		else if (pi < p.size() && p[pi] == '*')
		{
			star = pi++;
			mark = ni;
		}
		else if (star != std::string::npos)
		{
			pi = star + 1;
			ni = ++mark;
		}
		else
			return false;
	}
	while (pi < p.size() && p[pi] == '*')
		pi++;
	return pi == p.size();
}

static std::string RelativeTo(const std::string &root, const std::string &path)
{
	if (path.compare(0, root.size(), root) == 0)
	{
		std::string r = path.substr(root.size());
		while (!r.empty() && r[0] == '/')
			r.erase(0, 1);
		return r;
	}
	return path;
}

// File-name order as people read it: case-insensitive, and runs of digits
// compared by value ("2-mod" before "10-mod").
bool NaturalLess(const std::string &a, const std::string &b)
{
	size_t i = 0, j = 0;
	while (i < a.size() && j < b.size())
	{
		const unsigned char ca = (unsigned char)a[i], cb = (unsigned char)b[j];
		if (isdigit(ca) && isdigit(cb))
		{
			size_t ei = i, ej = j;
			while (ei < a.size() && isdigit((unsigned char)a[ei])) ei++;
			while (ej < b.size() && isdigit((unsigned char)b[ej])) ej++;
			size_t si = i, sj = j;
			while (si + 1 < ei && a[si] == '0') si++;  // leading zeros do not count
			while (sj + 1 < ej && b[sj] == '0') sj++;
			if (ei - si != ej - sj)
				return ei - si < ej - sj;
			const int c = a.compare(si, ei - si, b, sj, ej - sj);
			if (c != 0)
				return c < 0;
			i = ei;
			j = ej;
			continue;
		}
		const int la = tolower(ca), lb = tolower(cb);
		if (la != lb)
			return la < lb;
		i++;
		j++;
	}
	return a.size() - i < b.size() - j;
}

static void SortByFileName(std::vector<LibraryItem> &items)
{
	std::stable_sort(items.begin(), items.end(), [](const LibraryItem &x, const LibraryItem &y) {
		const std::string fx = PathFileName(x.ref), fy = PathFileName(y.ref);
		if (NaturalLess(fx, fy)) return true;
		if (NaturalLess(fy, fx)) return false;
		return NaturalLess(x.ref, y.ref);  // same name in different folders
	});
}

void Library::Scan(const Paths &paths, const std::vector<std::string> &extraIwadDirs,
                   const std::vector<std::string> &extraModDirs, const std::vector<IwadDef> &defs)
{
	iwads_.clear();
	mods_.clear();

	std::vector<std::pair<std::string, bool>> iwadDirs = { { paths.Iwads(), false } };
	for (const auto &d : extraIwadDirs)
		iwadDirs.push_back({ d, true });
	for (const auto &d : iwadDirs)
	{
		if (!DirExists(d.first))
		{
			if (d.second)
				RLOG_W("scan", "extra IWAD folder %s not found", d.first.c_str());
			continue;
		}
		for (const auto &e : ListFilesRecursive(d.first, 1))
		{
			const std::string ext = PathExtLower(e.name);
			if (!IsSupportedIwadExtension(ext))
				continue;
			LibraryItem it;
			it.info = InspectFile(e.path, defs);
			it.ref = d.second ? e.path : RelativeTo(d.first, e.path);
			it.root = d.first;
			it.external = d.second;
			RLOG_I("scan", "iwad %s: %s, %s%s", it.ref.c_str(), FileStatusName(it.info.status),
			       it.info.isIwad ? "IWAD" : "not an IWAD", it.info.iwadDef >= 0 ? (" (" + defs[it.info.iwadDef].name + ")").c_str() : "");
			iwads_.push_back(std::move(it));
		}
	}

	std::vector<std::pair<std::string, bool>> modDirs = { { paths.Mods(), false } };
	for (const auto &d : extraModDirs)
		modDirs.push_back({ d, true });
	for (const auto &d : modDirs)
	{
		if (!DirExists(d.first))
		{
			if (d.second)
				RLOG_W("scan", "extra mod folder %s not found", d.first.c_str());
			continue;
		}
		for (const auto &e : ListFilesRecursive(d.first, 2))
		{
			const std::string ext = PathExtLower(e.name);
			if (!IsSupportedModExtension(ext))
				continue;  // readme.txt, screenshots, ... are not listed
			LibraryItem it;
			it.info = InspectFile(e.path, defs);
			it.ref = d.second ? e.path : RelativeTo(d.first, e.path);
			it.root = d.first;
			it.external = d.second;
			RLOG_I("scan", "mod %s: %s, %s, %d maps", it.ref.c_str(), FileStatusName(it.info.status),
			       ModCategoryName(it.info.category), it.info.mapCount);
			mods_.push_back(std::move(it));
		}
	}
	SortByFileName(iwads_);
	SortByFileName(mods_);
}

const LibraryItem *Library::Find(const std::vector<LibraryItem> &items, const std::string &ref)
{
	const std::string l = ToLower(ref);
	for (const auto &it : items)
		if (ToLower(it.ref) == l || it.info.path == ref)
			return &it;
	// a bare file name also matches an item in a sub-folder
	if (ref.find('/') == std::string::npos)
		for (const auto &it : items)
			if (ToLower(PathFileName(it.ref)) == l)
				return &it;
	return nullptr;
}

const LibraryItem *Library::FindIwad(const std::string &ref) const { return Find(iwads_, ref); }
const LibraryItem *Library::FindMod(const std::string &ref) const { return Find(mods_, ref); }

std::vector<const LibraryItem *> Library::MatchMods(const std::string &pattern) const
{
	std::vector<const LibraryItem *> out;
	for (const auto &it : mods_)
		if (GlobMatch(pattern, PathFileName(it.ref)))
			out.push_back(&it);
	return out;
}

static void Add(ResolvedProfile &r, IssueLevel l, const std::string &t)
{
	r.issues.push_back({ l, t });
}

ResolvedProfile ResolveProfile(const Profile &p, const Library &lib, const std::vector<IwadDef> &defs)
{
	ResolvedProfile r;
	r.filled = p;

	// --- IWAD
	if (!p.iwad.empty())
	{
		const LibraryItem *it = lib.FindIwad(p.iwad);
		if (!it)
			Add(r, IssueLevel::Error, "IWAD ausente: " + p.iwad + " (coloque em iwads/)");
		else if (it->info.status != FileStatus::Ok)
			Add(r, IssueLevel::Error, "IWAD inválida: " + p.iwad + " (" + it->info.error + ")");
		else if (!it->info.isIwad)
			Add(r, IssueLevel::Error, p.iwad + " não é uma IWAD (é um PWAD/mod)");
		else if (!it->info.incompatible.empty())
			Add(r, IssueLevel::Error, p.iwad + ": " + it->info.incompatible);
		else
			r.iwad = it;
	}
	else if (!p.iwadCandidates.empty())
	{
		for (const auto &c : p.iwadCandidates)
		{
			const LibraryItem *it = lib.FindIwad(c);
			if (it && it->info.status == FileStatus::Ok && it->info.isIwad && !it->info.incompatible.empty())
			{
				Add(r, IssueLevel::Error, it->ref + ": " + it->info.incompatible);
				r.filled.iwad = it->ref;
				break;
			}
			if (it && it->info.status == FileStatus::Ok && it->info.isIwad)
			{
				r.iwad = it;
				r.filled.iwad = it->ref;
				Add(r, IssueLevel::Info, "IWAD do modelo encontrada: " + it->ref);
				break;
			}
		}
		if (!r.iwad)
		{
			std::string list;
			for (const auto &c : p.iwadCandidates)
				list += (list.empty() ? "" : " ou ") + c;
			Add(r, IssueLevel::Error, "Pendente de instalação: coloque " + list + " em iwads/");
			r.pendingInstall = true;
		}
	}
	else
	{
		Add(r, IssueLevel::Error, "Nenhuma IWAD escolhida para este perfil");
	}

	// --- mods
	std::vector<ModRef> mods = p.mods;
	if (mods.empty() && !p.modPatterns.empty())
	{
		for (const auto &rawPat : p.modPatterns)
		{
			const bool optional = !rawPat.empty() && rawPat[0] == '?';
			const std::string pat = optional ? rawPat.substr(1) : rawPat;
			auto matches = lib.MatchMods(pat);
			// a template must not pick an IWAD-type file as a mod
			matches.erase(std::remove_if(matches.begin(), matches.end(), [](const LibraryItem *m) { return m->info.isIwad; }), matches.end());
			if (matches.empty())
			{
				if (!optional)
				{
					Add(r, IssueLevel::Error, "Pendente de instalação: arquivo " + pat + " não encontrado em mods/");
					r.pendingInstall = true;
				}
				continue;
			}
			const LibraryItem *pick = matches.back();
			if (matches.size() > 1)
				Add(r, IssueLevel::Warning, "Vários arquivos correspondem a " + pat + "; usando " + pick->ref + " (altere em Mods)");
			else
				Add(r, IssueLevel::Info, "Mod do modelo encontrado: " + pick->ref);
			ModRef ref;
			ref.file = pick->ref;
			ref.optional = optional;
			mods.push_back(ref);
		}
		r.filled.mods = mods;
	}
	for (const auto &m : mods)
	{
		if (!m.enabled)
			continue;
		const LibraryItem *it = lib.FindMod(m.file);
		if (!it)
		{
			Add(r, m.optional ? IssueLevel::Warning : IssueLevel::Error, "Mod ausente: " + m.file);
			continue;
		}
		if (it->info.status != FileStatus::Ok)
		{
			Add(r, IssueLevel::Error, "Mod inválido: " + m.file + " (" + it->info.error + ")");
			continue;
		}
		if (it->info.isIwad)
		{
			Add(r, IssueLevel::Error, m.file + " é uma IWAD; selecione-a como IWAD do perfil");
			continue;
		}
		if (!it->info.incompatible.empty())
		{
			Add(r, IssueLevel::Error, m.file + ": " + it->info.incompatible);
			continue;
		}
		r.mods.push_back(it);
	}

	// --- dependencies declared by the mods themselves (GAMEINFO)
	for (const LibraryItem *m : r.mods)
	{
		for (const auto &dep : m->info.gameinfoLoad)
		{
			bool found = lib.FindMod(dep) != nullptr;
			if (!found)
				for (const LibraryItem *o : r.mods)
					if (ToLower(PathFileName(o->ref)) == ToLower(PathFileName(dep)))
						found = true;
			if (!found)
				Add(r, IssueLevel::Error, PathFileName(m->ref) + " precisa de " + dep + " (GAMEINFO LOAD), não encontrado");
		}
		if (!m->info.gameinfoIwad.empty() && r.iwad &&
		    ToLower(PathFileName(m->info.gameinfoIwad)) != ToLower(PathFileName(r.iwad->ref)))
		{
			Add(r, IssueLevel::Warning, PathFileName(m->ref) + " foi feito para " + m->info.gameinfoIwad +
			    " (IWAD do perfil: " + PathFileName(r.iwad->ref) + ")");
		}
	}
	// add-on IWADs that need a base game
	if (r.iwad && r.iwad->info.iwadDef >= 0 && r.iwad->info.iwadDef < (int)defs.size())
	{
		const std::string &req = defs[r.iwad->info.iwadDef].required;
		if (!req.empty())
			Add(r, IssueLevel::Warning, r.iwad->info.title + " exige " + req + " na pasta iwads/");
	}

	r.runnable = std::none_of(r.issues.begin(), r.issues.end(), [](const Issue &i) { return i.level == IssueLevel::Error; });
	return r;
}

} // namespace rdoom
