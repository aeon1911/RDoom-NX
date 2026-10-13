// RDoom launcher - editor, libraries, settings and diagnostics screens.
#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "fsutil.h"
#include "log.h"
#include "text.h"

namespace rdoom {

static const uint32_t kText = Rgba(242, 234, 226);
static const uint32_t kMuted = Rgba(176, 156, 146);
static const uint32_t kDim = Rgba(118, 100, 94);
static const uint32_t kAccent = Rgba(255, 106, 28);
static const uint32_t kAmber = Rgba(255, 196, 90);
static const uint32_t kRed = Rgba(232, 48, 36);
static const uint32_t kGreen = Rgba(92, 214, 128);
static const uint32_t kVulkan = Rgba(196, 28, 44);

static std::string Up(const std::string &s)
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

static std::string Size(uint64_t b)
{
	char buf[32];
	if (b >= (1ull << 30)) snprintf(buf, sizeof(buf), "%.1f GB", b / 1073741824.0);
	else if (b >= (1ull << 20)) snprintf(buf, sizeof(buf), "%.1f MB", b / 1048576.0);
	else snprintf(buf, sizeof(buf), "%llu KB", (unsigned long long)(b / 1024));
	return buf;
}

static const char *CategoryPt(ModCategory c)
{
	switch (c)
	{
	case ModCategory::MapPack: return "Pacote de mapas";
	case ModCategory::Gameplay: return "Jogabilidade";
	case ModCategory::GameplayAndMaps: return "Jogabilidade + mapas";
	case ModCategory::Resources: return "Recursos";
	case ModCategory::Dehacked: return "Patch DeHackEd";
	default: return "Desconhecido";
	}
}

// Load-order badge: the position among the enabled mods (the order of -file),
// "OFF" for a disabled one.
static void OrderBadge(Canvas &c, int x, int y, int size, int order)
{
	if (order > 0)
	{
		c.Fill(x, y, size, size, kAccent);
		DrawTextCentered(c, Font::Display, size - 12, x + size / 2, y + 5, Rgba(255, 255, 255), std::to_string(order));
	}
	else
	{
		c.Frame(x, y, size, size, kDim, 2);
		DrawTextCentered(c, Font::Bold, 14, x + size / 2, y + size / 2 - 9, kDim, "OFF");
	}
}

static void SelRow(Canvas &c, int x, int y, int w, int h, bool sel, bool moving = false)
{
	if (!sel)
		return;
	c.GradientH(x, y, w, h, WithAlpha(moving ? kAmber : kAccent, 150), WithAlpha(kAccent, 10));
	c.Fill(x, y, 5, h, moving ? Rgba(255, 255, 255) : kAmber);
}

// ---------------------------------------------------------------------------
// Profile editor
// ---------------------------------------------------------------------------

namespace {

enum EditRow
{
	ER_Name, ER_Desc, ER_Iwad, ER_Mods, ER_Renderer, ER_Quality, ER_Scale, ER_Fps, ER_Vsync, ER_Lights, ER_Filter,
	ER_Sens, ER_Dead, ER_Invert, ER_Autoaim, ER_Run, ER_Cross, ER_Skill, ER_Warp, ER_Fast, ER_NoMon, ER_Respawn,
	ER_Reset, ER_Count
};

const int kFpsSteps[] = { 30, 35, 60, 0 };
const int kFilters[] = { 0, 4, 5 };

const char *FilterName(int f)
{
	switch (f)
	{
	case 0: return "Nítido (sem filtro)";
	case 4: return "Bilinear";
	case 5: return "Trilinear";
	default: return "Personalizado";
	}
}

} // namespace

void LauncherApp::UpdateEditor(const InputFrame &in)
{
	State &st = states_[Screen::Editor];
	Profile *p = store_.FindMutable(editId_);
	if (!p)
	{
		Pop();
		return;
	}
	MoveList(st.sel, ER_Count, in, 8);
	if (in.down & BtnB)
	{
		Pop();
		return;
	}
	if (in.down & BtnPlus)
	{
		SetCurrent(p->id);
		TryLaunch();
		return;
	}
	const int dir = (in.repeat & BtnRight) ? 1 : (in.repeat & BtnLeft) ? -1 : 0;
	const bool a = (in.down & BtnA) != 0;
	bool changed = false;
	auto cycle = [&](int &v, const int *steps, int n) {
		int idx = 0;
		for (int i = 0; i < n; i++)
			if (steps[i] == v)
				idx = i;
		idx = (idx + (dir ? dir : 1) + n) % n;
		v = steps[idx];
		changed = true;
	};
	auto toggle = [&](bool &b) {
		b = !b;
		changed = true;
	};
	auto step = [&](float &v, float d, float lo, float hi) {
		int s = dir ? dir : 1;
		v = std::round((v + s * d) / d) * d;
		if (v > hi + 1e-4f) v = lo;
		if (v < lo - 1e-4f) v = hi;
		changed = true;
	};
	if (!dir && !a)
		return;
	switch (st.sel)
	{
	case ER_Name:
		if (a)
		{
			std::string name = p->name, err;
			if (Keyboard("Nome do perfil", p->name, 48, name) && !name.empty())
			{
				if (!store_.Rename(p->id, name, err))
					ToastMsg(err, true);
			}
		}
		break;
	case ER_Desc:
		if (a)
		{
			std::string d = p->description;
			if (Keyboard("Descrição", p->description, 200, d))
			{
				p->description = d;
				changed = true;
			}
		}
		break;
	case ER_Iwad:
		if (a)
		{
			pickIwad_ = true;
			Push(Screen::Iwads);
		}
		break;
	case ER_Mods:
		if (a)
		{
			// materialise the template choice so the list can be edited
			if (p->mods.empty() && !p->modPatterns.empty())
			{
				ResolvedProfile r = ResolveProfile(*p, lib_, defs_);
				p->mods = r.filled.mods;
				changed = true;
			}
			states_[Screen::Mods] = State();
			Push(Screen::Mods);
		}
		break;
	case ER_Renderer: {
		const Renderer order[] = { Renderer::Vulkan, Renderer::OpenGL, Renderer::GLES };
		int idx = 0;
		for (int i = 0; i < 3; i++)
			if (order[i] == p->video.renderer)
				idx = i;
		idx = (idx + (dir ? dir : 1) + 3) % 3;
		p->video.renderer = order[idx];
		changed = true;
		if (p->video.renderer != Renderer::Vulkan)
			ToastMsg(std::string(RendererLabel(p->video.renderer)) + " é secundário; Vulkan continua o recomendado.");
		break;
	}
	case ER_Quality: {
		const char *q[] = { "low", "stock", "high" };
		int idx = 1;
		for (int i = 0; i < 3; i++)
			if (p->video.quality == q[i])
				idx = i;
		idx = (idx + (dir ? dir : 1) + 3) % 3;
		p->video.quality = q[idx];
		changed = true;
		break;
	}
	case ER_Scale: step(p->video.internalScale, 0.05f, 0.5f, 1.0f); break;
	case ER_Fps: cycle(p->video.fpsLimit, kFpsSteps, 4); break;
	case ER_Vsync: toggle(p->video.vsync); break;
	case ER_Lights: toggle(p->video.dynamicLights); break;
	case ER_Filter: cycle(p->video.textureFilter, kFilters, 3); break;
	case ER_Sens: step(p->input.lookSensitivity, 0.25f, 0.25f, 3.0f); break;
	case ER_Dead: step(p->input.deadzone, 0.05f, 0.0f, 0.4f); break;
	case ER_Invert: toggle(p->input.invertLook); break;
	case ER_Autoaim:
		p->gameplay.autoaim = p->gameplay.autoaim > 0 ? 0.0f : 35.0f;
		changed = true;
		break;
	case ER_Run: toggle(p->gameplay.alwaysRun); break;
	case ER_Cross:
		p->gameplay.crosshair = p->gameplay.crosshair ? 0 : 1;
		changed = true;
		break;
	case ER_Skill:
		p->gameplay.skill = (p->gameplay.skill + (dir ? dir : 1) + 6) % 6;
		changed = true;
		break;
	case ER_Warp:
		if (a)
		{
			std::string w = p->gameplay.warp;
			if (Keyboard("Mapa inicial (ex.: MAP01, E1M1; vazio = menu)", p->gameplay.warp, 8, w))
			{
				if (IsValidWarp(w))
				{
					p->gameplay.warp = Up(w);
					changed = true;
				}
				else
					ToastMsg("Nome de mapa inválido (use letras e números)", true);
			}
		}
		break;
	case ER_Fast: toggle(p->gameplay.fast); break;
	case ER_NoMon: toggle(p->gameplay.noMonsters); break;
	case ER_Respawn: toggle(p->gameplay.respawn); break;
	case ER_Reset:
		if (a)
		{
			const std::string id = p->id;
			Ask("Restaurar padrões", "Todas as opções voltam ao padrão do modelo. IWAD e mods são mantidos.",
			    { "Cancelar", "Restaurar" }, [this, id](int b) {
				std::string err;
				if (b == 1)
				{
					if (store_.RestoreDefaults(id, err)) ToastMsg("Padrões restaurados");
					else ToastMsg(err, true);
				}
			});
		}
		break;
	}
	if (changed)
	{
		std::string err;
		if (!store_.Save(*p, err))
			ToastMsg("Erro ao salvar: " + err, true);
	}
}

void LauncherApp::DrawEditor(Canvas &c)
{
	State &st = states_[Screen::Editor];
	const Profile *p = store_.Find(editId_);
	if (!p)
		return;
	DrawHeader(c, "Editar perfil", p->name + "  •  alterações são salvas na hora");
	ResolvedProfile r = ResolveProfile(*p, lib_, defs_);

	struct Line { const char *section; std::string label, value; uint32_t col; };
	std::vector<Line> rows(ER_Count);
	auto onoff = [](bool b) { return std::string(b ? "Ligado" : "Desligado"); };
	char buf[64];
	rows[ER_Name] = { "PERFIL", "Nome", p->name, kText };
	rows[ER_Desc] = { nullptr, "Descrição", p->description.empty() ? std::string("—") : p->description, kMuted };
	rows[ER_Iwad] = { nullptr, "IWAD", r.iwad ? r.iwad->info.title + " (" + PathFileName(r.iwad->ref) + ")" : (p->iwad.empty() ? std::string("Escolher…") : p->iwad + " [ausente]"), r.iwad ? kText : kRed };
	int en = 0;
	for (auto &m : (p->mods.empty() ? r.filled.mods : p->mods))
		if (m.enabled)
			en++;
	rows[ER_Mods] = { nullptr, "Mods e ordem de carga", std::to_string(en) + " ativo(s)  ›", kText };
	rows[ER_Renderer] = { "VÍDEO", "Renderizador", RendererLabel(p->video.renderer), p->video.renderer == Renderer::Vulkan ? kAmber : kText };
	rows[ER_Quality] = { nullptr, "Qualidade", p->video.quality == "low" ? "Baixa" : p->video.quality == "high" ? "Alta" : "RDoom — Stock Switch", kText };
	snprintf(buf, sizeof(buf), "%d%% (%d×%d)", (int)std::lround(p->video.internalScale * 100), (int)std::lround(1280 * p->video.internalScale), (int)std::lround(720 * p->video.internalScale));
	rows[ER_Scale] = { nullptr, "Resolução interna", buf, kText };
	rows[ER_Fps] = { nullptr, "Limite de FPS", p->video.fpsLimit ? std::to_string(p->video.fpsLimit) + " FPS" : std::string("Sem limite"), kText };
	rows[ER_Vsync] = { nullptr, "VSync", onoff(p->video.vsync), kText };
	rows[ER_Lights] = { nullptr, "Luzes dinâmicas", onoff(p->video.dynamicLights), kText };
	rows[ER_Filter] = { nullptr, "Filtro de textura", FilterName(p->video.textureFilter), kText };
	snprintf(buf, sizeof(buf), "%.2fx", p->input.lookSensitivity);
	rows[ER_Sens] = { "CONTROLES", "Sensibilidade da mira", buf, kText };
	snprintf(buf, sizeof(buf), "%d%%", (int)std::lround(p->input.deadzone * 100));
	rows[ER_Dead] = { nullptr, "Zona morta dos analógicos", buf, kText };
	rows[ER_Invert] = { nullptr, "Inverter mira vertical", onoff(p->input.invertLook), kText };
	rows[ER_Autoaim] = { "JOGO", "Mira automática", onoff(p->gameplay.autoaim > 0), kText };
	rows[ER_Run] = { nullptr, "Correr sempre", onoff(p->gameplay.alwaysRun), kText };
	rows[ER_Cross] = { nullptr, "Retículo", onoff(p->gameplay.crosshair > 0), kText };
	const char *skills[] = { "Escolher no menu", "1 — Muito fácil", "2 — Fácil", "3 — Normal", "4 — Difícil", "5 — Pesadelo" };
	rows[ER_Skill] = { nullptr, "Dificuldade inicial", skills[std::max(0, std::min(5, p->gameplay.skill))], kText };
	rows[ER_Warp] = { nullptr, "Mapa inicial", p->gameplay.warp.empty() ? std::string("Menu do jogo") : p->gameplay.warp, kText };
	rows[ER_Fast] = { nullptr, "Monstros rápidos (-fast)", onoff(p->gameplay.fast), kText };
	rows[ER_NoMon] = { nullptr, "Sem monstros (-nomonsters)", onoff(p->gameplay.noMonsters), kText };
	rows[ER_Respawn] = { nullptr, "Reaparecimento (-respawn)", onoff(p->gameplay.respawn), kText };
	rows[ER_Reset] = { "MANUTENÇÃO", "Restaurar padrões do perfil", "›", kAmber };

	const int x = 48, y0 = 136, w = 760, rh = 42, visible = 12;
	KeepVisible(st, ER_Count, visible);
	DrawPanel(c, x - 12, y0 - 12, w + 24, visible * rh + 24);
	for (int i = 0; i < visible && st.scroll + i < ER_Count; i++)
	{
		const int idx = st.scroll + i;
		const int y = y0 + i * rh;
		SelRow(c, x, y, w, rh - 4, idx == st.sel);
		DrawText(c, Font::Body, 20, x + 22, y + 6, idx == st.sel ? Rgba(255, 255, 255) : kMuted, rows[idx].label);
		const bool arrows = idx >= ER_Renderer && idx <= ER_Respawn && idx != ER_Warp;
		std::string v = arrows ? "‹  " + rows[idx].value + "  ›" : rows[idx].value;
		DrawTextRight(c, Font::Bold, 20, x + w - 18, y + 6, rows[idx].col, Ellipsize(Font::Bold, 20, v, 400));
	}
	DrawScrollbar(c, x + w + 4, y0, visible * rh, ER_Count, visible, st.scroll);

	// help panel for the selected row
	const int hx = 846, hw = 386;
	DrawPanel(c, hx, y0 - 12, hw, visible * rh + 24);
	const char *section = nullptr;
	for (int i = st.sel; i >= 0 && !section; i--)
		section = rows[i].section;
	DrawText(c, Font::Display, 30, hx + 24, y0 + 4, kAccent, section ? section : "");
	std::string help;
	switch (st.sel)
	{
	case ER_Iwad: help = "A IWAD é o jogo base (DOOM II, Freedoom, WadFusion…). Coloque os arquivos em switch/rdoom/iwads/."; break;
	case ER_Mods: help = "Ative, desative e ordene os mods. A ordem importa: arquivos carregados depois sobrescrevem os anteriores."; break;
	case ER_Renderer: help = "Vulkan (Mesa NVK) é o renderizador principal do RDoom. OpenGL e OpenGL ES ficam como alternativas explícitas; o RDoom nunca troca de renderizador sozinho."; break;
	case ER_Quality: help = "RDoom — Stock Switch: sem SSAO, bloom, lens flare ou MSAA. Baixa também desliga luzes dinâmicas e limita a escala a 75%. Alta liga bloom e sombras de luz."; break;
	case ER_Scale: help = "Resolução em que o jogo é desenhado antes de ser escalado para a tela. Valores menores aliviam mods pesados."; break;
	case ER_Fps: help = "30 ou 35 FPS dão tempos de quadro mais estáveis em mods pesados; 60 FPS para conteúdo leve. O RDoom usa sempre os clocks de fábrica do console."; break;
	case ER_Sens: case ER_Dead: case ER_Invert: help = "Valores aplicados aos Joy-Con e ao Pro Controller quando o jogo inicia. Os mapeamentos de botões feitos no menu do jogo ficam salvos neste perfil."; break;
	case ER_Warp: help = "Inicia direto no mapa indicado (ex.: MAP01). Deixe vazio para abrir o menu do jogo."; break;
	default: help = "Use ‹ › para mudar valores e A para abrir. Cada perfil tem configuração e saves separados."; break;
	}
	DrawTextWrapped(c, Font::Body, 19, hx + 24, y0 + 50, hw - 48, kText, help, 12);

	// problems of this profile
	int py = y0 + visible * rh - 120;
	for (auto &is : r.issues)
	{
		if (is.level == IssueLevel::Info || py > y0 + visible * rh - 20)
			continue;
		c.FillCircle(hx + 30, py + 11, 5, is.level == IssueLevel::Error ? kRed : kAmber);
		py += DrawTextWrapped(c, Font::Body, 16, hx + 44, py, hw - 64, is.level == IssueLevel::Error ? Rgba(255, 150, 140) : kAmber, is.text, 2);
	}
	DrawHints(c, { { "A", "Abrir/Alterar" }, { "‹ ›", "Valor" }, { "+", "Iniciar" }, { "B", "Voltar" } });
}

// ---------------------------------------------------------------------------
// IWAD library
// ---------------------------------------------------------------------------

void LauncherApp::UpdateIwads(const InputFrame &in)
{
	State &st = states_[Screen::Iwads];
	const auto &items = lib_.Iwads();
	MoveList(st.sel, (int)items.size(), in);
	if (in.down & BtnB)
	{
		pickIwad_ = false;
		Pop();
		return;
	}
	if (in.down & BtnY)
	{
		Rescan();
		ToastMsg("Biblioteca atualizada: " + std::to_string(lib_.Iwads().size()) + " IWAD(s)");
		return;
	}
	if (items.empty() || !(in.down & BtnA))
		return;
	const LibraryItem &it = items[st.sel];
	if (it.info.status != FileStatus::Ok || !it.info.isIwad)
	{
		Message("Não é possível usar", it.info.status != FileStatus::Ok
			? it.info.fileName + ": arquivo inválido (" + it.info.error + ")."
			: it.info.fileName + " não é uma IWAD. Mova-o para mods/ se for um mod.");
		return;
	}
	const std::string target = pickIwad_ ? editId_ : currentId_;
	Profile *p = store_.FindMutable(target);
	if (!p)
		return;
	p->iwad = it.ref;
	std::string err;
	if (store_.Save(*p, err))
		ToastMsg("IWAD de \"" + p->name + "\": " + it.info.title);
	else
		ToastMsg(err, true);
	if (pickIwad_)
	{
		pickIwad_ = false;
		Pop();
	}
}

void LauncherApp::DrawIwads(Canvas &c)
{
	State &st = states_[Screen::Iwads];
	const auto &items = lib_.Iwads();
	DrawHeader(c, "Biblioteca de IWADs", pickIwad_ ? "Escolha a IWAD do perfil" : std::to_string(items.size()) + " arquivo(s) em " + paths_.Iwads());
	const int x = 48, y0 = 136, w = 700, rh = 82, visible = 6;
	KeepVisible(st, (int)items.size(), visible);
	DrawPanel(c, x - 12, y0 - 12, w + 24, visible * rh + 24);
	if (items.empty())
		DrawTextWrapped(c, Font::Body, 21, x + 16, y0 + 20, w - 40, kText,
		                "Nenhuma IWAD encontrada. Copie, por exemplo, freedoom2.wad ou doom2.wad para switch/rdoom/iwads/ no cartão SD e pressione Y.");
	for (int i = 0; i < visible && st.scroll + i < (int)items.size(); i++)
	{
		const int idx = st.scroll + i;
		const LibraryItem &it = items[idx];
		const int y = y0 + i * rh;
		SelRow(c, x, y, w, rh - 6, idx == st.sel);
		c.Fill(x + 16, y + 6, 104, 65, Rgba(0, 0, 0, 160));
		if (const Image *t = thumbs_.Get(it, 104, 65))
			c.Blit(*t, x + 16, y + 6);
		else
			DrawTextCentered(c, Font::Display, 22, x + 68, y + 24, kDim, "IWAD");
		DrawText(c, Font::Display, 30, x + 136, y + 4, kText, Up(it.info.title), w - 340);
		DrawText(c, Font::Body, 16, x + 136, y + 44, kMuted, it.ref + "  •  " + Size(it.info.size), w - 340);
		if (it.info.status != FileStatus::Ok)
			DrawChip(c, x + w - 170, y + 22, "INVÁLIDO", Rgba(130, 20, 16), kText);
		else if (!it.info.isIwad)
			DrawChip(c, x + w - 170, y + 22, "NÃO É IWAD", Rgba(120, 80, 10), kText);
		else
			DrawChip(c, x + w - 170, y + 22, "DISPONÍVEL", Rgba(24, 96, 52), kText);
	}
	DrawScrollbar(c, x + w + 4, y0, visible * rh, (int)items.size(), visible, st.scroll);

	// details of the selection
	const int dx = 786, dw = 446;
	DrawPanel(c, dx, y0 - 12, dw, visible * rh + 24);
	if (!items.empty())
	{
		const LibraryItem &it = items[st.sel];
		c.Fill(dx + 22, y0 + 6, 400, 250, Rgba(0, 0, 0, 180));
		if (const Image *t = thumbs_.Get(it, 400, 250))
			c.Blit(*t, dx + 22, y0 + 6);
		else
			DrawTextCentered(c, Font::Body, 18, dx + 222, y0 + 118, kDim, it.info.hasTitlePic ? "gerando miniatura…" : "sem TITLEPIC");
		int y = y0 + 270;
		auto kv = [&](const std::string &k, const std::string &v, uint32_t col = kText) {
			DrawText(c, Font::Body, 15, dx + 24, y + 2, kDim, Up(k));
			DrawText(c, Font::Body, 18, dx + 140, y, col, v, dw - 160);
			y += 28;
		};
		std::string type = it.info.archive == ArchiveType::Wad ? "WAD" : it.info.archive == ArchiveType::Zip ? "PK3/IPK3 (zip)" : it.info.archive == ArchiveType::SevenZip ? "PK7 (7z)" : "—";
		kv("Jogo", it.info.iwadDef >= 0 ? defs_[it.info.iwadDef].name : (it.info.isIwad ? it.info.title : "—"));
		kv("Tipo", type + (it.info.mapCount ? "  •  " + std::to_string(it.info.mapCount) + " mapas" : ""));
		kv("Estado", it.info.status != FileStatus::Ok ? "Inválido: " + it.info.error : it.info.isIwad ? "Válido" : "PWAD/mod, não é IWAD",
		   it.info.status != FileStatus::Ok ? kRed : it.info.isIwad ? kGreen : kAmber);
		auto users = ProfilesUsing(it.ref, true);
		std::string u;
		for (auto &s : users)
			u += (u.empty() ? "" : ", ") + s;
		kv("Perfis", u.empty() ? "nenhum" : u, kMuted);
		// compatibility with the current profile's mods
		if (const Profile *p = store_.Find(pickIwad_ ? editId_ : currentId_))
		{
			ResolvedProfile r = ResolveProfile(*p, lib_, defs_);
			for (auto *m : r.mods)
				if (!m->info.gameinfoIwad.empty() && ToLower(PathFileName(m->info.gameinfoIwad)) != ToLower(PathFileName(it.ref)))
				{
					kv("Aviso", PathFileName(m->ref) + " espera " + m->info.gameinfoIwad, kAmber);
					break;
				}
		}
	}
	DrawHints(c, { { "A", pickIwad_ ? "Escolher" : "Usar no perfil ativo" }, { "Y", "Reescanear" }, { "B", "Voltar" } });
}

// ---------------------------------------------------------------------------
// Profile mod list (load order)
// ---------------------------------------------------------------------------

void LauncherApp::UpdateMods(const InputFrame &in)
{
	State &st = states_[Screen::Mods];
	Profile *p = store_.FindMutable(editId_);
	if (!p)
	{
		Pop();
		return;
	}
	auto &mods = p->mods;
	const int n = (int)mods.size();
	bool changed = false;
	if (moving_)
	{
		if ((in.repeat & BtnUp) && st.sel > 0)
		{
			std::swap(mods[st.sel], mods[st.sel - 1]);
			st.sel--;
			changed = true;
		}
		if ((in.repeat & BtnDown) && st.sel < n - 1)
		{
			std::swap(mods[st.sel], mods[st.sel + 1]);
			st.sel++;
			changed = true;
		}
		if (in.down & (BtnA | BtnB | BtnY))
			moving_ = false;
	}
	else
	{
		MoveList(st.sel, n, in);
		if (in.down & BtnB)
		{
			Pop();
			return;
		}
		if (in.down & (BtnPlus | BtnR))
		{
			Push(Screen::ModPicker);
			return;
		}
		if (n > 0 && (in.down & BtnA))
		{
			mods[st.sel].enabled = !mods[st.sel].enabled;
			changed = true;
		}
		if (n > 0 && (in.down & BtnY))
			moving_ = true;
		if (n > 0 && (in.down & BtnX))
		{
			const std::string f = mods[st.sel].file;
			const std::string id = p->id;
			Ask("Remover do perfil", "Remover " + f + " da lista deste perfil? O arquivo continua no cartão SD.", { "Cancelar", "Remover" },
			    [this, id, f](int b) {
				Profile *q = store_.FindMutable(id);
				if (b != 1 || !q)
					return;
				q->mods.erase(std::remove_if(q->mods.begin(), q->mods.end(), [&](const ModRef &m) { return m.file == f; }), q->mods.end());
				std::string err;
				if (!store_.Save(*q, err))
					ToastMsg(err, true);
				states_[Screen::Mods].sel = std::max(0, std::min(states_[Screen::Mods].sel, (int)q->mods.size() - 1));
			});
		}
	}
	if (changed)
	{
		std::string err;
		if (!store_.Save(*p, err))
			ToastMsg(err, true);
	}
}

void LauncherApp::DrawMods(Canvas &c)
{
	State &st = states_[Screen::Mods];
	const Profile *p = store_.Find(editId_);
	if (!p)
		return;
	DrawHeader(c, "Mods do perfil", p->name + "  •  ordem de carga: 1 carrega primeiro, o último sobrescreve os anteriores");
	const int x = 48, y0 = 136, w = 1184, rh = 58, visible = 8;
	KeepVisible(st, (int)p->mods.size(), visible);
	DrawPanel(c, x - 12, y0 - 12, w + 24, visible * rh + 24);
	if (p->mods.empty())
		DrawTextWrapped(c, Font::Body, 21, x + 16, y0 + 20, w - 40, kText,
		                "Nenhum mod neste perfil. Pressione + para adicionar da biblioteca (switch/rdoom/mods/).");
	// load order = position among the enabled mods
	std::vector<int> order(p->mods.size(), 0);
	for (size_t k = 0, n = 0; k < p->mods.size(); k++)
		if (p->mods[k].enabled)
			order[k] = (int)++n;
	for (int i = 0; i < visible && st.scroll + i < (int)p->mods.size(); i++)
	{
		const int idx = st.scroll + i;
		const ModRef &m = p->mods[idx];
		const LibraryItem *it = lib_.FindMod(m.file);
		const int y = y0 + i * rh;
		SelRow(c, x, y, w, rh - 6, idx == st.sel, moving_ && idx == st.sel);
		OrderBadge(c, x + 10, y + 4, 44, order[idx]);
		// checkbox
		c.Frame(x + 68, y + 14, 24, 24, m.enabled ? kAccent : kDim, 2);
		if (m.enabled)
			c.Fill(x + 74, y + 20, 12, 12, kAccent);
		DrawText(c, Font::Bold, 21, x + 106, y + 4, m.enabled ? kText : kDim, PathFileName(m.file), 520);
		std::string sub = it && it->info.title != PathStem(m.file) ? it->info.title : m.file;
		if (m.optional)
			sub += "  (opcional)";
		if (!m.enabled)
			sub += "  •  desativado, não é carregado";
		DrawText(c, Font::Body, 15, x + 106, y + 32, kMuted, sub, 520);
		if (!it)
			DrawChip(c, x + 660, y + 14, "AUSENTE", Rgba(130, 20, 16), kText);
		else if (it->info.status != FileStatus::Ok)
			DrawChip(c, x + 660, y + 14, "INVÁLIDO", Rgba(130, 20, 16), kText);
		else
		{
			DrawChip(c, x + 660, y + 14, Up(CategoryPt(it->info.category)), Rgba(60, 40, 34), kText);
			std::string info = Size(it->info.size);
			if (it->info.mapCount)
				info += "  •  " + std::to_string(it->info.mapCount) + " mapas";
			if (it->info.hasZScript)
				info += "  •  ZScript";
			DrawTextRight(c, Font::Body, 16, x + w - 20, y + 18, kMuted, info);
		}
	}
	DrawScrollbar(c, x + w + 4, y0, visible * rh, (int)p->mods.size(), visible, st.scroll);
	if (moving_)
		DrawHints(c, { { "D-Pad", "Mover" }, { "A", "Concluir" } });
	else
		DrawHints(c, { { "A", "Ativar/Desativar" }, { "Y", "Mover" }, { "X", "Remover" }, { "+", "Adicionar" }, { "B", "Voltar" } });
}

// ---------------------------------------------------------------------------
// Mod library (browse / add to profile)
// ---------------------------------------------------------------------------

void LauncherApp::UpdateModPicker(const InputFrame &in)
{
	State &st = states_[Screen::ModPicker];
	const auto &items = lib_.Mods();
	MoveList(st.sel, (int)items.size(), in);
	if (in.down & BtnB)
	{
		Pop();
		return;
	}
	if (in.down & BtnY)
	{
		Rescan();
		ToastMsg("Biblioteca atualizada: " + std::to_string(lib_.Mods().size()) + " mod(s)");
		return;
	}
	if (items.empty() || !(in.down & BtnA))
		return;
	const LibraryItem &it = items[st.sel];
	if (it.info.status != FileStatus::Ok)
	{
		Message("Arquivo inválido", it.info.fileName + ": " + it.info.error);
		return;
	}
	if (it.info.isIwad)
	{
		Message("É uma IWAD", it.info.fileName + " contém IWADINFO e deve ser usado como IWAD. Mova-o para iwads/.");
		return;
	}
	const std::string target = editId_.empty() ? currentId_ : editId_;
	Profile *p = store_.FindMutable(target);
	if (!p)
		return;
	for (auto &m : p->mods)
		if (ToLower(m.file) == ToLower(it.ref))
		{
			ToastMsg("Já está no perfil \"" + p->name + "\"");
			return;
		}
	if (p->mods.empty() && !p->modPatterns.empty())
		p->mods = ResolveProfile(*p, lib_, defs_).filled.mods;
	ModRef ref;
	ref.file = it.ref;
	p->mods.push_back(ref);
	std::string err;
	if (store_.Save(*p, err))
		ToastMsg("Adicionado a \"" + p->name + "\" (posição " + std::to_string(p->mods.size()) + ")");
	else
		ToastMsg(err, true);
}

void LauncherApp::DrawModPicker(Canvas &c)
{
	State &st = states_[Screen::ModPicker];
	const auto &items = lib_.Mods();
	const std::string target = editId_.empty() ? currentId_ : editId_;
	const Profile *p = store_.Find(target);
	DrawHeader(c, "Biblioteca de mods", std::to_string(items.size()) + " arquivo(s)  •  A adiciona ao perfil \"" + (p ? p->name : "") + "\"");
	const int x = 48, y0 = 136, w = 700, rh = 62, visible = 8;
	KeepVisible(st, (int)items.size(), visible);
	DrawPanel(c, x - 12, y0 - 12, w + 24, visible * rh + 24);
	if (items.empty())
		DrawTextWrapped(c, Font::Body, 21, x + 16, y0 + 20, w - 40, kText,
		                "Nenhum mod encontrado. Copie arquivos .pk3, .wad, .zip, .pk7 ou .deh para switch/rdoom/mods/ e pressione Y.");
	for (int i = 0; i < visible && st.scroll + i < (int)items.size(); i++)
	{
		const int idx = st.scroll + i;
		const LibraryItem &it = items[idx];
		const int y = y0 + i * rh;
		SelRow(c, x, y, w, rh - 6, idx == st.sel);
		int inProfile = 0;  // 1-based position in the profile's list
		if (p)
			for (size_t k = 0; k < p->mods.size(); k++)
				if (ToLower(p->mods[k].file) == ToLower(it.ref))
					inProfile = (int)k + 1;
		// the file name is what people copied to the SD card; the title comes from inside the file
		DrawText(c, Font::Bold, 21, x + 20, y + 4, kText, PathFileName(it.ref), w - 260);
		std::string sub = it.info.title != PathStem(it.ref) ? it.info.title + "  •  " : std::string();
		if (it.ref != PathFileName(it.ref))
			sub += it.ref + "  •  ";
		DrawText(c, Font::Body, 15, x + 20, y + 33, kMuted, sub + Size(it.info.size), w - 260);
		if (it.info.status != FileStatus::Ok)
			DrawChip(c, x + w - 200, y + 14, "INVÁLIDO", Rgba(130, 20, 16), kText);
		else if (it.info.isIwad)
			DrawChip(c, x + w - 200, y + 14, "É UMA IWAD", Rgba(120, 80, 10), kText);
		else if (inProfile)
			DrawChip(c, x + w - 200, y + 14, "NO PERFIL #" + std::to_string(inProfile), Rgba(24, 96, 52), kText);
		else
			DrawChip(c, x + w - 200, y + 14, Up(CategoryPt(it.info.category)), Rgba(60, 40, 34), kText);
	}
	DrawScrollbar(c, x + w + 4, y0, visible * rh, (int)items.size(), visible, st.scroll);

	const int dx = 786, dw = 446;
	DrawPanel(c, dx, y0 - 12, dw, visible * rh + 24);
	if (!items.empty())
	{
		const LibraryItem &it = items[st.sel];
		int y = y0 + 6;
		y += DrawTextWrapped(c, Font::Display, 34, dx + 24, y, dw - 48, kText, Up(it.info.title), 2, -4) + 8;
		auto kv = [&](const std::string &k, const std::string &v, uint32_t col = kText) {
			DrawText(c, Font::Body, 15, dx + 24, y + 2, kDim, Up(k));
			y += DrawTextWrapped(c, Font::Body, 18, dx + 150, y, dw - 174, col, v, 2) + 4;
		};
		std::string type = it.info.archive == ArchiveType::Wad ? "WAD" : it.info.archive == ArchiveType::Zip ? "PK3 (zip)"
			: it.info.archive == ArchiveType::SevenZip ? "PK7 (7z)" : (it.info.hasDehacked ? "DeHackEd" : "—");
		kv("Arquivo", it.ref, kMuted);
		kv("Formato", type + "  •  " + Size(it.info.size));
		kv("Categoria", CategoryPt(it.info.category));
		std::string content;
		if (it.info.mapCount) content += std::to_string(it.info.mapCount) + " mapas ";
		if (it.info.hasZScript) content += "ZScript ";
		if (it.info.hasDecorate) content += "DECORATE ";
		if (it.info.hasDehacked) content += "DEHACKED ";
		if (it.info.hasMapinfo) content += "MAPINFO";
		kv("Conteúdo", content.empty() ? (it.info.archive == ArchiveType::SevenZip ? "7z: lido pelo motor" : "recursos") : content);
		if (!it.info.gameinfoIwad.empty())
			kv("IWAD pedida", it.info.gameinfoIwad, kAmber);
		if (!it.info.gameinfoLoad.empty())
		{
			std::string deps;
			for (auto &d : it.info.gameinfoLoad)
				deps += (deps.empty() ? "" : ", ") + d + (lib_.FindMod(d) ? "" : " (ausente)");
			kv("Dependências", deps, kAmber);
		}
		else
			kv("Dependências", "nenhuma declarada (GAMEINFO)", kMuted);
		if (it.info.status != FileStatus::Ok)
			kv("Erro", it.info.error, kRed);
		auto users = ProfilesUsing(it.ref, false);
		std::string u;
		for (auto &s : users)
			u += (u.empty() ? "" : ", ") + s;
		kv("Perfis", u.empty() ? "nenhum" : u, kMuted);
	}
	DrawHints(c, { { "A", "Adicionar ao perfil" }, { "Y", "Reescanear" }, { "B", "Voltar" } });
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void LauncherApp::UpdateSettings(const InputFrame &in)
{
	State &st = states_[Screen::Settings];
	const int n = 7;
	MoveList(st.sel, n, in);
	if (in.down & BtnB)
	{
		Pop();
		return;
	}
	if (!(in.down & BtnA) && !(in.repeat & (BtnLeft | BtnRight)))
		return;
	std::string err;
	auto editDirs = [this](std::vector<std::string> &dirs, const std::string &what) {
		std::vector<std::string> items = { "Adicionar pasta…" };
		for (auto &d : dirs)
			items.push_back("Remover: " + d);
		Choose("Pastas extras de " + what, items, [this, &dirs, what](int i) {
			if (i < 0)
				return;
			if (i == 0)
			{
				std::string path = "sdmc:/switch/gzdoom/" + std::string(what == "IWADs" ? "iwads" : "pwads");
				if (!Keyboard("Caminho da pasta", path, 200, path))
					return;
				if (!DirExists(path))
				{
					Message("Pasta não encontrada", path + " não existe no cartão SD.");
					return;
				}
				dirs.push_back(path);
			}
			else
				dirs.erase(dirs.begin() + (i - 1));
			std::string e;
			if (!SaveLauncherSettings(paths_, settings_, e))
				ToastMsg(e, true);
			Rescan();
			ToastMsg("Biblioteca atualizada");
		});
	};
	switch (st.sel)
	{
	case 0: editDirs(settings_.extraIwadDirs, "IWADs"); break;
	case 1: editDirs(settings_.extraModDirs, "mods"); break;
	case 2:
		settings_.animations = !settings_.animations;
		if (!SaveLauncherSettings(paths_, settings_, err))
			ToastMsg(err, true);
		break;
	case 3:
		Rescan();
		ToastMsg("Biblioteca atualizada: " + std::to_string(lib_.Iwads().size()) + " IWAD(s), " + std::to_string(lib_.Mods().size()) + " mod(s)");
		break;
	case 4:
		Message("Clocks de fábrica",
		        "O RDoom funciona apenas com o gerenciamento normal de energia do Nintendo Switch. Ele não tem opções de overclock, "
		        "não altera frequências de CPU, GPU ou memória e não depende de sys-clk ou módulos semelhantes. "
		        "O desempenho vem de ajustes de resolução, limite de FPS e qualidade.");
		break;
	case 5:
		Message("Sobre o RDoom",
		        info_.version + "\n" + info_.engineVersion + "\n" + info_.gfxVersion + "\n\n"
		        "Port não oficial do UZDoom para Nintendo Switch (Atmosphère). Não é um produto da id Software nem dos mantenedores do UZDoom. "
		        "Licença GPLv3; avisos de terceiros em switch/rdoom/licenses/.");
		break;
	case 6:
		Ask("Restaurar launcher", "Voltar as configurações globais do launcher ao padrão? Perfis não são afetados.", { "Cancelar", "Restaurar" },
		    [this](int b) {
			if (b != 1)
				return;
			std::string last = settings_.lastProfile, e;
			settings_ = LauncherSettings();
			settings_.lastProfile = last;
			if (!SaveLauncherSettings(paths_, settings_, e))
				ToastMsg(e, true);
			Rescan();
		});
		break;
	}
}

void LauncherApp::DrawSettings(Canvas &c)
{
	State &st = states_[Screen::Settings];
	DrawHeader(c, "Configurações", "Opções globais do launcher (cada perfil guarda as suas)");
	auto list = [](const std::vector<std::string> &v) {
		std::string s;
		for (auto &d : v)
			s += (s.empty() ? "" : ", ") + d;
		return s.empty() ? std::string("nenhuma") : s;
	};
	struct Row { std::string label, value; };
	std::vector<Row> rows = {
		{ "Pastas extras de IWADs", list(settings_.extraIwadDirs) },
		{ "Pastas extras de mods", list(settings_.extraModDirs) },
		{ "Animações da interface", settings_.animations ? "Ligadas" : "Desligadas" },
		{ "Reescanear bibliotecas", "›" },
		{ "Política de clocks", "Somente clocks de fábrica" },
		{ "Sobre o RDoom e licenças", "›" },
		{ "Restaurar configurações do launcher", "›" },
	};
	const int x = 48, y0 = 136, w = 1184, rh = 60;
	DrawPanel(c, x - 12, y0 - 12, w + 24, (int)rows.size() * rh + 24);
	for (int i = 0; i < (int)rows.size(); i++)
	{
		const int y = y0 + i * rh;
		SelRow(c, x, y, w, rh - 6, i == st.sel);
		DrawText(c, Font::Body, 22, x + 22, y + 12, i == st.sel ? Rgba(255, 255, 255) : kMuted, rows[i].label);
		DrawTextRight(c, Font::Bold, 20, x + w - 22, y + 14, i == 4 ? kGreen : kText, Ellipsize(Font::Bold, 20, rows[i].value, 640));
	}
	DrawText(c, Font::Body, 17, 52, y0 + (int)rows.size() * rh + 30, kDim,
	         "Pastas extras permitem usar bibliotecas existentes, como sdmc:/switch/gzdoom/iwads, sem copiar arquivos grandes.");
	DrawHints(c, { { "A", "Abrir/Alterar" }, { "B", "Voltar" } });
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

void LauncherApp::UpdateDiagnostics(const InputFrame &in)
{
	State &st = states_[Screen::Diagnostics];
	if (in.repeat & BtnUp) st.scroll = std::max(0, st.scroll - 1);
	if (in.repeat & BtnDown) st.scroll++;
	if (in.down & BtnB)
		Pop();
	if (in.down & BtnY)
	{
		ReadFile(info_.lastSessionFile, lastSession_);
		ToastMsg("Relatório da última sessão recarregado");
	}
	if (in.down & BtnX)
	{
		Ask("Limpar caches de shaders",
		    "Apaga o cache de pipelines Vulkan do RDoom e o cache de shaders do Mesa (sdmc:/.mesa). "
		    "Use se o jogo passou a travar ou fechar com \"device lost\". A primeira partida depois disso "
		    "compila os shaders de novo (pequenas pausas). IWADs, mods, perfis e saves não são tocados.",
		    { "Cancelar", "Limpar" }, [this](int b) {
			if (b != 1)
				return;
			int n = RemoveFile(PathJoin(paths_.Cache(), "uzdoom/vulkanpipelinecache")) ? 1 : 0;
			// Mesa keeps its cache at the root of the SD card (sdmc:/.mesa)
			const std::string sd = paths_.root.compare(0, 6, "sdmc:/") == 0 ? std::string("sdmc:/") : PathJoin(paths_.root, "../..") + "/";
			const int m = RemoveTree(sd + ".mesa/mesa_shader_cache_sf");
			if (m > 0)
				n += m;
			RLOG_I("diag", "shader caches cleared by the user: %d files", n);
			ToastMsg("Caches de shaders limpos (" + std::to_string(n) + " arquivo(s))");
		});
	}
}

void LauncherApp::DrawDiagnostics(Canvas &c)
{
	State &st = states_[Screen::Diagnostics];
	DrawHeader(c, "Diagnóstico", "Versões, memória, renderizador da última sessão e problemas encontrados");
	SystemInfo si = plat_.Info();
	std::vector<std::pair<std::string, uint32_t>> lines;
	auto add = [&](const std::string &s, uint32_t col = kText) { lines.push_back({ s, col }); };
	add("[VERSÕES]", kAccent);
	add(info_.version + "  •  compilado em " + info_.buildDate);
	add(info_.engineVersion);
	add(info_.gfxVersion);
	add("[SISTEMA]", kAccent);
	add(std::string("Modo de execução: ") + (si.appletMode ? "applet (memória limitada!)" : "aplicativo (título, memória completa)"), si.appletMode ? kAmber : kGreen);
	if (si.memTotal)
		add("Memória do processo: " + Size(si.memUsed) + " usados de " + Size(si.memTotal));
	add("Espaço livre no SD: " + (si.sdFree == UINT64_MAX ? std::string("desconhecido") : Size(si.sdFree)));
	add(std::string("Console: ") + (si.docked ? "na base (TV)" : "portátil") + (si.firmware.empty() ? "" : "  •  firmware " + si.firmware));
	if (!si.emulator.empty())
		add("Ambiente: " + si.emulator, kAmber);
	add("Clocks: somente os de fábrica (o RDoom não altera CPU/GPU/memória)", kGreen);
	add("[ÚLTIMA SESSÃO DO MOTOR]", kAccent);
	if (lastSession_.empty())
		add("Nenhuma sessão registrada ainda (" + info_.lastSessionFile + ")", kMuted);
	else
	{
		size_t s = 0;
		while (s < lastSession_.size())
		{
			size_t e = lastSession_.find('\n', s);
			std::string l = lastSession_.substr(s, e == std::string::npos ? std::string::npos : e - s);
			if (!l.empty())
				add(l, l.find("FAIL") != std::string::npos || l.find("error") != std::string::npos ? kRed : kText);
			if (e == std::string::npos)
				break;
			s = e + 1;
		}
	}
	{
		std::string lost;
		if (ReadFile(PathJoin(paths_.Logs(), "gpu-lost-previous.txt"), lost) && !lost.empty())
		{
			add("[ÚLTIMA FALHA DA GPU (device lost)]", kAccent);
			add("Os caches de shaders foram limpos automaticamente depois dela.", kAmber);
			size_t s0 = 0;
			for (int k = 0; k < 14 && s0 < lost.size(); k++)
			{
				size_t e = lost.find('\n', s0);
				std::string l = lost.substr(s0, e == std::string::npos ? std::string::npos : e - s0);
				if (!l.empty())
					add(l, k == 0 ? kRed : kMuted);
				if (e == std::string::npos)
					break;
				s0 = e + 1;
			}
		}
	}
	add("[BIBLIOTECA]", kAccent);
	add(std::to_string(lib_.Iwads().size()) + " IWAD(s), " + std::to_string(lib_.Mods().size()) + " mod(s), " + std::to_string(store_.All().size()) + " perfil(is), " + std::to_string(defs_.size()) + " definições de IWAD");
	add("[PROBLEMAS]", kAccent);
	if (loadWarnings_.empty())
		add("Nenhum problema ao carregar perfis e configurações.", kGreen);
	for (auto &w : loadWarnings_)
		add(w, kAmber);
	add("[ARQUIVOS DE LOG]", kAccent);
	add(PathJoin(paths_.Logs(), "rdoom.log") + "  (launcher)", kMuted);
	add(PathJoin(paths_.Logs(), "engine.log") + "  (motor)", kMuted);
	add(PathJoin(paths_.Logs(), "last-launch.txt") + "  (último comando de início)", kMuted);

	const int x = 48, y0 = 136, rh = 30, visible = 17;
	st.scroll = std::max(0, std::min(st.scroll, std::max(0, (int)lines.size() - visible)));
	DrawPanel(c, x - 12, y0 - 12, 1208, visible * rh + 24);
	for (int i = 0; i < visible && st.scroll + i < (int)lines.size(); i++)
	{
		const auto &l = lines[st.scroll + i];
		const bool head = !l.first.empty() && l.first[0] == '[';
		DrawText(c, head ? Font::Display : Font::Body, head ? 26 : 19, x + 16, y0 + i * rh + (head ? -1 : 3), l.second, l.first, 1150);
	}
	DrawScrollbar(c, x + 1188, y0, visible * rh, (int)lines.size(), visible, st.scroll);
	DrawHints(c, { { "D-Pad", "Rolar" }, { "X", "Limpar caches de shaders" }, { "Y", "Recarregar" }, { "B", "Voltar" } });
}

} // namespace rdoom
