// Renders the RDoom launcher screens on a PC with scripted input and saves
// them as PNG files. Same UI code as the NRO; only the platform differs.
//   ui_preview <sd root> <art dir> <game_support.pk3|iwadinfo.txt> <out dir>
#include <cstdio>
#include <string>
#include <vector>

#include "app.h"
#include "fsutil.h"
#include "log.h"

using namespace rdoom;

class HostPlatform : public Platform
{
public:
	Canvas canvas{ 1280, 720 };
	uint64_t t = 0;
	bool Poll(InputFrame &) override { return true; }
	Canvas &BeginDraw() override { return canvas; }
	void EndDraw() override {}
	uint64_t NowMs() override { return t; }
	bool TextInput(const std::string &, const std::string &initial, size_t, std::string &out) override { out = initial; return true; }
	SystemInfo Info() override
	{
		SystemInfo si;
		si.memTotal = 3221225472ull;
		si.memUsed = 52428800ull;
		si.sdFree = FreeSpace(".");
		si.firmware = "host preview";
		return si;
	}
	void Shutdown() override {}
};

int main(int argc, char **argv)
{
	if (argc < 5)
	{
		fprintf(stderr, "usage: %s <sd root> <art dir> <iwadinfo> <out dir>\n", argv[0]);
		return 2;
	}
	Paths paths;
	paths.root = argv[1];
	paths.EnsureLayout();
	// artwork comes from the build output, as on the SD card
	for (auto &e : ListDir(argv[2]))
		if (!e.isDir && (PathExtLower(e.name) == ".jpg" || PathExtLower(e.name) == ".png"))
			CopyFile(e.path, PathJoin(paths.Art(), e.name));
	std::string out = argv[4];
	MakeDirs(out);
	LogOpen(PathJoin(out, "preview.log"), true);

	HostPlatform plat;
	AppInfo info;
	info.version = "RDoom 1.0.0";
	info.engineVersion = "UZDoom 5.0.3 (7910df71)";
	info.gfxVersion = "Mesa 26.2.4 NVK (Vulkan 1.4)";
	info.buildDate = "host preview";
	info.enginePk3 = argv[3];
	info.lastSessionFile = PathJoin(paths.Logs(), "engine-session.txt");
	LauncherApp app(plat, paths, info);
	app.Init();

	struct Step { uint32_t buttons; const char *shot; };
	std::vector<Step> script = {
		{ 0, "01-home" },
		{ BtnR, "02-home-next-profile" },
		{ BtnL, nullptr },
		{ BtnDown, nullptr }, { BtnA, "03-profiles" },
		{ BtnRight, nullptr }, { BtnMinus, "04-profile-actions" }, { BtnB, nullptr },
		{ BtnX, "05-editor" },
		{ BtnDown, nullptr }, { BtnDown, nullptr }, { BtnDown, nullptr }, { BtnDown, "06-editor-renderer" },
		{ BtnUp, nullptr }, { BtnA, "07-profile-mods" }, { BtnPlus, "08-mod-library" },
		{ BtnB, nullptr }, { BtnB, nullptr }, { BtnB, nullptr }, { BtnB, nullptr },
		{ BtnUp, nullptr }, { BtnDown, nullptr }, { BtnDown, nullptr }, { BtnA, nullptr }, { 0, nullptr }, { 0, nullptr }, { 0, "09-iwads" },
		{ BtnB, nullptr }, { BtnDown, nullptr }, { BtnDown, nullptr }, { BtnDown, nullptr }, { BtnA, "10-settings" },
		{ BtnDown, nullptr }, { BtnDown, nullptr }, { BtnDown, nullptr }, { BtnDown, nullptr }, { BtnDown, nullptr }, { BtnA, "10b-about" },
		{ BtnB, nullptr }, { BtnUp, nullptr }, { BtnUp, nullptr }, { BtnUp, nullptr }, { BtnUp, nullptr }, { BtnUp, nullptr },
		{ BtnB, nullptr }, { BtnDown, nullptr }, { BtnA, "11-diagnostics" },
		{ BtnB, nullptr }, { BtnUp, nullptr }, { BtnUp, nullptr }, { BtnUp, nullptr }, { BtnUp, nullptr }, { BtnUp, nullptr }, { BtnUp, nullptr },
		{ BtnA, "12-launch" },
	};
	uint32_t held = 0;
	for (auto &s : script)
	{
		for (int f = 0; f < 12; f++)  // let animations settle and thumbnails load
		{
			InputFrame in;
			in.down = f == 0 ? s.buttons & ~held : 0;
			in.held = f == 0 ? s.buttons : 0;
			in.repeat = in.down;
			plat.t += 33;
			if (!app.Frame(in))
				break;
		}
		if (s.shot)
		{
			SavePng(PathJoin(out, std::string(s.shot) + ".png"), plat.canvas.Snapshot());
			printf("saved %s\n", s.shot);
		}
	}
	if (app.FinalResult() == LauncherApp::Result::Launch)
	{
		printf("launch argv:");
		for (auto &a : app.Plan().argv)
			printf(" [%s]", a.c_str());
		printf("\n");
	}
	return 0;
}
