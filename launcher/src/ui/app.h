// RDoom launcher - the interactive front end shown before the engine starts.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "canvas.h"
#include "launch.h"
#include "library.h"
#include "paths.h"
#include "platform.h"
#include "profile.h"
#include "thumbs.h"

namespace rdoom {

struct AppInfo
{
	std::string version;        // "RDoom 1.0.0"
	std::string engineVersion;  // "UZDoom 5.0.3 (7910df71)"
	std::string gfxVersion;     // "Mesa 26.2.4 NVK"
	std::string buildDate;
	std::string enginePk3;      // game_support.pk3 path (iwadinfo.txt)
	std::string lastSessionFile;// engine session report (logs/engine-session.txt)
};

class LauncherApp
{
public:
	enum class Result { Launch, Exit };
	LauncherApp(Platform &platform, Paths paths, AppInfo info);
	Result Run(LaunchPlan &out);

	// For automated previews: run N frames with scripted input.
	void Init();
	bool Frame(const InputFrame &in);  // false when finished
	Result FinalResult() const { return result_; }
	const LaunchPlan &Plan() const { return plan_; }

private:
	enum class Screen { Home, Profiles, Editor, Iwads, Mods, ModPicker, Settings, Diagnostics };
	struct State { int sel = 0; int scroll = 0; float anim = 0; };

	struct Dialog
	{
		bool active = false;
		std::string title, body;
		std::vector<std::string> buttons;   // horizontal buttons, or
		std::vector<std::string> items;     // a vertical list
		int sel = 0, scroll = 0;
		std::function<void(int)> done;      // -1 = cancelled
	};

	// data
	void LoadAll();
	void Rescan();
	Profile *Current();
	void SetCurrent(const std::string &id);
	void SaveCurrent();
	ResolvedProfile ResolveCurrent();
	void TryLaunch();
	void ToastMsg(const std::string &msg, bool error = false);
	std::vector<std::string> ProfilesUsing(const std::string &iwadRef, bool iwad) const;
	std::string RendererLine(const Profile &p) const;

	// screens
	void Push(Screen s);
	void Pop();
	void Update(const InputFrame &in);
	void Draw(Canvas &c);
	void UpdateHome(const InputFrame &in);
	void DrawHome(Canvas &c);
	void UpdateProfiles(const InputFrame &in);
	void DrawProfiles(Canvas &c);
	void UpdateEditor(const InputFrame &in);
	void DrawEditor(Canvas &c);
	void UpdateIwads(const InputFrame &in);
	void DrawIwads(Canvas &c);
	void UpdateMods(const InputFrame &in);
	void DrawMods(Canvas &c);
	void UpdateModPicker(const InputFrame &in);
	void DrawModPicker(Canvas &c);
	void UpdateSettings(const InputFrame &in);
	void DrawSettings(Canvas &c);
	void UpdateDiagnostics(const InputFrame &in);
	void DrawDiagnostics(Canvas &c);

	// widgets
	void DrawHeader(Canvas &c, const std::string &title, const std::string &subtitle);
	void DrawHints(Canvas &c, const std::vector<std::pair<std::string, std::string>> &hints);
	void DrawPanel(Canvas &c, int x, int y, int w, int h, bool bright = false);
	void DrawChip(Canvas &c, int x, int y, const std::string &text, uint32_t bg, uint32_t fg, int px = 16);
	void DrawScrollbar(Canvas &c, int x, int y, int h, int total, int visible, int first);
	void DrawProfileCard(Canvas &c, const Profile &p, int x, int y, int w, int h, bool selected);
	void DrawDialog(Canvas &c);
	void DrawToast(Canvas &c);
	void UpdateDialog(const InputFrame &in);
	void Ask(const std::string &title, const std::string &body, std::vector<std::string> buttons, std::function<void(int)> done);
	void Choose(const std::string &title, std::vector<std::string> items, std::function<void(int)> done);
	void Message(const std::string &title, const std::string &body);
	bool Keyboard(const std::string &title, const std::string &initial, size_t maxLen, std::string &out);
	static void MoveList(int &sel, int count, const InputFrame &in, int page = 6);
	static void KeepVisible(State &st, int count, int visible);

	Platform &plat_;
	Paths paths_;
	AppInfo info_;
	LauncherSettings settings_;
	ProfileStore store_;
	Library lib_;
	std::vector<IwadDef> defs_;
	ThumbCache thumbs_;
	std::string currentId_;
	std::string editId_;            // profile shown in the editor / mod list
	bool pickIwad_ = false;         // IWAD screen opened to choose one
	bool moving_ = false;           // mod list: moving the selected entry

	std::vector<Screen> stack_;
	std::map<Screen, State> states_;
	Dialog dlg_;
	std::string toast_;
	bool toastError_ = false;
	uint64_t toastUntil_ = 0;
	uint64_t now_ = 0;
	float hl_ = 0;                  // animated highlight position on the home menu

	Image bgMain_, bgInner_, logo_, logoSmall_, icon_;
	std::map<std::string, Image> cards_;
	std::vector<std::string> loadWarnings_;
	std::string lastSession_;

	bool finished_ = false;
	Result result_ = Result::Exit;
	LaunchPlan plan_;
};

} // namespace rdoom
