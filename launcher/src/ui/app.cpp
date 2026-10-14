#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "fsutil.h"
#include "log.h"
#include "text.h"

namespace rdoom {

// ---------------------------------------------------------------------------
// palette and layout
// ---------------------------------------------------------------------------

static const uint32_t kText = Rgba(242, 234, 226);
static const uint32_t kMuted = Rgba(176, 156, 146);
static const uint32_t kDim = Rgba(118, 100, 94);
static const uint32_t kAccent = Rgba(255, 106, 28);
static const uint32_t kAmber = Rgba(255, 196, 90);
static const uint32_t kRed = Rgba(232, 48, 36);
static const uint32_t kGreen = Rgba(92, 214, 128);
static const uint32_t kPanel = Rgba(16, 8, 8, 214);
static const uint32_t kPanelHi = Rgba(36, 14, 10, 230);
static const uint32_t kBorder = Rgba(255, 106, 28, 80);
static const uint32_t kVulkan = Rgba(196, 28, 44);

static const int kW = 1280, kH = 720;
static const int kHintsY = 684;

static std::string Upper(const std::string &s)
{
	std::string o = s;
	for (size_t i = 0; i < o.size(); i++)
	{
		char &c = o[i];
		if (c >= 'a' && c <= 'z')
			c = char(c - 'a' + 'A');
		// Latin-1 lower case in UTF-8 (U+00E0..U+00FE except U+00F7)
		else if ((unsigned char)c == 0xC3 && i + 1 < o.size())
		{
			unsigned char n = (unsigned char)o[i + 1];
			if (n >= 0xA0 && n <= 0xBE && n != 0xB7)
				o[i + 1] = (char)(n - 0x20);
			i++;
		}
	}
	return o;
}

static std::string HumanSize(uint64_t b)
{
	char buf[32];
	if (b >= (1ull << 30))
		snprintf(buf, sizeof(buf), "%.1f GB", b / 1073741824.0);
	else if (b >= (1ull << 20))
		snprintf(buf, sizeof(buf), "%.1f MB", b / 1048576.0);
	else
		snprintf(buf, sizeof(buf), "%llu KB", (unsigned long long)(b / 1024));
	return buf;
}

static const char *QualityLabel(const std::string &q)
{
	if (q == "low") return "Baixa";
	if (q == "high") return "Alta";
	return "RDoom — Stock Switch";
}

static std::string FpsLabel(int fps)
{
	return fps == 0 ? std::string("Sem limite") : std::to_string(fps) + " FPS";
}

static std::string ScaleLabel(float s)
{
	char buf[48];
	snprintf(buf, sizeof(buf), "%d%% (%d×%d)", (int)std::lround(s * 100), (int)std::lround(1280 * s), (int)std::lround(720 * s));
	return buf;
}

static const char *SkillName(int s)
{
	static const char *names[] = { "Escolher no menu", "1 — Muito fácil", "2 — Fácil",
	                               "3 — Normal", "4 — Difícil", "5 — Pesadelo" };
	return names[std::max(0, std::min(5, s))];
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------

LauncherApp::LauncherApp(Platform &platform, Paths paths, AppInfo info)
	: plat_(platform), paths_(paths), info_(std::move(info)), store_(paths, info_.version), thumbs_(paths)
{
}

void LauncherApp::Init()
{
	TextInit();
	now_ = plat_.NowMs();
	// first frame while the library is scanned (large pk3 files take a moment)
	{
		Canvas &c = plat_.BeginDraw();
		c.Clear(Rgba(8, 3, 3));
		LoadImageFile(PathJoin(paths_.Art(), "splash.jpg"), bgMain_);
		if (bgMain_.Valid())
			c.Blit(bgMain_, 0, 0);
		DrawTextCentered(c, Font::Body, 22, kW / 2, 560, kMuted, "Carregando biblioteca…");
		plat_.EndDraw();
		bgMain_.Release();
	}
	LoadImageFile(PathJoin(paths_.Art(), "bg_main.jpg"), bgMain_);
	LoadImageFile(PathJoin(paths_.Art(), "bg_inner.jpg"), bgInner_);
	LoadImageFile(PathJoin(paths_.Art(), "logo_300.png"), logoSmall_);
	LoadImageFile(PathJoin(paths_.Art(), "logo_400.png"), logo_);
	for (const char *t : { "freedoom2", "brutaldoom", "wadfusion", "lexicon", "compendium", "custom" })
	{
		Image img;
		if (LoadImageFile(PathJoin(paths_.Art(), std::string("card_") + t + ".jpg"), img))
			cards_[t] = std::move(img);
	}
	if (!bgMain_.Valid())
		loadWarnings_.push_back("Arte do launcher ausente em launcher/ (usando cores lisas).");
	LoadAll();
	stack_ = { Screen::Home };
}

void LauncherApp::LoadAll()
{
	std::string warn;
	LoadLauncherSettings(paths_, settings_, warn);
	if (!warn.empty())
		loadWarnings_.push_back(warn);
	defs_ = LoadIwadDefs(info_.enginePk3);
	if (defs_.empty())
		loadWarnings_.push_back("game_support.pk3 ausente: IWADs identificadas só pelo nome.");
	store_.Load();
	for (const auto &p : store_.Problems())
		loadWarnings_.push_back(p);
	Rescan();
	currentId_ = settings_.lastProfile;
	if (!store_.Find(currentId_) && !store_.All().empty())
		currentId_ = store_.All().front().id;
	ReadFile(info_.lastSessionFile, lastSession_);
}

void LauncherApp::Rescan()
{
	lib_.Scan(paths_, settings_.extraIwadDirs, settings_.extraModDirs, defs_);
	thumbs_.Clear();
}

Profile *LauncherApp::Current()
{
	return store_.FindMutable(currentId_);
}

void LauncherApp::SetCurrent(const std::string &id)
{
	currentId_ = id;
	settings_.lastProfile = id;
	std::string err;
	if (!SaveLauncherSettings(paths_, settings_, err))
		ToastMsg("Não foi possível salvar: " + err, true);
}

void LauncherApp::SaveCurrent()
{
	Profile *p = store_.FindMutable(editId_.empty() ? currentId_ : editId_);
	if (!p)
		return;
	std::string err;
	if (!store_.Save(*p, err))
		ToastMsg("Erro ao salvar o perfil: " + err, true);
}

ResolvedProfile LauncherApp::ResolveCurrent()
{
	Profile *p = Current();
	if (!p)
		return ResolvedProfile();
	return ResolveProfile(*p, lib_, defs_);
}

void LauncherApp::ToastMsg(const std::string &msg, bool error)
{
	toast_ = msg;
	toastError_ = error;
	toastUntil_ = now_ + 3200;
	if (error)
		RLOG_W("ui", "%s", msg.c_str());
}

std::vector<std::string> LauncherApp::ProfilesUsing(const std::string &ref, bool iwad) const
{
	std::vector<std::string> out;
	for (const auto &p : store_.All())
	{
		if (iwad)
		{
			if (ToLower(PathFileName(p.iwad)) == ToLower(PathFileName(ref)))
				out.push_back(p.name);
			else if (p.iwad.empty())
				for (const auto &c : p.iwadCandidates)
					if (ToLower(c) == ToLower(PathFileName(ref)))
						out.push_back(p.name + " (modelo)");
		}
		else
		{
			for (const auto &m : p.mods)
				if (ToLower(PathFileName(m.file)) == ToLower(PathFileName(ref)))
					out.push_back(p.name);
		}
	}
	return out;
}

std::string LauncherApp::RendererLine(const Profile &p) const
{
	return RendererLabel(p.video.renderer);
}

// ---------------------------------------------------------------------------
// main loop
// ---------------------------------------------------------------------------

LauncherApp::Result LauncherApp::Run(LaunchPlan &out)
{
	Init();
	InputFrame in;
	while (!finished_)
	{
		if (!plat_.Poll(in))
		{
			result_ = Result::Exit;
			break;
		}
		if (!Frame(in))
			break;
	}
	out = plan_;
	return result_;
}

bool LauncherApp::Frame(const InputFrame &in)
{
	now_ = plat_.NowMs();
	if (dlg_.active)
		UpdateDialog(in);
	else
		Update(in);
	if (finished_)
		return false;
	thumbs_.Step();
	Canvas &c = plat_.BeginDraw();
	Draw(c);
	plat_.EndDraw();
	return !finished_;
}

void LauncherApp::Push(Screen s)
{
	stack_.push_back(s);
	State &st = states_[s];
	st.anim = 0;
}

void LauncherApp::Pop()
{
	if (stack_.size() > 1)
		stack_.pop_back();
	moving_ = false;
}

void LauncherApp::Update(const InputFrame &in)
{
	switch (stack_.back())
	{
	case Screen::Home: UpdateHome(in); break;
	case Screen::Profiles: UpdateProfiles(in); break;
	case Screen::Editor: UpdateEditor(in); break;
	case Screen::Iwads: UpdateIwads(in); break;
	case Screen::Mods: UpdateMods(in); break;
	case Screen::ModPicker: UpdateModPicker(in); break;
	case Screen::Settings: UpdateSettings(in); break;
	case Screen::Diagnostics: UpdateDiagnostics(in); break;
	}
}

void LauncherApp::Draw(Canvas &c)
{
	const Screen s = stack_.back();
	const Image &bg = s == Screen::Home ? bgMain_ : (bgInner_.Valid() ? bgInner_ : bgMain_);
	if (bg.Valid())
		c.Blit(bg, 0, 0);
	else
		c.GradientV(0, 0, kW, kH, Rgba(30, 8, 6), Rgba(6, 2, 2));
	switch (s)
	{
	case Screen::Home: DrawHome(c); break;
	case Screen::Profiles: DrawProfiles(c); break;
	case Screen::Editor: DrawEditor(c); break;
	case Screen::Iwads: DrawIwads(c); break;
	case Screen::Mods: DrawMods(c); break;
	case Screen::ModPicker: DrawModPicker(c); break;
	case Screen::Settings: DrawSettings(c); break;
	case Screen::Diagnostics: DrawDiagnostics(c); break;
	}
	DrawToast(c);
	if (dlg_.active)
		DrawDialog(c);
}

void LauncherApp::MoveList(int &sel, int count, const InputFrame &in, int page)
{
	if (count <= 0)
	{
		sel = 0;
		return;
	}
	if (in.repeat & BtnUp) sel = sel > 0 ? sel - 1 : count - 1;
	if (in.repeat & BtnDown) sel = sel < count - 1 ? sel + 1 : 0;
	if (in.repeat & BtnZL) sel = std::max(0, sel - page);
	if (in.repeat & BtnZR) sel = std::min(count - 1, sel + page);
	sel = std::max(0, std::min(count - 1, sel));
}

void LauncherApp::KeepVisible(State &st, int count, int visible)
{
	if (st.sel < st.scroll)
		st.scroll = st.sel;
	if (st.sel >= st.scroll + visible)
		st.scroll = st.sel - visible + 1;
	st.scroll = std::max(0, std::min(st.scroll, std::max(0, count - visible)));
}

// ---------------------------------------------------------------------------
// widgets
// ---------------------------------------------------------------------------

void LauncherApp::DrawPanel(Canvas &c, int x, int y, int w, int h, bool bright)
{
	c.Fill(x, y, w, h, bright ? kPanelHi : kPanel);
	c.Frame(x, y, w, h, kBorder, 1);
	c.Brackets(x - 3, y - 3, w + 6, h + 6, 18, 3, WithAlpha(kAccent, 200));
}

void LauncherApp::DrawChip(Canvas &c, int x, int y, const std::string &text, uint32_t bg, uint32_t fg, int px)
{
	const int w = TextWidth(Font::Bold, px, text) + 18;
	const int h = TextLineHeight(Font::Bold, px) + 2;
	c.FillRound(x, y, w, h, h / 2, bg);
	DrawText(c, Font::Bold, px, x + 9, y + 2, fg, text);
}

void LauncherApp::DrawScrollbar(Canvas &c, int x, int y, int h, int total, int visible, int first)
{
	if (total <= visible)
		return;
	c.Fill(x, y, 4, h, Rgba(255, 255, 255, 24));
	int th = std::max(24, h * visible / total);
	int ty = y + (h - th) * first / std::max(1, total - visible);
	c.Fill(x, ty, 4, th, WithAlpha(kAccent, 200));
}

static void DrawButtonGlyph(Canvas &c, int x, int y, const std::string &label)
{
	const bool round = label.size() == 1 && (label == "A" || label == "B" || label == "X" || label == "Y");
	const int w = round ? 26 : std::max(30, TextWidth(Font::Bold, 15, label) + 14);
	if (round)
		c.FillCircle(x + 13, y + 13, 13, Rgba(236, 228, 220));
	else
		c.FillRound(x, y, w, 26, 8, Rgba(236, 228, 220));
	DrawTextCentered(c, Font::Bold, 15, x + w / 2, y + 3, Rgba(20, 10, 10), label);
}

void LauncherApp::DrawHints(Canvas &c, const std::vector<std::pair<std::string, std::string>> &hints)
{
	c.GradientV(0, kHintsY - 8, kW, kH - kHintsY + 8, Rgba(0, 0, 0, 0), Rgba(0, 0, 0, 230));
	c.Fill(0, kHintsY - 1, kW, 1, WithAlpha(kAccent, 70));
	int x = kW - 40;
	for (auto it = hints.rbegin(); it != hints.rend(); ++it)
	{
		int tw = TextWidth(Font::Body, 18, it->second);
		x -= tw;
		DrawText(c, Font::Body, 18, x, kHintsY + 6, kText, it->second);
		const int gw = (it->first.size() == 1 && std::string("ABXY").find(it->first) != std::string::npos)
			? 26 : std::max(30, TextWidth(Font::Bold, 15, it->first) + 14);
		x -= gw + 8;
		DrawButtonGlyph(c, x, kHintsY + 6, it->first);
		x -= 26;
	}
	DrawText(c, Font::Body, 16, 40, kHintsY + 9, kDim, info_.version + "  •  " + info_.engineVersion);
}

void LauncherApp::DrawHeader(Canvas &c, const std::string &title, const std::string &subtitle)
{
	c.GradientV(0, 0, kW, 120, Rgba(0, 0, 0, 200), Rgba(0, 0, 0, 0));
	if (logoSmall_.Valid())
		c.Blit(logoSmall_, kW - logoSmall_.w - 36, 22, 230);
	c.Fill(48, 40, 6, 58, kAccent);
	DrawText(c, Font::Display, 56, 68, 30, kText, Upper(title));
	if (!subtitle.empty())
		DrawText(c, Font::Body, 18, 70, 92, kMuted, subtitle, 820);
}

void LauncherApp::DrawProfileCard(Canvas &c, const Profile &p, int x, int y, int w, int h, bool selected)
{
	auto it = cards_.find(p.templateId);
	if (it == cards_.end())
		it = cards_.find("custom");
	if (it != cards_.end())
	{
		const Image &card = it->second;
		if (card.w == w && card.h == h)
			c.Blit(card, x, y);
		else
		{
			Image scaled = CoverImage(card, w, h);
			c.Blit(scaled, x, y);
		}
	}
	else
		c.GradientV(x, y, w, h, Rgba(40, 12, 8), Rgba(10, 4, 4));
	c.GradientV(x, y + h / 2, w, h - h / 2, Rgba(0, 0, 0, 0), Rgba(0, 0, 0, 200));
	if (selected)
	{
		c.Frame(x - 3, y - 3, w + 6, h + 6, kAccent, 3);
		c.Brackets(x - 9, y - 9, w + 18, h + 18, 22, 3, kAmber);
	}
	else
		c.Frame(x, y, w, h, Rgba(255, 255, 255, 30), 1);
}

void LauncherApp::DrawToast(Canvas &c)
{
	if (toast_.empty() || now_ > toastUntil_)
		return;
	const int w = std::min(900, TextWidth(Font::Body, 20, toast_) + 48);
	const int x = (kW - w) / 2, y = kHintsY - 70;
	c.FillRound(x, y, w, 46, 10, toastError_ ? Rgba(110, 16, 12, 235) : Rgba(24, 14, 12, 235));
	c.Fill(x, y, 5, 46, toastError_ ? kRed : kAccent);
	DrawText(c, Font::Body, 20, x + 24, y + 11, kText, toast_, w - 40);
}

// ---------------------------------------------------------------------------
// dialogs
// ---------------------------------------------------------------------------

void LauncherApp::Ask(const std::string &title, const std::string &body, std::vector<std::string> buttons, std::function<void(int)> done)
{
	dlg_ = Dialog();
	dlg_.active = true;
	dlg_.title = title;
	dlg_.body = body;
	dlg_.buttons = std::move(buttons);
	dlg_.done = std::move(done);
}

void LauncherApp::Choose(const std::string &title, std::vector<std::string> items, std::function<void(int)> done)
{
	dlg_ = Dialog();
	dlg_.active = true;
	dlg_.title = title;
	dlg_.items = std::move(items);
	dlg_.done = std::move(done);
}

void LauncherApp::Message(const std::string &title, const std::string &body)
{
	Ask(title, body, { "OK" }, nullptr);
}

bool LauncherApp::Keyboard(const std::string &title, const std::string &initial, size_t maxLen, std::string &out)
{
	return plat_.TextInput(title, initial, maxLen, out);
}

void LauncherApp::UpdateDialog(const InputFrame &in)
{
	Dialog &d = dlg_;
	int result = -2;
	if (!d.items.empty())
	{
		MoveList(d.sel, (int)d.items.size(), in);
		if (in.down & BtnA) result = d.sel;
	}
	else
	{
		if (in.repeat & BtnLeft) d.sel = std::max(0, d.sel - 1);
		if (in.repeat & BtnRight) d.sel = std::min((int)d.buttons.size() - 1, d.sel + 1);
		if (in.down & BtnA) result = d.sel;
	}
	if (in.down & BtnB) result = -1;
	if (result != -2)
	{
		auto done = d.done;
		d.active = false;
		if (done)
			done(result);
	}
}

void LauncherApp::DrawDialog(Canvas &c)
{
	Dialog &d = dlg_;
	c.Fill(0, 0, kW, kH, Rgba(0, 0, 0, 150));
	const int w = 720;
	const bool list = !d.items.empty();
	const int visible = std::min((int)d.items.size(), 8);
	int bodyH = 0;
	if (!d.body.empty())
	{
		// measure by drawing into nothing: reuse wrapped height estimate
		Canvas probe(1, 1);
		bodyH = DrawTextWrapped(probe, Font::Body, 20, 0, 0, w - 64, kText, d.body, 12);
	}
	const int h = 96 + bodyH + (list ? visible * 52 + 16 : 78);
	const int x = (kW - w) / 2, y = std::max(40, (kH - h) / 2);
	DrawPanel(c, x, y, w, h, true);
	DrawText(c, Font::Display, 40, x + 32, y + 22, kAccent, Upper(d.title), w - 64);
	int cy = y + 78;
	if (!d.body.empty())
		cy += DrawTextWrapped(c, Font::Body, 20, x + 32, cy, w - 64, kText, d.body, 12) + 8;
	if (list)
	{
		if (d.sel < d.scroll) d.scroll = d.sel;
		if (d.sel >= d.scroll + visible) d.scroll = d.sel - visible + 1;
		for (int i = 0; i < visible; i++)
		{
			const int idx = d.scroll + i;
			const bool sel = idx == d.sel;
			const int ry = cy + i * 52;
			if (sel)
			{
				c.GradientH(x + 24, ry, w - 48, 46, WithAlpha(kAccent, 170), WithAlpha(kAccent, 20));
				c.Fill(x + 24, ry, 5, 46, kAmber);
			}
			DrawText(c, Font::Body, 21, x + 44, ry + 10, sel ? Rgba(255, 255, 255) : kText, d.items[idx], w - 100);
		}
		DrawScrollbar(c, x + w - 18, cy, visible * 52, (int)d.items.size(), visible, d.scroll);
	}
	else
	{
		int bx = x + w - 32;
		for (int i = (int)d.buttons.size() - 1; i >= 0; i--)
		{
			const int bw = std::max(140, TextWidth(Font::Display, 28, Upper(d.buttons[i])) + 48);
			bx -= bw;
			const bool sel = i == d.sel;
			c.FillRound(bx, y + h - 66, bw, 46, 8, sel ? kAccent : Rgba(255, 255, 255, 26));
			DrawTextCentered(c, Font::Display, 28, bx + bw / 2, y + h - 60, sel ? Rgba(20, 6, 4) : kText, Upper(d.buttons[i]));
			bx -= 16;
		}
	}
}

// ---------------------------------------------------------------------------
// launch
// ---------------------------------------------------------------------------

void LauncherApp::TryLaunch()
{
	Profile *p = Current();
	if (!p)
	{
		Message("Nenhum perfil", "Crie ou selecione um perfil antes de iniciar.");
		return;
	}
	ResolvedProfile r = ResolveProfile(*p, lib_, defs_);
	std::string errors, warnings;
	for (const auto &i : r.issues)
	{
		if (i.level == IssueLevel::Error)
			errors += "• " + i.text + "\n";
		else if (i.level == IssueLevel::Warning)
			warnings += "• " + i.text + "\n";
	}
	if (!r.runnable)
	{
		Message(r.pendingInstall ? "Pendente de instalação" : "Não é possível iniciar", errors);
		return;
	}
	// mods that declare the same cvar stop UZDoom at start-up ("cvar ... already exists")
	{
		std::vector<std::pair<std::string, std::string>> mods;
		for (const LibraryItem *m : r.mods)
			if (m)
				mods.push_back({ m->info.fileName, m->info.path });
		for (const auto &clash : FindCvarClashes(mods))
			warnings += "• " + clash + ": esses mods provavelmente não rodam juntos (o jogo para com erro). Se forem episódios ou versões do mesmo mod, ative só um.\n";
	}
	auto go = [this, r]() {
		Profile *p = Current();
		if (!p)
			return;
		// template choices become part of the profile once it is played
		if (p->iwad.empty() || (p->mods.empty() && !r.filled.mods.empty()))
		{
			p->iwad = r.filled.iwad;
			p->mods = r.filled.mods;
			std::string err;
			store_.Save(*p, err);
		}
		std::string err;
		if (!PrepareEngineIni(*p, paths_, err))
		{
			Message("Erro ao preparar o perfil", err);
			return;
		}
		MakeDirs(paths_.SaveDir(p->id));
		SetCurrent(p->id);
		// the engine's loading screen shows the profile over a dimmed backdrop
		WriteFileAtomic(PathJoin(paths_.Cache(), "loadinfo.txt"), p->name + "\n");
		const std::string bgFile = PathJoin(paths_.Cache(), "loadbg.rgba");
		if (!FileExists(bgFile) && bgMain_.Valid() && bgMain_.w == 1280 && bgMain_.h == 720)
		{
			std::string raw((const char *)bgMain_.px.data(), bgMain_.px.size() * 4);
			for (size_t i = 0; i < raw.size(); i += 4)
				for (int k = 0; k < 3; k++)
					raw[i + k] = (char)((unsigned char)raw[i + k] * 3 / 5);
			WriteFileAtomic(bgFile, raw);
		}
		plan_ = BuildLaunchPlan(*p, r, paths_);
		std::string line;
		for (const auto &a : plan_.argv)
			line += (line.empty() ? "" : " ") + (a.find(' ') != std::string::npos ? "\"" + a + "\"" : a);
		RLOG_I("launch", "profile %s (%s), renderer %s", p->id.c_str(), p->name.c_str(), RendererLabel(p->video.renderer));
		RLOG_I("launch", "argv: %s", line.c_str());
		WriteFileAtomic(PathJoin(paths_.Logs(), "last-launch.txt"),
		                "profile=" + p->id + "\nrenderer=" + RendererId(p->video.renderer) + "\nargv=" + line + "\n");
		result_ = Result::Launch;
		finished_ = true;
	};
	if (!warnings.empty())
		Ask("Avisos", warnings + "\nIniciar mesmo assim?", { "Cancelar", "Iniciar" }, [go](int b) { if (b == 1) go(); });
	else
		go();
}

// ---------------------------------------------------------------------------
// Home
// ---------------------------------------------------------------------------

static const char *kHomeItems[] = {
	"Iniciar", "Perfis", "Biblioteca de IWADs", "Biblioteca de mods", "Editar perfil", "Configurações", "Diagnóstico", "Sair",
};
static const int kHomeCount = 8;

void LauncherApp::UpdateHome(const InputFrame &in)
{
	State &st = states_[Screen::Home];
	MoveList(st.sel, kHomeCount, in);
	const auto &all = store_.All();
	if ((in.repeat & (BtnL | BtnR)) && !all.empty())
	{
		int idx = 0;
		for (int i = 0; i < (int)all.size(); i++)
			if (all[i].id == currentId_)
				idx = i;
		idx = (idx + ((in.repeat & BtnR) ? 1 : -1) + (int)all.size()) % (int)all.size();
		SetCurrent(all[idx].id);
	}
	if (in.down & BtnPlus)
	{
		TryLaunch();
		return;
	}
	if (in.down & BtnMinus)
	{
		editId_ = currentId_;
		Push(Screen::Editor);
		return;
	}
	if (in.down & BtnA)
	{
		switch (st.sel)
		{
		case 0: TryLaunch(); break;
		case 1: Push(Screen::Profiles); break;
		case 2: pickIwad_ = false; Push(Screen::Iwads); break;
		case 3: Push(Screen::ModPicker); editId_.clear(); break;
		case 4: editId_ = currentId_; Push(Screen::Editor); break;
		case 5: Push(Screen::Settings); break;
		case 6: Push(Screen::Diagnostics); break;
		case 7:
			Ask("Sair", "Fechar o RDoom e voltar ao menu de homebrew?", { "Cancelar", "Sair" }, [this](int b) {
				if (b == 1)
				{
					result_ = Result::Exit;
					finished_ = true;
				}
			});
			break;
		}
	}
	if (in.down & BtnB)
		Ask("Sair", "Fechar o RDoom e voltar ao menu de homebrew?", { "Cancelar", "Sair" }, [this](int b) {
			if (b == 1)
			{
				result_ = Result::Exit;
				finished_ = true;
			}
		});
}

void LauncherApp::DrawHome(Canvas &c)
{
	State &st = states_[Screen::Home];
	// logo and tagline
	if (logo_.Valid())
		c.Blit(logo_, 22, 14);
	else
		DrawText(c, Font::Display, 110, 48, 20, kAccent, "RDoom");
	DrawText(c, Font::Body, 16, 50, 150, kMuted, "UZDoom para Nintendo Switch  •  Vulkan  •  clocks de fábrica");

	// menu
	const int mx = 48, my = 232, mh = 50;
	const float target = (float)st.sel;
	hl_ += (target - hl_) * 0.35f;
	if (std::fabs(target - hl_) < 0.01f)
		hl_ = target;
	const int hy = my + (int)std::lround(hl_ * mh);
	c.GradientH(mx, hy, 330, mh - 6, WithAlpha(kAccent, 190), WithAlpha(kAccent, 0));
	c.Fill(mx, hy, 6, mh - 6, kAmber);
	for (int i = 0; i < kHomeCount; i++)
	{
		const bool sel = i == st.sel;
		const uint32_t col = sel ? Rgba(255, 255, 255) : (i == 0 ? kAmber : kText);
		DrawText(c, Font::Display, 36, mx + 24, my + i * mh + 2, col, Upper(kHomeItems[i]));
	}

	// active profile panel
	const int px = 418, py = 112, pw = 822, ph = 552;
	DrawPanel(c, px, py, pw, ph);
	Profile *p = Current();
	if (!p)
	{
		DrawTextWrapped(c, Font::Body, 22, px + 32, py + 40, pw - 64, kText, "Nenhum perfil. Abra PERFIS e crie um a partir de um modelo.");
		return;
	}
	ResolvedProfile r = ResolveProfile(*p, lib_, defs_);
	DrawText(c, Font::Body, 16, px + 28, py + 18, kMuted, "PERFIL ATIVO");
	DrawTextRight(c, Font::Body, 16, px + pw - 28, py + 18, kDim, "L / R para trocar");
	DrawProfileCard(c, *p, px + 28, py + 48, 384, 216, false);
	// IWAD thumbnail (TITLEPIC from the IWAD itself) over the card corner
	if (r.iwad)
		if (const Image *t = thumbs_.Get(*r.iwad, 128, 80))
		{
			c.Fill(px + 28 + 384 - 140, py + 48 + 216 - 92, 136, 88, Rgba(0, 0, 0, 200));
			c.Blit(*t, px + 28 + 384 - 136, py + 48 + 216 - 88);
		}
	const int tx = px + 436, tw = pw - 436 - 28;
	int ty = py + 44;
	ty += DrawTextWrapped(c, Font::Display, 46, tx, ty, tw, kText, Upper(p->name), 2, -6);
	ty += 6;
	DrawTextWrapped(c, Font::Body, 18, tx, ty, tw, kMuted, p->description, 4);

	// details
	int dy = py + 288;
	auto row = [&](const std::string &label, const std::string &value, uint32_t col) {
		DrawText(c, Font::Body, 16, px + 28, dy + 3, kDim, Upper(label));
		DrawText(c, Font::Body, 20, px + 190, dy, col, value, pw - 190 - 28);
		dy += 34;
	};
	std::string iwadText = r.iwad ? r.iwad->info.title + "  (" + PathFileName(r.iwad->ref) + ")"
	                              : (p->iwad.empty() ? std::string("—") : p->iwad + "  [ausente]");
	row("IWAD", iwadText, r.iwad ? kText : kRed);
	std::string modsText;
	int enabled = 0;
	for (const auto *m : r.mods)
	{
		enabled++;
		modsText += (modsText.empty() ? "" : "  ") + std::to_string(enabled) + ". " + PathFileName(m->ref);
	}
	row("Mods", enabled ? std::to_string(enabled) + " ativo(s): " + modsText : std::string("Nenhum"), kText);
	// renderer line with the badge
	DrawText(c, Font::Body, 16, px + 28, dy + 3, kDim, "RENDERIZADOR");
	if (p->video.renderer == Renderer::Vulkan)
	{
		DrawChip(c, px + 190, dy - 1, "VULKAN", kVulkan, Rgba(255, 255, 255), 18);
		DrawText(c, Font::Body, 18, px + 300, dy + 1, kMuted, "Mesa NVK nativo • padrão");
	}
	else
	{
		DrawChip(c, px + 190, dy - 1, Upper(RendererLabel(p->video.renderer)), Rgba(80, 70, 66), kText, 18);
		DrawText(c, Font::Body, 18, px + 340, dy + 1, kAmber, "secundário (Vulkan é o recomendado)");
	}
	dy += 34;
	row("Vídeo", ScaleLabel(p->video.internalScale) + "  •  " + FpsLabel(p->video.fpsLimit) + "  •  " + QualityLabel(p->video.quality), kText);

	// status
	dy += 6;
	int shown = 0;
	for (const auto &i : r.issues)
	{
		if (i.level == IssueLevel::Info || shown >= 2)
			continue;
		c.FillCircle(px + 36, dy + 12, 6, i.level == IssueLevel::Error ? kRed : kAmber);
		DrawText(c, Font::Body, 18, px + 52, dy, i.level == IssueLevel::Error ? Rgba(255, 150, 140) : kAmber, i.text, pw - 52 - 300);
		dy += 28;
		shown++;
	}
	if (r.runnable && shown == 0)
	{
		c.FillCircle(px + 36, dy + 12, 6, kGreen);
		DrawText(c, Font::Body, 18, px + 52, dy, kGreen, "Pronto para jogar");
	}

	// start button
	const int bx = px + pw - 300, by = py + ph - 84, bw = 272, bh = 60;
	const float pulse = 0.5f + 0.5f * std::sin(now_ / 380.0f);
	const bool can = r.runnable;
	if (can)
		c.FillRound(bx - 6, by - 6, bw + 12, bh + 12, 14, WithAlpha(kAccent, (int)(50 + 60 * pulse)));
	c.FillRound(bx, by, bw, bh, 10, can ? kAccent : Rgba(90, 60, 54));
	c.GradientV(bx + 4, by + 4, bw - 8, bh / 2 - 4, Rgba(255, 255, 255, 60), Rgba(255, 255, 255, 0));
	DrawTextCentered(c, Font::Display, 40, bx + bw / 2 + 14, by + 9, can ? Rgba(24, 6, 2) : kMuted, can ? "INICIAR" : "PENDENTE");
	DrawButtonGlyph(c, bx + 18, by + 17, "+");

	// warnings banner (applet mode, load problems)
	SystemInfo si = plat_.Info();
	std::string banner;
	if (si.appletMode)
		banner = "Modo applet: pouca memória. Abra o RDoom segurando R sobre um jogo (modo título).";
	else if (!loadWarnings_.empty())
		banner = loadWarnings_.front();
	if (!banner.empty())
	{
		c.FillRound(418, 62, 822, 38, 8, Rgba(90, 30, 8, 220));
		DrawText(c, Font::Body, 17, 434, 70, kAmber, banner, 790);
	}

	DrawHints(c, { { "A", "Selecionar" }, { "L/R", "Perfil" }, { "-", "Editar" }, { "+", "Iniciar" } });
}

// ---------------------------------------------------------------------------
// Profiles
// ---------------------------------------------------------------------------

void LauncherApp::UpdateProfiles(const InputFrame &in)
{
	State &st = states_[Screen::Profiles];
	const auto &all = store_.All();
	const int n = (int)all.size(), cols = 3;
	if (in.repeat & BtnLeft) st.sel = std::max(0, st.sel - 1);
	if (in.repeat & BtnRight) st.sel = std::min(n - 1, st.sel + 1);
	if (in.repeat & BtnUp) st.sel = std::max(0, st.sel - cols);
	if (in.repeat & BtnDown) st.sel = std::min(n - 1, st.sel + cols);
	st.sel = std::max(0, std::min(std::max(0, n - 1), st.sel));
	int row = st.sel / cols;
	if (row < st.scroll) st.scroll = row;
	if (row >= st.scroll + 2) st.scroll = row - 1;

	if (in.down & BtnB)
	{
		Pop();
		return;
	}
	if (in.down & BtnY)
	{
		std::vector<std::string> names;
		auto t = BuiltinTemplates();
		for (auto &p : t)
			names.push_back(p.name);
		names.push_back("A partir do perfil atual");
		Choose("Novo perfil", names, [this, t](int i) {
			if (i < 0)
				return;
			std::string err;
			Profile created;
			if (i == (int)t.size())
			{
				if (!store_.Duplicate(currentId_, created, err))
				{
					ToastMsg(err, true);
					return;
				}
			}
			else
			{
				std::string name = t[i].name;
				Keyboard("Nome do perfil", name, 48, name);
				if (!store_.Create(t[i].templateId, name, created, err))
				{
					ToastMsg(err, true);
					return;
				}
			}
			ToastMsg("Perfil criado: " + created.name);
			SetCurrent(created.id);
			editId_ = created.id;
			Push(Screen::Editor);
		});
		return;
	}
	if (n == 0)
		return;
	const Profile sel = all[st.sel];
	if (in.down & BtnA)
	{
		SetCurrent(sel.id);
		ToastMsg("Perfil ativo: " + sel.name);
		Pop();
		return;
	}
	if (in.down & BtnX)
	{
		editId_ = sel.id;
		Push(Screen::Editor);
		return;
	}
	if (in.down & BtnMinus)
	{
		Choose(sel.name, { "Editar", "Duplicar", "Renomear", "Restaurar padrões", "Exportar", "Importar perfil…", "Excluir" },
		       [this, sel](int a) {
			std::string err;
			Profile out;
			switch (a)
			{
			case 0: editId_ = sel.id; Push(Screen::Editor); break;
			case 1:
				if (store_.Duplicate(sel.id, out, err)) ToastMsg("Duplicado: " + out.name);
				else ToastMsg(err, true);
				break;
			case 2: {
				std::string name = sel.name;
				if (Keyboard("Novo nome", sel.name, 48, name) && name != sel.name)
				{
					if (store_.Rename(sel.id, name, err)) ToastMsg("Renomeado");
					else ToastMsg(err, true);
				}
				break;
			}
			case 3:
				Ask("Restaurar padrões", "Voltar todas as opções de \"" + sel.name + "\" ao padrão? IWAD e mods são mantidos. Uma cópia vai para backup/.",
				    { "Cancelar", "Restaurar" }, [this, sel](int b) {
					std::string e;
					if (b == 1)
					{
						if (store_.RestoreDefaults(sel.id, e)) ToastMsg("Padrões restaurados");
						else ToastMsg(e, true);
					}
				});
				break;
			case 4: {
				std::string path;
				if (store_.Export(sel.id, path, err)) Message("Exportado", "Arquivo salvo em:\n" + path);
				else ToastMsg(err, true);
				break;
			}
			case 5: {
				std::vector<std::string> files;
				for (auto &e : ListDir(paths_.Export()))
					if (!e.isDir && PathExtLower(e.name) == ".json")
						files.push_back(e.name);
				if (files.empty())
				{
					Message("Importar", "Coloque arquivos .json de perfil em\n" + paths_.Export());
					break;
				}
				Choose("Importar de export/", files, [this, files](int i) {
					if (i < 0) return;
					std::string e;
					Profile p;
					if (store_.Import(PathJoin(paths_.Export(), files[i]), p, e)) ToastMsg("Importado: " + p.name);
					else Message("Falha ao importar", e);
				});
				break;
			}
			case 6:
				Ask("Excluir perfil", "Excluir \"" + sel.name + "\"? Os arquivos de IWAD, mods e saves NÃO são apagados; uma cópia do perfil vai para backup/.",
				    { "Cancelar", "Excluir" }, [this, sel](int b) {
					std::string e;
					if (b != 1) return;
					if (store_.Remove(sel.id, e))
					{
						ToastMsg("Perfil excluído");
						if (currentId_ == sel.id && !store_.All().empty())
							SetCurrent(store_.All().front().id);
					}
					else ToastMsg(e, true);
				});
				break;
			default: break;
			}
		});
	}
}

void LauncherApp::DrawProfiles(Canvas &c)
{
	State &st = states_[Screen::Profiles];
	DrawHeader(c, "Perfis", std::to_string(store_.All().size()) + " perfis • cada um com IWAD, mods, configuração e saves próprios");
	const auto &all = store_.All();
	const int cols = 3, cw = 368, ch = 207, gx = 30, gy = 92;
	const int x0 = 48, y0 = 150;
	for (int i = 0; i < (int)all.size(); i++)
	{
		const int row = i / cols - st.scroll;
		if (row < 0 || row > 1)
			continue;
		const int x = x0 + (i % cols) * (cw + gx), y = y0 + row * (ch + gy);
		const Profile &p = all[i];
		const bool sel = i == st.sel;
		DrawProfileCard(c, p, x, y, cw, ch, sel);
		ResolvedProfile r = ResolveProfile(p, lib_, defs_);
		DrawText(c, Font::Display, 34, x + 18, y + ch - 52, kText, Upper(p.name), cw - 36);
		std::string sub = RendererLabel(p.video.renderer);
		sub += r.iwad ? "  •  " + PathFileName(r.iwad->ref) : "";
		DrawText(c, Font::Body, 16, x + 18, y + ch - 18, kMuted, sub, cw - 36);
		if (p.id == currentId_)
			DrawChip(c, x + 14, y + 12, "ATIVO", kAccent, Rgba(20, 6, 2), 15);
		if (!r.runnable)
		{
			const bool unset = p.iwad.empty() && p.iwadCandidates.empty();
			const char *chip = r.pendingInstall ? "PENDENTE" : unset ? "CONFIGURAR" : "ERRO";
			DrawChip(c, x + cw - TextWidth(Font::Bold, 15, chip) - 34, y + 12, chip, r.pendingInstall || unset ? Rgba(120, 80, 10) : Rgba(130, 20, 16), kText, 15);
		}
		if (sel)
		{
			// first problem of the selected profile, under the grid
			for (auto &is : r.issues)
				if (is.level != IssueLevel::Info)
				{
					DrawText(c, Font::Body, 18, 52, 640, is.level == IssueLevel::Error ? Rgba(255, 150, 140) : kAmber, is.text, 1180);
					break;
				}
		}
	}
	DrawHints(c, { { "A", "Ativar" }, { "X", "Editar" }, { "Y", "Novo" }, { "-", "Ações" }, { "B", "Voltar" } });
}

} // namespace rdoom
