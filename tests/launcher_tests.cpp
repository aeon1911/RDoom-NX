// Host tests for the RDoom launcher core. No commercial data is used: every
// WAD/PK3 here is synthesised by the test itself, plus (optionally) the real
// Freedoom IWADs and the engine's iwadinfo.txt when their paths are given.
//
//   launcher_tests <scratch dir> [iwadinfo.txt] [freedoom dir]

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <zlib.h>

#include "archive.h"
#include "content.h"
#include "fsutil.h"
#include "launch.h"
#include "library.h"
#include "log.h"
#include "paths.h"
#include "profile.h"

using namespace rdoom;

static int g_checks = 0, g_failures = 0;
static const char *g_test = "";

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; fprintf(stderr, "  FAIL [%s] %s:%d: %s\n", g_test, __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { g_checks++; auto va_ = (a); auto vb_ = (b); if (!(va_ == vb_)) { g_failures++; fprintf(stderr, "  FAIL [%s] %s:%d: %s == %s\n", g_test, __FILE__, __LINE__, #a, #b); } } while (0)

static std::string g_scratch, g_iwadinfo, g_freedoom;

// --------------------------------------------------------------------------
// fixtures
// --------------------------------------------------------------------------

static void Put32(std::string &s, uint32_t v) { for (int i = 0; i < 4; i++) s += char((v >> (8 * i)) & 0xff); }
static void Put16(std::string &s, uint16_t v) { s += char(v & 0xff); s += char(v >> 8); }

struct Lump { std::string name, data; };

static std::string MakeWad(bool iwad, const std::vector<Lump> &lumps)
{
	std::string body;
	std::vector<std::pair<uint32_t, uint32_t>> pos;
	for (auto &l : lumps)
	{
		pos.push_back({ 12 + (uint32_t)body.size(), (uint32_t)l.data.size() });
		body += l.data;
	}
	std::string w = iwad ? "IWAD" : "PWAD";
	Put32(w, (uint32_t)lumps.size());
	Put32(w, 12 + (uint32_t)body.size());
	w += body;
	for (size_t i = 0; i < lumps.size(); i++)
	{
		Put32(w, pos[i].first);
		Put32(w, pos[i].second);
		char n[8] = {};
		strncpy(n, lumps[i].name.c_str(), 8);
		w.append(n, 8);
	}
	return w;
}

static std::string Deflate(const std::string &in)
{
	z_stream zs;
	memset(&zs, 0, sizeof(zs));
	deflateInit2(&zs, 9, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
	std::string out(compressBound((uLong)in.size()) + 64, '\0');
	zs.next_in = (Bytef *)in.data();
	zs.avail_in = (uInt)in.size();
	zs.next_out = (Bytef *)&out[0];
	zs.avail_out = (uInt)out.size();
	deflate(&zs, Z_FINISH);
	out.resize(zs.total_out);
	deflateEnd(&zs);
	return out;
}

static std::string MakeZip(const std::vector<Lump> &files)
{
	std::string z, cd;
	for (auto &f : files)
	{
		const bool deflate = f.data.size() > 16;
		const std::string comp = deflate ? Deflate(f.data) : f.data;
		const uint32_t crc = (uint32_t)crc32(0, (const Bytef *)f.data.data(), (uInt)f.data.size());
		const uint32_t ofs = (uint32_t)z.size();
		Put32(z, 0x04034b50); Put16(z, 20); Put16(z, 0); Put16(z, deflate ? 8 : 0); Put16(z, 0); Put16(z, 0);
		Put32(z, crc); Put32(z, (uint32_t)comp.size()); Put32(z, (uint32_t)f.data.size());
		Put16(z, (uint16_t)f.name.size()); Put16(z, 0);
		z += f.name;
		z += comp;
		Put32(cd, 0x02014b50); Put16(cd, 20); Put16(cd, 20); Put16(cd, 0); Put16(cd, deflate ? 8 : 0); Put16(cd, 0); Put16(cd, 0);
		Put32(cd, crc); Put32(cd, (uint32_t)comp.size()); Put32(cd, (uint32_t)f.data.size());
		Put16(cd, (uint16_t)f.name.size()); Put16(cd, 0); Put16(cd, 0); Put16(cd, 0); Put16(cd, 0); Put32(cd, 0); Put32(cd, ofs);
		cd += f.name;
	}
	const uint32_t cdOfs = (uint32_t)z.size();
	z += cd;
	Put32(z, 0x06054b50); Put16(z, 0); Put16(z, 0); Put16(z, (uint16_t)files.size()); Put16(z, (uint16_t)files.size());
	Put32(z, (uint32_t)cd.size()); Put32(z, cdOfs); Put16(z, 0);
	return z;
}

static void WriteRaw(const std::string &path, const std::string &data)
{
	MakeDirs(PathParent(path));
	FILE *f = fopen(path.c_str(), "wb");
	fwrite(data.data(), 1, data.size(), f);
	fclose(f);
}

static const char *kIwadInfo =
	"// test definitions, same syntax as UZDoom's iwadinfo.txt\n"
	"IWad\n{\n\tName = \"Freedoom: Phase 2\"\n\tAutoname = \"doom.freedoom.phase2\"\n\tGame = \"Doom\"\n"
	"\tIWADName = \"freedoom2.wad\", 2\n\tMustContain = \"MAP01\", \"FREEDOOM\"\n\tBannerColors = \"32 54 43\", \"c6 dc d1\"\n}\n"
	"IWad\n{\n\tName = \"DOOM 2: Hell on Earth\"\n\tGame = \"Doom\"\n\tIWADName = \"doom2.wad\"\n\tMustContain = \"MAP01\"\n}\n"
	"/* block comment */\nNames\n{\n\t\"doom2.wad\"\n\t\"freedoom2.wad\"\n}\n";

static Lump PicLump()
{
	// 2x2 Doom picture: column 0 = palette index 1 at y 0..1, column 1 empty
	std::string d;
	Put16(d, 2); Put16(d, 2); Put16(d, 0); Put16(d, 0);
	Put32(d, 16); Put32(d, 22);
	d += char(0); d += char(2); d += char(0); d += char(1); d += char(1); d += char(0); d += char(0xff);
	d += char(0xff);
	return { "TITLEPIC", d };
}

static Lump PalLump()
{
	std::string p(768 * 14, '\0');
	p[3] = (char)200; p[4] = 10; p[5] = 20;  // index 1
	return { "PLAYPAL", p };
}

static Paths SetupSd(const std::string &name)
{
	Paths p;
	p.root = PathJoin(g_scratch, name);
	std::string cmd = "rm -rf '" + p.root + "'";
	(void)system(cmd.c_str());
	CHECK(p.EnsureLayout());
	return p;
}

static void InstallFixtures(const Paths &p)
{
	WriteRaw(PathJoin(p.Iwads(), "freedoom2.wad"), MakeWad(true, { { "FREEDOOM", "x" }, PalLump(), PicLump(), { "MAP01", "" }, { "THINGS", "t" } }));
	WriteRaw(PathJoin(p.Iwads(), "not-an-iwad.wad"), MakeWad(false, { { "MAP01", "" }, { "THINGS", "" } }));
	WriteRaw(PathJoin(p.Iwads(), "truncated.wad"), std::string("IWAD\x05\0\0\0\xff\xff\0\0", 12));
	WriteRaw(PathJoin(p.Iwads(), "readme.txt"), "hello");
	WriteRaw(PathJoin(p.Mods(), "brutal21.pk3"), MakeZip({ { "zscript.txt", "version \"4.0\"\nclass A : Actor {}\n" }, { "gameinfo.txt", "STARTUPTITLE = \"Brutal Test\"\nLOAD = \"needed.pk3\"\n" } }));
	WriteRaw(PathJoin(p.Mods(), "brutal22test6.pk3"), MakeZip({ { "decorate.txt", "actor X {}" } }));
	WriteRaw(PathJoin(p.Mods(), "maps/megawad.wad"), MakeWad(false, { { "MAP01", "" }, { "THINGS", "" }, { "MAP02", "" }, { "TEXTMAP", "" } }));
	WriteRaw(PathJoin(p.Mods(), "patch.deh"), "Patch File for DeHackEd v3.0\n");
	WriteRaw(PathJoin(p.Mods(), "broken.pk3"), "PK\x03\x04garbage-without-directory");
	WriteRaw(PathJoin(p.Mods(), "notes.txt"), "not a mod");
	WriteRaw(PathJoin(p.Mods(), "fusion_copy.ipk3"), MakeZip({ { "iwadinfo.txt", "IWad { Name = \"WadFusion Test\" Game = \"Doom\" MustContain = \"MAP01\" }" } }));
	// CVARINFO clashes (two Ashes episodes loaded together stop UZDoom)
	{
		CHECK_EQ(ParseCvarInfo("// c\nserver int hud_vertical = 0;\nuser noarchive bool Foo_Bar=true; /* x; */ nosave string s = \"a;b\";").size(), (size_t)3);
		const std::string a = PathJoin(p.Mods(), "clash-a.wad"), b = PathJoin(p.Mods(), "clash-b.pk3"), c = PathJoin(p.Mods(), "clash-c.wad");
		WriteRaw(a, MakeWad(false, { { "CVARINFO", "server int hud_vertical = 0;\nuser bool a_only = true;" } }));
		WriteRaw(b, MakeZip({ { "cvarinfo.txt", "server int HUD_Vertical = 1;" } }));
		WriteRaw(c, MakeWad(false, { { "CVARINFO", "user float c_only = 1.0;" } }));
		auto clashes = FindCvarClashes({ { "A", a }, { "B", b }, { "C", c } });
		CHECK_EQ(clashes.size(), (size_t)1);
		CHECK(!clashes.empty() && clashes[0].find("hud_vertical") != std::string::npos);
		CHECK(FindCvarClashes({ { "A", a }, { "C", c } }).empty());
		RemoveFile(a); RemoveFile(b); RemoveFile(c);
	}
	WriteRaw(PathJoin(p.Iwads(), "doom_fusion.ipk3"), MakeZip({ { "iwadinfo.txt", "IWad { Name = \"WadFusion Test\" Game = \"Doom\" MustContain = \"MAP01\" }" }, { "maps/map01.wad", "PWAD" } }));
}

// --------------------------------------------------------------------------
// tests
// --------------------------------------------------------------------------

static void TestIwadInfoParse()
{
	auto defs = ParseIwadInfo(kIwadInfo);
	CHECK_EQ(defs.size(), (size_t)2);
	CHECK_EQ(defs[0].name, std::string("Freedoom: Phase 2"));
	CHECK_EQ(defs[0].mustContain.size(), (size_t)2);
	CHECK_EQ(defs[0].iwadName, std::string("freedoom2.wad"));
	if (!g_iwadinfo.empty())
	{
		auto real = LoadIwadDefs(g_iwadinfo);
		CHECK(real.size() >= 30);
		bool fd2 = false;
		for (auto &d : real)
			if (d.name.find("Freedoom: Phase 2") != std::string::npos)
				fd2 = true;
		CHECK(fd2);
	}
}

static void TestArchives()
{
	Paths p = SetupSd("archives");
	InstallFixtures(p);
	Archive a;
	CHECK(a.Open(PathJoin(p.Mods(), "brutal21.pk3")));
	CHECK(a.Type() == ArchiveType::Zip);
	const ArchiveEntry *e = a.Find("zscript");
	CHECK(e != nullptr);
	std::string text;
	CHECK(e && a.Read(*e, text));
	CHECK(text.find("class A") != std::string::npos);
	Archive b;
	CHECK(!b.Open(PathJoin(p.Mods(), "broken.pk3")));
	CHECK(!b.Error().empty());
	Archive c;
	CHECK(!c.Open(PathJoin(p.Iwads(), "truncated.wad")));
	Archive d;
	CHECK(!d.Open(PathJoin(p.root, "does-not-exist.wad")));
}

static void TestDiscovery()
{
	Paths p = SetupSd("discovery");
	InstallFixtures(p);
	auto defs = ParseIwadInfo(kIwadInfo);
	Library lib;
	lib.Scan(p, {}, {}, defs);

	const LibraryItem *fd2 = lib.FindIwad("freedoom2.wad");
	CHECK(fd2 && fd2->info.isIwad && fd2->info.status == FileStatus::Ok);
	CHECK(fd2 && fd2->info.iwadDef == 0);
	CHECK(fd2 && fd2->info.title == "Freedoom: Phase 2");
	const LibraryItem *pw = lib.FindIwad("not-an-iwad.wad");
	CHECK(pw && !pw->info.isIwad && pw->info.status == FileStatus::Ok);
	const LibraryItem *tr = lib.FindIwad("truncated.wad");
	CHECK(tr && tr->info.status == FileStatus::Invalid);
	CHECK(lib.FindIwad("readme.txt") == nullptr);  // not listed at all
	const LibraryItem *fus = lib.FindIwad("doom_fusion.ipk3");
	CHECK(fus && fus->info.isIwad && fus->info.title == "WadFusion Test");

	const LibraryItem *b21 = lib.FindMod("brutal21.pk3");
	CHECK(b21 && b21->info.hasZScript && b21->info.category == ModCategory::Gameplay);
	CHECK(b21 && b21->info.title == "Brutal Test");
	CHECK(b21 && b21->info.gameinfoLoad.size() == 1);
	const LibraryItem *mw = lib.FindMod("maps/megawad.wad");
	CHECK(mw && mw->info.mapCount == 2 && mw->info.category == ModCategory::MapPack);
	CHECK(lib.FindMod("megawad.wad") == mw);  // bare file name finds sub-folder items
	const LibraryItem *deh = lib.FindMod("patch.deh");
	CHECK(deh && deh->info.category == ModCategory::Dehacked);
	const LibraryItem *br = lib.FindMod("broken.pk3");
	CHECK(br && br->info.status == FileStatus::Invalid);
	CHECK(lib.FindMod("notes.txt") == nullptr);
	CHECK_EQ(lib.MatchMods("brutal*.pk3").size(), (size_t)2);
	CHECK(GlobMatch("lexicon-core*.pk3", "LEXICON-CORE-v1.0.pk3"));
	CHECK(!GlobMatch("lexicon-core*.pk3", "lexicon-base-v1.0.pk3"));
	// library order: case-insensitive, numbers by value
	CHECK(NaturalLess("2-mod.pk3", "10-mod.pk3"));
	CHECK(!NaturalLess("10-mod.pk3", "2-mod.pk3"));
	CHECK(NaturalLess("brutalv21.pk3", "Trailblazer.pk3"));
	CHECK(NaturalLess("Ashes.pk3", "brutal.pk3"));
	CHECK(NaturalLess("map2.wad", "map02b.wad"));
	CHECK(!NaturalLess("a.pk3", "A.pk3") && !NaturalLess("A.pk3", "a.pk3"));
}

static void TestTitlePic()
{
	Paths p = SetupSd("titlepic");
	InstallFixtures(p);
	std::vector<uint8_t> rgba;
	int w = 0, h = 0;
	CHECK(ExtractTitlePic(PathJoin(p.Iwads(), "freedoom2.wad"), "", rgba, w, h));
	CHECK(w == 2 && h == 2);
	CHECK(rgba.size() == 16 && rgba[0] == 200 && rgba[1] == 10 && rgba[3] == 255);
	CHECK(rgba.size() == 16 && rgba[7] == 0);  // column 1 is transparent
}

static void TestProfilesLifecycle()
{
	Paths p = SetupSd("profiles");
	InstallFixtures(p);
	ProfileStore store(p, "test");
	store.Load();
	CHECK_EQ(store.All().size(), BuiltinTemplates().size());  // first run creates templates
	CHECK(store.Find("freedoom2-vulkan-stock") != nullptr);
	CHECK(FileExists(p.ProfileFile("brutal-doom")));
	for (auto &pr : store.All())
		CHECK(pr.video.renderer == Renderer::Vulkan);  // Vulkan is the default everywhere

	std::string err;
	Profile created;
	CHECK(store.Create("custom", "Meu Perfil", created, err));
	CHECK_EQ(created.id, std::string("meu-perfil"));
	created.iwad = "freedoom2.wad";
	created.mods = { { "maps/megawad.wad", true, false }, { "brutal22test6.pk3", true, false }, { "patch.deh", false, false } };
	created.video.fpsLimit = 30;
	created.video.internalScale = 0.75f;
	created.cvars["r_drawfuzz"] = "0";
	CHECK(store.Save(created, err));

	Profile dup;
	CHECK(store.Duplicate("meu-perfil", dup, err));
	CHECK_EQ(dup.id, std::string("meu-perfil-2"));
	CHECK(store.Rename("meu-perfil-2", "Segundo", err));

	// persistence: a fresh store reads the same data back, mod order intact
	ProfileStore again(p, "test");
	again.Load();
	const Profile *rp = again.Find("meu-perfil");
	CHECK(rp != nullptr);
	if (rp)
	{
		CHECK_EQ(rp->mods.size(), (size_t)3);
		CHECK_EQ(rp->mods[0].file, std::string("maps/megawad.wad"));
		CHECK_EQ(rp->mods[1].file, std::string("brutal22test6.pk3"));
		CHECK(!rp->mods[2].enabled);
		CHECK_EQ(rp->video.fpsLimit, 30);
		CHECK(rp->video.internalScale > 0.74f && rp->video.internalScale < 0.76f);
		CHECK_EQ(rp->cvars.at("r_drawfuzz"), std::string("0"));
	}
	CHECK(again.Find("meu-perfil-2") && again.Find("meu-perfil-2")->name == "Segundo");

	// export / import
	std::string exported;
	CHECK(again.Export("meu-perfil", exported, err));
	CHECK(FileExists(exported));
	Profile imported;
	CHECK(again.Import(exported, imported, err));
	CHECK(imported.id != "meu-perfil" && imported.mods.size() == 3);

	// restore defaults keeps files, resets options
	CHECK(again.RestoreDefaults("meu-perfil", err));
	CHECK_EQ(again.Find("meu-perfil")->video.fpsLimit, 60);
	CHECK_EQ(again.Find("meu-perfil")->mods.size(), (size_t)3);

	// delete removes the profile but never the mod files
	CHECK(again.Remove("meu-perfil", err));
	CHECK(!FileExists(p.ProfileFile("meu-perfil")));
	CHECK(FileExists(PathJoin(p.Mods(), "brutal22test6.pk3")));
	CHECK(!ListDir(p.Backup()).empty());  // a copy of the deleted profile is kept
}

static void TestCorruptAndMigration()
{
	Paths p = SetupSd("corrupt");
	WriteRaw(p.ProfileFile("bad"), "{ \"name\": \"x\", ");                 // truncated JSON
	WriteRaw(p.ProfileFile("legacy"), "{ \"name\": \"Old\", \"iwad\": \"doom2.wad\", \"mods\": [\"a.pk3\"] }");
	WriteRaw(p.ProfileFile("wrongtypes"), "{ \"version\": 1, \"name\": 5, \"video\": { \"renderer\": \"metal\", \"fps_limit\": 999 }, \"cvars\": { \"ok_var\": \"1\", \"evil\": \"1; quit\" } }");
	WriteRaw(p.ProfileFile("oldgl"), "{ \"version\": 1, \"name\": \"Old GL\", \"video\": { \"renderer\": \"opengl\" } }");
	ProfileStore s(p, "test");
	s.Load();
	CHECK_EQ(s.Problems().size(), (size_t)1);       // only the truncated file
	CHECK(FileExists(p.ProfileFile("bad")));         // never deleted
	CHECK(s.Find("bad") == nullptr);
	const Profile *legacy = s.Find("legacy");
	CHECK(legacy && legacy->version == kProfileFormatVersion && legacy->mods.size() == 1);
	bool migratedBackup = false;
	for (auto &e : ListDir(p.Backup()))
		if (e.name.find("legacy.pre-migration") == 0)
			migratedBackup = true;
	CHECK(migratedBackup);
	const Profile *wt = s.Find("wrongtypes");
	CHECK(wt && wt->video.renderer == Renderer::Vulkan && wt->video.fpsLimit == 60);
	CHECK(wt && wt->cvars.count("ok_var") == 1 && wt->cvars.count("evil") == 0);
	const Profile *oldgl = s.Find("oldgl");	// OpenGL was removed: such profiles load as Vulkan
	CHECK(oldgl && oldgl->video.renderer == Renderer::Vulkan);

	// launcher.json recovery
	WriteRaw(p.LauncherConfigFile(), "not json");
	LauncherSettings ls;
	std::string warn;
	CHECK(!LoadLauncherSettings(p, ls, warn));
	CHECK(!warn.empty() && ls.lastProfile.empty());
	ls.lastProfile = "legacy";
	ls.extraModDirs = { "sdmc:/switch/gzdoom/pwads" };
	std::string err;
	CHECK(SaveLauncherSettings(p, ls, err));
	LauncherSettings back;
	CHECK(LoadLauncherSettings(p, back, warn) && back.lastProfile == "legacy" && back.extraModDirs.size() == 1);
}

static void TestResolveAndLaunch()
{
	Paths p = SetupSd("resolve");
	InstallFixtures(p);
	auto defs = ParseIwadInfo(kIwadInfo);
	Library lib;
	lib.Scan(p, {}, {}, defs);

	// template with installed files
	Profile fd = MakeDefaultProfile("freedoom2");
	auto r = ResolveProfile(fd, lib, defs);
	CHECK(r.runnable && r.iwad && r.filled.iwad == "freedoom2.wad");

	// template with missing files is "pending", not runnable
	Profile comp = MakeDefaultProfile("compendium");
	auto rc = ResolveProfile(comp, lib, defs);
	CHECK(!rc.runnable && rc.pendingInstall);

	// brutal template: two matches -> warning, last one picked
	Profile bd = MakeDefaultProfile("brutaldoom");
	bd.iwadCandidates = { "freedoom2.wad" };
	auto rb = ResolveProfile(bd, lib, defs);
	CHECK(rb.runnable && rb.mods.size() == 1 && rb.filled.mods[0].file == "brutal22test6.pk3");
	bool warned = false;
	for (auto &i : rb.issues)
		if (i.level == IssueLevel::Warning)
			warned = true;
	CHECK(warned);

	// Project Brutality template: GitHub zip name and release names, not Brutal Doom
	Profile pb = MakeDefaultProfile("brutality");
	CHECK_EQ(pb.id, std::string("project-brutality"));
	CHECK(pb.modPatterns.size() == 1);
	CHECK(GlobMatch(pb.modPatterns[0], "Project_Brutality-master.pk3"));
	CHECK(GlobMatch(pb.modPatterns[0], "Project Brutality 3.0.pk3"));
	CHECK(!GlobMatch(pb.modPatterns[0], "brutalv21.pk3"));

	// missing dependency declared by GAMEINFO LOAD
	Profile dep;
	dep.id = "dep";
	dep.iwad = "freedoom2.wad";
	dep.mods = { { "brutal21.pk3", true, false } };
	auto rd = ResolveProfile(dep, lib, defs);
	CHECK(!rd.runnable);

	// WadFusion built for GZDoom/UZDoom 4.14: rejected with a clear reason
	WriteRaw(PathJoin(p.Iwads(), "old_fusion.ipk3"), MakeZip({ { "iwadinfo.txt", "IWad { Name = \"DOOM Fusion\" Game = \"Doom\" MustContain = \"MAP01\" }" },
		{ "zscript/id24gore.zs", "class ID24LargeCorpsePile : Actor {}" }, { "zscript/id24nature.zs", "class ID24BushShort : Actor {}" } }));
	Library lib2;
	lib2.Scan(p, {}, {}, defs);
	Profile wf = MakeDefaultProfile("wadfusion");
	wf.iwadCandidates = { "old_fusion.ipk3" };
	auto rw = ResolveProfile(wf, lib2, defs);
	CHECK(!rw.runnable);
	bool explained = false;
	for (auto &i : rw.issues)
		if (i.text.find("WadFusion 1.6") != std::string::npos)
			explained = true;
	CHECK(explained);

	// invalid paths and wrong kinds
	Profile bad;
	bad.id = "bad";
	bad.iwad = "not-an-iwad.wad";
	bad.mods = { { "nope.pk3", true, false }, { "broken.pk3", true, false }, { "fusion_copy.ipk3", true, false } };
	auto rx = ResolveProfile(bad, lib, defs);
	int errors = 0;
	for (auto &i : rx.issues)
		if (i.level == IssueLevel::Error)
			errors++;
	CHECK_EQ(errors, 4);

	// launch plan: order, ini, savedir, no unvalidated text
	Profile lp;
	lp.id = "launch-test";
	lp.iwad = "freedoom2.wad";
	lp.mods = { { "maps/megawad.wad", true, false }, { "brutal22test6.pk3", true, false }, { "patch.deh", true, false } };
	lp.gameplay.skill = 3;
	lp.gameplay.warp = "MAP01";
	lp.cvars["evil"] = "1;quit";  // injected after validation: must be dropped
	auto rl = ResolveProfile(lp, lib, defs);
	CHECK(rl.runnable);
	LaunchPlan plan = BuildLaunchPlan(lp, rl, p);
	auto &a = plan.argv;
	auto idx = [&](const std::string &s) { for (size_t i = 0; i < a.size(); i++) if (a[i] == s) return (int)i; return -1; };
	CHECK(idx("-iwad") == 1 && a[2].find("freedoom2.wad") != std::string::npos);
	int fi = idx("-file");
	CHECK(fi > 0 && a[fi + 1].find("megawad.wad") != std::string::npos && a[fi + 2].find("brutal22test6.pk3") != std::string::npos);
	CHECK(idx("-deh") > 0);
	CHECK(idx("-config") > 0 && a[idx("-config") + 1] == p.EngineConfigFile("launch-test"));
	CHECK(idx("-savedir") > 0);
	CHECK(idx("evil") < 0);
	CHECK(idx("+map") > 0);
	for (auto &s : a)
		CHECK(s.find(';') == std::string::npos);

	// engine ini: renderer written before start, other content preserved
	std::string err;
	WriteRaw(p.EngineConfigFile("launch-test"), "[GlobalSettings]\nvid_vsync=true\n\n[Doom.Player]\nname=Player\n");
	// an engine ini left on OpenGL by an early build goes back to Vulkan
	WriteRaw(p.EngineConfigFile("launch-test"), "[GlobalSettings]\nvid_preferbackend=0\nvid_vsync=true\n\n[Doom.Player]\nname=Player\n");
	CHECK(PrepareEngineIni(lp, p, err));
	std::string ini;
	ReadFile(p.EngineConfigFile("launch-test"), ini);
	CHECK_EQ(IniGet(ini, "GlobalSettings", "vid_preferbackend"), std::string("1"));
	CHECK_EQ(IniGet(ini, "GlobalSettings", "vid_vsync"), std::string("true"));
	CHECK_EQ(IniGet(ini, "Doom.Player", "name"), std::string("Player"));
	lp.video.renderer = Renderer::Vulkan;
	CHECK(PrepareEngineIni(lp, p, err));
	ReadFile(p.EngineConfigFile("launch-test"), ini);
	CHECK_EQ(IniGet(ini, "GlobalSettings", "vid_preferbackend"), std::string("1"));
	Profile fresh;
	fresh.id = "fresh";
	CHECK(PrepareEngineIni(fresh, p, err));
	ReadFile(p.EngineConfigFile("fresh"), ini);
	CHECK_EQ(IniGet(ini, "GlobalSettings", "vid_preferbackend"), std::string("1"));
}

static void TestAtomicWrite()
{
	Paths p = SetupSd("atomic");
	const std::string f = PathJoin(p.Config(), "x.json");
	CHECK(WriteFileAtomic(f, "one") == WriteResult::Ok);
	CHECK(WriteFileAtomic(f, "two") == WriteResult::Ok);
	std::string s;
	ReadFile(f, s);
	CHECK_EQ(s, std::string("two"));
	ReadFile(f + ".bak", s);
	CHECK_EQ(s, std::string("one"));
	CHECK(!FileExists(f + ".tmp"));
	CHECK(WriteFileAtomic(PathJoin(p.root, "missing-dir/x.json"), "z") == WriteResult::OpenFailed);
	CHECK(MakeDirs(PathJoin(p.root, "a b/c d")));  // spaces and nesting
	CHECK(WriteFileAtomic(PathJoin(p.root, "a b/c d/e f.json"), "ok") == WriteResult::Ok);
}

static void TestValidation()
{
	CHECK(IsValidProfileId("freedoom2-vulkan-stock"));
	CHECK(!IsValidProfileId("../etc"));
	CHECK(!IsValidProfileId(""));
	CHECK(IsValidCvarValue("0.75"));
	CHECK(!IsValidCvarValue("1 quit"));
	CHECK(!IsValidCvarValue("\"x\""));
	CHECK(!IsValidCvarName("+exec"));
	CHECK(IsValidWarp("MAP01") && IsValidWarp("E1M1") && !IsValidWarp("MAP01;quit"));
	CHECK_EQ(MakeProfileId("Configura\xC3\xA7\xC3\xA3o personalizada"), std::string("configura-o-personalizada"));
}

static void TestRealFreedoom()
{
	if (g_freedoom.empty() || g_iwadinfo.empty())
	{
		printf("  (skipped: no Freedoom/iwadinfo paths given)\n");
		return;
	}
	auto defs = LoadIwadDefs(g_iwadinfo);
	ContentInfo f2 = InspectFile(PathJoin(g_freedoom, "freedoom2.wad"), defs);
	CHECK(f2.isIwad && f2.iwadDef >= 0);
	CHECK(f2.iwadDef >= 0 && defs[f2.iwadDef].name.find("Freedoom: Phase 2") != std::string::npos);
	CHECK(f2.mapCount == 32);
	ContentInfo f1 = InspectFile(PathJoin(g_freedoom, "freedoom1.wad"), defs);
	CHECK(f1.isIwad && f1.iwadDef >= 0 && defs[f1.iwadDef].name.find("Phase 1") != std::string::npos);
	std::vector<uint8_t> rgba;
	int w, h;
	CHECK(ExtractTitlePic(PathJoin(g_freedoom, "freedoom2.wad"), "", rgba, w, h));
	CHECK(w == 320 && h == 200);
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <scratch dir> [iwadinfo.txt] [freedoom dir]\n", argv[0]);
		return 2;
	}
	g_scratch = argv[1];
	if (argc > 2) g_iwadinfo = argv[2];
	if (argc > 3) g_freedoom = argv[3];
	MakeDirs(g_scratch);
	LogOpen(PathJoin(g_scratch, "tests.log"), true);

	struct { const char *name; void (*fn)(); } tests[] = {
		{ "iwadinfo", TestIwadInfoParse },
		{ "archives", TestArchives },
		{ "discovery", TestDiscovery },
		{ "titlepic", TestTitlePic },
		{ "profiles", TestProfilesLifecycle },
		{ "corrupt+migration", TestCorruptAndMigration },
		{ "resolve+launch", TestResolveAndLaunch },
		{ "atomic-write", TestAtomicWrite },
		{ "validation", TestValidation },
		{ "real-freedoom", TestRealFreedoom },
	};
	for (auto &t : tests)
	{
		g_test = t.name;
		int before = g_failures;
		t.fn();
		printf("%-20s %s\n", t.name, g_failures == before ? "ok" : "FAILED");
	}
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
