/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "vangogh/page.h"
#include "vangogh/world.h"
#include "vangogh/bfg.h"
#include "vangogh/spr.h"

#include "common/debug.h"
#include "common/file.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"
#include "graphics/cursorman.h"
#include "graphics/palette.h"
#include "graphics/surface.h"

#include <math.h>

namespace Vangogh {

namespace {

// ---------------------------------------------------------------------------
// Constants of the exe (world.md sec.8, sec.9).

const int kGlideFrames = 20;			// dat_4f98bc: mode-4 painting glide divisor
const int kStrafeMax = 50, kTurnMax = 20, kFwdMax = 75;
const int kPitchLimit = 500;

// ---------------------------------------------------------------------------
// Scene tables.

const char *const kSceneNames[kNumScenes] = {
	"musee", "auberge", "hopiext", "maisonet", "mangeurs", "cafe", "chambrev",
	"maisonj", "hopiint", "pont", "terrasse", "jardin", "champ", "eglise"
};

const int kMaxBoxVariants = 4;

// SetBox(n) volumes per scene (BFG records beside BOX.3DI); n = index + 1.
const char *const kBoxVariants[kNumScenes][kMaxBoxVariants] = {
	{ "BOX1.3DI", "BOX2.3DI", "BOX3.3DI", "BOX4.3DI" },	// musee
	{ "BOXBAS.3DI", "BOXHAUT.3DI", nullptr, nullptr },	// auberge
	{ "BOX1.3DI", nullptr, nullptr, nullptr },			// hopiext
	{ nullptr, nullptr, nullptr, nullptr },
	{ nullptr, nullptr, nullptr, nullptr },
	{ nullptr, nullptr, nullptr, nullptr },
	{ nullptr, nullptr, nullptr, nullptr },
	{ "BOX1.3DI", "BOX2.3DI", "BOX3.3DI", nullptr },	// maisonj
	{ nullptr, nullptr, nullptr, nullptr },
	{ "BOX2.3DI", nullptr, nullptr, nullptr },			// pont
	{ nullptr, nullptr, nullptr, nullptr },
	{ "BOX1.3DI", nullptr, nullptr, nullptr },			// jardin
	{ nullptr, nullptr, nullptr, nullptr },
	{ nullptr, nullptr, nullptr, nullptr },
};

struct SpecEntry {
	const char *name;
	byte type;
};

// Pickable object tables, initial `type` per named 3DC node (world.md per-scene
// sections / SetScene init). 255 = inert; 2,3,4,6 are the clickable kinds.
const SpecEntry kMusee[] = {
	{ "m01_02", 4 }, { "m01_03", 4 }, { "m03_01", 4 }, { "m03_02", 4 }, { "m03_03", 4 }, { "m03_04", 4 },
	{ "m03_05", 4 }, { "m03_06", 4 }, { "m04_01", 4 }, { "m04_02", 4 }, { "m04_03", 4 },
	{ "robot", 4 }, { "etoile", 2 }, { "prtholl", 1 }, { "prtarles", 1 }, { "prtauvers", 1 },
	{ "vase", 255 }, { "ecran", 4 }, { "cou", 4 }, { "oeild", 4 }, { "oeilg", 4 }, { "tete", 4 }
};
const SpecEntry kAuberge[] = {
	{ "hors", 255 }, { "saccoche", 4 }, { "blanche", 255 }, { "stetoscop", 2 }, { "portebas", 6 }, { "portehaut", 6 },
	{ "casquette", 3 }, { "tableau", 255 }, { "porte01", 6 }, { "porte02", 6 }, { "sac", 4 }, { "sac01", 4 }
};
const SpecEntry kHopiext[] = {
	{ "cloche", 255 }, { "grille", 255 }, { "plak", 255 }, { "grilledrt", 4 }, { "grillegche", 4 }, { "arbre11", 3 },
	{ "porteent", 6 }, { "korde1", 255 }, { "korde2", 255 }, { "korde3", 255 }, { "korde4", 255 }
};
const SpecEntry kMaisonet[] = {
	{ "poule", 255 }, { "pelle", 4 }, { "oiso", 255 }, { "terre", 255 }, { "plume", 2 }, { "nid", 3 }, { "porte04", 6 }
};
const SpecEntry kMangeurs[] = {
	{ "buche", 255 }, { "chaise1", 255 }, { "fagot", 255 }, { "fenetre", 255 }, { "portepoel", 4 }, { "bersso", 255 },
	{ "patat01", 2 }, { "pendul", 255 }, { "POT", 3 }, { "vapeur", 255 }, { "theieres", 255 }, { "porte", 6 }
};
const SpecEntry kCafe[] = {
	{ "ke", 4 }, { "gant", 3 }, { "briket", 2 }, { "clef", 4 }, { "carte", 2 }, { "pomme", 2 }, { "barporte", 255 },
	{ "ombre", 255 }, { "orloge", 2 }, { "lampe4", 3 }, { "theiere", 2 }, { "pichet", 2 }, { "mirroir", 255 },
	{ "porte01", 6 }, { "porte02", 6 }
};
const SpecEntry kChambreV[] = {
	{ "porte", 255 }, { "porte02", 6 }
};
const SpecEntry kChambreB[] = {
	{ "Object09", 2 }, { "porte", 255 }, { "CHAISE1", 255 }, { "cadre", 2 }, { "Object02", 2 }, { "TIROIR", 255 },
	{ "camee", 2 }, { "shoes", 2 }, { "Object07", 2 }, { "papier", 255 }, { "mirroircas", 255 }, { "ombrelle", 2 },
	{ "oreiller", 3 }, { "tab01", 3 }, { "tab02", 3 }, { "bougie", 255 }, { "Object01", 255 }, { "mirroir1", 255 },
	{ "rasoir", 255 }, { "mirroirmor", 2 }, { "porte02", 6 }, { "papierferm", 255 }, { "Object10", 255 },
	{ "shoes01", 2 }, { "tabpay", 3 }, { "Object06", 2 }
};
const SpecEntry kMaisonj[] = {
	{ "pot01", 255 }, { "pot02", 255 }, { "nuages", 255 }, { "escargot", 2 }, { "pendule", 3 }, { "portail", 255 },
	{ "portemj", 255 }, { "portail01", 255 }
};
const SpecEntry kHopiint[] = {
	{ "ARMOIRE", 255 }, { "LAMPE", 2 }, { "BONNET", 2 }, { "CROIX", 2 }, { "BOUGIE", 2 }, { "MIRROIRVG", 3 },
	{ "FOU", 3 }, { "PLAK", 6 }, { "ECHIK", 3 }
};
const SpecEntry kPont[] = {
	{ "apple01", 2 }, { "chemise", 3 }, { "trappe", 255 }, { "pouli03", 255 }, { "encre01", 2 }, { "pouli02", 255 },
	{ "poignee04", 4 }
};
const SpecEntry kTerrasse[] = {
	{ "lettre", 2 }, { "de", 255 }, { "MANIVELLE", 255 }, { "lunette", 2 }, { "KASKET", 3 }, { "drapo", 3 },
	{ "porte01", 6 }, { "porte02", 6 }, { "platode", 2 }
};
const SpecEntry kJardin[] = {
	{ "rato", 2 }, { "fuzz", 2 }, { "auberge", 255 }, { "cerf", 4 }, { "partoche", 3 }, { "barriere", 255 },
	{ "porte01", 6 }, { "porte02", 6 }
};
const SpecEntry kChamp[] = {
	{ "faux", 255 }, { "coclicot", 3 }, { "gun", 2 }, { "sabots", 3 }
};
const SpecEntry kEglise[] = {
	{ "cerf", 2 }, { "gerbe", 2 }, { "eglise10", 3 }
};

const SpecEntry *specsForScene(int scene, int variant, uint &count) {
	switch (scene) {
	case kSceneMusee: count = ARRAYSIZE(kMusee); return kMusee;
	case kSceneAuberge: count = ARRAYSIZE(kAuberge); return kAuberge;
	case kSceneHopiext: count = ARRAYSIZE(kHopiext); return kHopiext;
	case kSceneMaisonet: count = ARRAYSIZE(kMaisonet); return kMaisonet;
	case kSceneMangeurs: count = ARRAYSIZE(kMangeurs); return kMangeurs;
	case kSceneCafe: count = ARRAYSIZE(kCafe); return kCafe;
	case kSceneChambre:
		if (variant == 0) { count = ARRAYSIZE(kChambreV); return kChambreV; }
		count = ARRAYSIZE(kChambreB);
		return kChambreB;
	case kSceneMaisonj: count = ARRAYSIZE(kMaisonj); return kMaisonj;
	case kSceneHopiint: count = ARRAYSIZE(kHopiint); return kHopiint;
	case kScenePont: count = ARRAYSIZE(kPont); return kPont;
	case kSceneTerrasse: count = ARRAYSIZE(kTerrasse); return kTerrasse;
	case kSceneJardin: count = ARRAYSIZE(kJardin); return kJardin;
	case kSceneChamp: count = ARRAYSIZE(kChamp); return kChamp;
	case kSceneEglise: count = ARRAYSIZE(kEglise); return kEglise;
	default: count = 0; return nullptr;
	}
}

// ---------------------------------------------------------------------------
// Poses (x, y, z, rotA, rotB). world.md sec.9.2.

struct PoseRow {
	int16 from, to;	// scene ids
	int16 x, y, z, a, b;
};

// SetScene transition poses (prev -> cur, prev != cur, both non-museum).
const PoseRow kTransitions[] = {
	{ 1, 11, -6620, 182, -578, 4066, 1128 },
	{ 2, 7, 3320, 358, 12464, 0, 2264 },
	{ 2, 8, -1318, -11, 8435, 4036, 1476 },
	{ 2, 9, 4933, -22, 13060, 0, 2581 },
	{ 3, 4, -384, 35, 1373, 3946, 1701 },
	{ 4, 3, 7310, 183, 5921, 0, 2873 },
	{ 5, 10, 196, -46, 3078, 4066, 1519 },
	{ 6, 7, -6199, 358, 737, 0, 1657 },
	{ 7, 2, 17495, 6733, 14500, 4036, 2018 },
	{ 7, 6, 100, -336, 439, 3946, 3446 },
	{ 7, 9, 11579, -56, -2069, 30, 3253 },
	{ 7, 10, 677, -63, -1916, 4066, 68 },
	{ 8, 2, 9533, 6735, 11297, 4066, 1727 },
	{ 9, 2, 17028, 6815, -389, 4036, 4086 },
	{ 9, 7, -831, 489, -9000, 4066, 72 },
	{ 10, 5, 1617, -90, -116, 3976, 3788 },
	{ 10, 7, -7605, 358, -1698, 0, 1054 },
	{ 11, 1, 0, 0, 0, 0, 0 },
	{ 11, 12, 2425, 160, -4360, 4006, 3989 },
	{ 11, 13, -19321, 841, 23485, 0, 1651 },
	{ 12, 11, -8760, 228, -928, 3946, 973 },
	{ 12, 13, -2500, -634, 384, 60, 3279 },
	{ 13, 11, 6743, 235, -1283, 4006, 3036 },
	{ 13, 12, -13857, -522, 9973, 4006, 1241 },
};

// Museum pose when arriving from scene `index` (cur = 0, prev = index); index 0 is the boot pose.
struct MuseumPose {
	int16 x, y, z, a, b;
};
const MuseumPose kMuseumPoses[kNumScenes] = {
	{ -39, -209, 361, 4066, 20 },		// 0 boot
	{ 3230, -297, 5166, 4036, 3073 },	// 1 auberge
	{ 538, -297, 8256, 4036, 290 },		// 2 hopiext
	{ -3125, -297, 5252, 4066, 1008 },	// 3 maisonet
	{ -4849, -297, 5195, 4036, 3101 },	// 4 mangeurs
	{ -562, -296, 6923, 4036, 2355 },	// 5 cafe
	{ -909, -297, 7462, 4066, 3062 },	// 6 chambre
	{ -466, -297, 8184, 4066, 3788 },	// 7 maisonj
	{ 538, -297, 8256, 4036, 290 },		// 8 hopiint
	{ 703, -297, 6902, 4036, 1782 },	// 9 pont
	{ 933, -297, 7535, 0, 988 },		// 10 terrasse
	{ 3230, -297, 5166, 4036, 3073 },	// 11 jardin
	{ 4403, -297, 5867, 4006, 339 },	// 12 champ
	{ 4524, -297, 4758, 4006, 1791 },	// 13 eglise
};

// Hopiext has no default pose (it is only ever entered from maisonj/hopiint/pont); debug 'goto' uses 9->2.
const int kHopiextFallbackRow = 13;

// ---------------------------------------------------------------------------
// Exits (world.md sec.8.3).

struct ClickExit {
	int scene;
	const char *object;
	int dest;
	int maxDist;	// 0 = no distance check
};

const ClickExit kClickExits[] = {
	{ kSceneMusee, "m01_02", kSceneMaisonet, 1200 }, { kSceneMusee, "m01_03", kSceneMangeurs, 1200 },
	{ kSceneMusee, "m03_01", kSceneCafe, 1200 }, { kSceneMusee, "m03_02", kSceneChambre, 1200 },
	{ kSceneMusee, "m03_03", kSceneMaisonj, 1200 }, { kSceneMusee, "m03_04", kSceneHopiint, 1200 },
	{ kSceneMusee, "m03_05", kSceneTerrasse, 1200 }, { kSceneMusee, "m03_06", kScenePont, 1200 },
	{ kSceneMusee, "m04_01", kSceneJardin, 1200 }, { kSceneMusee, "m04_02", kSceneChamp, 1200 },
	{ kSceneMusee, "m04_03", kSceneEglise, 1200 },
	{ kSceneAuberge, "porte01", kSceneJardin, 0 }, { kSceneAuberge, "porte02", kSceneJardin, 0 },
	{ kSceneHopiext, "porteent", kSceneHopiint, 0 },
	{ kSceneHopiint, "PLAK", kSceneHopiext, 3000 },
	{ kSceneMaisonet, "porte04", kSceneMangeurs, 5000 },
	{ kSceneMangeurs, "porte", kSceneMaisonet, 0 },
	{ kSceneCafe, "porte01", kSceneTerrasse, 0 }, { kSceneCafe, "porte02", kSceneTerrasse, 0 },
	{ kSceneChambre, "porte02", kSceneMaisonj, 0 },
	{ kSceneTerrasse, "porte01", kSceneCafe, 0 }, { kSceneTerrasse, "porte02", kSceneCafe, 0 },
	{ kSceneJardin, "porte01", kSceneAuberge, 0 }, { kSceneJardin, "porte02", kSceneAuberge, 0 },
};

/** Camera-threshold exits; returns the destination scene or -1. */
int cameraExit(int scene, int x, int z) {
	switch (scene) {
	case kSceneHopiext:
		if (z < -1000) return kScenePont;
		break;
	case kSceneMaisonj:
		if (z < -12500) return kScenePont;
		if (x < -8500 && z > -10000) return kSceneTerrasse;
		if (x > -6233 && x < -6000 && z > 2000) return kSceneChambre;
		break;
	case kScenePont:
		if (x > 12000) return kSceneMaisonj;
		if (x > 5500 && z > 11000) return kSceneHopiext;
		break;
	case kSceneTerrasse:
		if (z <= -2080) return kSceneMaisonj;
		break;
	case kSceneJardin:
		if (z < 300 && x > 10000) return kSceneEglise;
		if (z < -500 && x < -9300) return kSceneChamp;
		break;
	case kSceneChamp:
		if (x < -16000) return kSceneEglise;
		if (z < -5500) return kSceneJardin;
		break;
	case kSceneEglise:
		if (x > -2000) return kSceneChamp;
		if (z > 24500) return kSceneJardin;
		break;
	default:
		break;
	}
	return -1;
}

// ---------------------------------------------------------------------------
// Cursors: table 0x4fa288 {frame, hotX, hotY}; sprites are centre-anchored.

struct CursorDef {
	int8 frame, hotX, hotY;
};
const CursorDef kCursorDefs[14] = {
	{ 0, -7, -10 }, { 1, -9, 0 }, { 2, 8, 0 }, { 3, 0, -9 }, { 4, 0, 8 }, { 5, -9, -9 }, { 6, 8, -9 },
	{ 7, -9, 9 }, { 8, 8, 9 }, { 9, 0, 0 }, { 10, 2, -8 }, { 11, 0, -9 }, { 12, 0, -6 }, { 13, 0, -6 }
};

// ---------------------------------------------------------------------------
// 1.15 fixed-point trig tables (sub_4767c0), index masked with 0xfff.

int32 gSin[4096], gCos[4096];
bool gTrigReady = false;

void buildTrig() {
	if (gTrigReady)
		return;
	for (int k = 0; k < 4096; k++) {
		gSin[k] = (int32)(sin(2.0 * M_PI * k / 4096.0) * 32768.0);
		gCos[k] = (int32)(cos(2.0 * M_PI * k / 4096.0) * 32768.0);
	}
	gTrigReady = true;
}

inline int16 toI16(double v) {
	return (int16)(int32)v;
}

/** Pitch as the signed value the movement code keeps (-2048..2047). */
inline int16 signedAngle(int32 a) {
	return (int16)(((a + 2048) & 0xfff) - 2048);
}

} // end of anonymous namespace

World::World() : _box(0), _scene(-1), _prevScene(0), _groundProbe(500), _mouseLook(false),
		_numObjects(0), _hoverObject(-1), _clickCount(0), _cursorKind(kCursorArrow), _shownKind(-1), _cursorHidden(false), _cursorsReady(false),
		_glideDest(0), _glideLeft(0), _menuRequest(false), _overlay(false), _pageRequest(-1), _numAnims(0), _framesInScene(0), _timerLimit(0),
		_voice(0), _trainSound(false), _clipActive(false), _clipScene(0), _clipPrev(0) {
	buildTrig();
	memset(_timer, 0, sizeof(_timer));
	_carry = kCarryNone;
	_carryName[0] = '\0';
	_clockTimer = _clockLast = _clockState = 0;
	_clockEnable = false;
	_billiardRt = false;
	_clipKeepPose = false;
	Common::strlcpy(_mirrorTex, "MIRROIRG", sizeof(_mirrorTex));
	_mgTick = _mgCry = _mgBoil = 0;
	_mgCrying = _mgWindowRt = _mgCoucou = false;
	_robotTalk = _robotUV = _robotUVStep = 0;
	_robotUVDir = 1;
	_finRequest = false;
	_retourShown = false;
	for (int i = 0; i < kMaxAnims; i++) {
		_anims[i].file = _anims[i].node = nullptr;
		_anims[i].elapsed = 1;
		_anims[i].active = _anims[i].loaded = false;
	}
	memset(_boxLoaded, 0, sizeof(_boxLoaded));
	memset(_objects, 0, sizeof(_objects));
	memset(_cursorPalette, 0, sizeof(_cursorPalette));
	_hoverName[0] = '\0';
	resetMovement();
	memset(_prevPos, 0, sizeof(_prevPos));
	_glideFrom = Pose3D();
	_glideTo = Pose3D();
}

World::~World() {
	_comp.free();
}

bool World::init() {
	if (!_inv.init())
		warning("Vangogh: inventory bar images unavailable");
	Common::File f;
	if (!f.open(Common::Path("sprites/curseurs.spr"))) {
		warning("Vangogh: could not open sprites/curseurs.spr");
		return false;
	}
	SPRFile spr;
	if (!spr.load(f) || spr.numCells() < kNumCursors) {
		warning("Vangogh: curseurs.spr is unusable");
		return false;
	}

	const Graphics::Palette &pal = spr.getPalette();
	memset(_cursorPalette, 0, sizeof(_cursorPalette));
	memcpy(_cursorPalette, pal.data(), MIN<uint>(pal.size(), 256) * 3);

	for (int k = 0; k < kNumCursors; k++) {
		Graphics::Surface *cell = spr.decodeCell(kCursorDefs[k].frame);
		if (!cell || cell->format.bytesPerPixel != 1) {
			warning("Vangogh: curseurs.spr cell %d did not decode", kCursorDefs[k].frame);
			if (cell) {
				cell->free();
				delete cell;
			}
			return false;
		}
		Cursor &c = _cursors[k];
		c.w = cell->w;
		c.h = cell->h;
		c.hotX = kCursorDefs[k].hotX;
		c.hotY = kCursorDefs[k].hotY;
		c.pixels.resize(c.w * c.h);
		for (int y = 0; y < c.h; y++)
			memcpy(&c.pixels[y * c.w], cell->getBasePtr(0, y), c.w);
		cell->free();
		delete cell;
	}
	static const char *const kExtNames[kNumExtCursors] = {
		"main", "curza", "curdoigt", "acces", "deja-vu", "curferme", "fagot", "buche", "cle", "manivel"
	};
	for (int k = 0; k < kNumExtCursors; k++) {
		if (!_ext[k].loadTga(kExtNames[k]))
			return false;
	}
	if (!_retour.loadTga("retour"))
		return false;
	_cursorsReady = true;
	return true;
}

void World::drawCursor(Graphics::Surface &dst, int kind, int x, int y) const {
	if (kind >= kNumCursors && kind < kNumCursorKinds) {
		// graphs_2d cursors are drawn with their top-left corner at the pointer (sub_40164f at dat_4f9540/44).
		_ext[kind - kNumCursors].blit(dst, 0, x, y);
		return;
	}
	const byte *pixels, *palette;
	int w, h, hx, hy;
	if (!cursorImage(kind, pixels, w, h, hx, hy, palette))
		return;
	for (int j = 0; j < h; j++) {
		for (int i = 0; i < w; i++) {
			const byte v = pixels[j * w + i];
			const int px = x - hx + i, py = y - hy + j;
			if (!v || px < 0 || py < 0 || px >= dst.w || py >= dst.h)
				continue;
			const uint32 c = dst.format.RGBToColor(palette[v * 3], palette[v * 3 + 1], palette[v * 3 + 2]);
			if (dst.format.bytesPerPixel == 2)
				*((uint16 *)dst.getBasePtr(px, py)) = (uint16)c;
			else
				*((uint32 *)dst.getBasePtr(px, py)) = c;
		}
	}
}

bool World::cursorImage(int kind, const byte *&pixels, int &w, int &h, int &hotX, int &hotY, const byte *&palette) const {
	if (!_cursorsReady || kind < 0 || kind >= kNumCursors)
		return false;
	const Cursor &c = _cursors[kind];
	pixels = c.pixels.begin();
	w = c.w;
	h = c.h;
	// The exe centres the sprite on (x - hotX, y - hotY): the pointer tip is at (w/2 + hotX, h/2 + hotY) inside the cell.
	hotX = c.w / 2 + c.hotX;
	hotY = c.h / 2 + c.hotY;
	palette = _cursorPalette;
	return true;
}

void World::updateCursor() {
	if (!_cursorsReady)
		return;
	const bool hide = _cursorKind < 0;
	if (hide != _cursorHidden) {
		CursorMan.showMouse(!hide);
		_cursorHidden = hide;
	}
	if (hide || _shownKind == _cursorKind)
		return;
	if (_cursorKind >= kNumCursors) {
		const UISprite &s = _ext[_cursorKind - kNumCursors];
		const Graphics::PixelFormat argb = Graphics::PixelFormat::createFormatARGB32();
		CursorMan.replaceCursor(s.argb(), s.width(), s.height(), 0, 0, 0, &argb);
		_shownKind = _cursorKind;
		return;
	}
	const byte *pixels, *palette;
	int w, h, hx, hy;
	if (!cursorImage(_cursorKind, pixels, w, h, hx, hy, palette))
		return;
	CursorMan.replaceCursorPalette(palette, 0, 256);
	CursorMan.replaceCursor(pixels, w, h, hx, hy, 0);
	_shownKind = _cursorKind;
}

void World::setCursor(int kind) {
	_cursorKind = kind;
	updateCursor();
}

bool World::takeMenuRequest() {
	const bool r = _menuRequest;
	_menuRequest = false;
	return r;
}

Common::String World::sceneName(int id) const {
	if (id == kSceneChambre)
		return _state.get(kCbVariant) ? "chambreb" : "chambrev";
	if (id < 0 || id >= kNumScenes)
		return Common::String();
	return kSceneNames[id];
}

int World::sceneIdByName(const Common::String &name) {
	if (name.equalsIgnoreCase("chambre") || name.equalsIgnoreCase("chambrev") || name.equalsIgnoreCase("chambreb"))
		return kSceneChambre;
	for (int i = 0; i < kNumScenes; i++) {
		if (name.equalsIgnoreCase(kSceneNames[i]))
			return i;
	}
	return -1;
}

void World::resetMovement() {
	_fwd = _strafe = _pitchSpd = _turn = 0;
	_rotA = _rotB = 0;
	_pos[0] = _pos[1] = _pos[2] = 0;
	_mouseLook = false;
}

Pose3D World::pose() const {
	Pose3D p;
	p.x = _pos[0];
	p.y = _pos[1];
	p.z = _pos[2];
	p.rotA = _rotA;
	p.rotB = _rotB & 0xfff;
	return p;
}

void World::setPose(const Pose3D &p) {
	_pos[0] = (int16)p.x;
	_pos[1] = (int16)p.y;
	_pos[2] = (int16)p.z;
	_rotA = signedAngle(p.rotA);
	_rotB = (int16)(p.rotB & 0xfff);
	_fwd = _strafe = _pitchSpd = _turn = 0;
	for (int k = 0; k < 3; k++)
		_prevPos[k] = _pos[k];
}

bool World::aimAt(const char *name, int dist) {
	const int n = _model.findNode(name);
	if (n < 0)
		return false;
	float p[3];
	if (!nodeCentre(name, p))
		return false;
	// Try camera spots on rings around the node (nearest and straightest first) until the real pick at the screen
	// centre returns the node, the camera survives collide() and nothing opaque is in the way.
	const float ang2 = 2.0f * (float)M_PI / 4096.0f;
	const int32 eyeY = _pos[1];
	// The camera must sit on the walkable ground (ground follow decides its height, like movement() does) and the
	// pitch is clamped to the exe's limit, so the search works on x/z rings only.
	static const float kScale[8] = { 0.4f, 0.6f, 0.8f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f };
	for (int s = 0; s < 8; s++) {
		for (int k = 0; k < 48; k++) {
			const float th = (float)k * (float)M_PI / 24.0f;
			const float r = (float)dist * kScale[s];
			Pose3D pose;
			pose.x = (int32)(p[0] + cosf(th) * r);
			pose.y = 0;
			pose.z = (int32)(p[2] + sinf(th) * r);
			if (pose.x != (int16)pose.x || pose.z != (int16)pose.z)
				continue;
			const int32 cam[3] = { pose.x, eyeY, pose.z };
			int32 gy = eyeY;	// no ground under the probe: movement() leaves the height alone
			_collision.groundY(cam, _groundProbe, gy);
			pose.y = gy;
			const float d[3] = { p[0] - (float)pose.x, p[1] - (float)pose.y, p[2] - (float)pose.z };
			const float dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
			pose.rotA = CLIP((int32)lroundf(asinf(CLIP(-d[1] / dl, -1.0f, 1.0f)) / ang2), -kPitchLimit, kPitchLimit);
			pose.rotB = (int32)lroundf(atan2f(d[0], d[2]) / ang2);
			setPose(pose);
			collide();
			if (_pos[0] != pose.x || _pos[2] != pose.z)
				continue;
			Pose3D cp;
			cameraPose(cp);
			_renderer.render(_model, cp);
			const int hit = _renderer.pick(_model, 320, 240, nullptr);
			if (hit == n) {
				debug("Vangogh: aim '%s': camera at %d,%d,%d rot %d,%d (ring %d, step %d)", name, _pos[0], _pos[1], _pos[2],
					pose.rotA, pose.rotB, s, k);
				return true;
			}
		}
	}
	debug("Vangogh: aim '%s': node at %.0f,%.0f,%.0f, no walkable spot with a clear pick", name, p[0], p[1], p[2]);
	return false;
}

bool World::enterScene(int id, int prev) {
	if (id < 0 || id >= kNumScenes)
		return false;

	const Common::String name = sceneName(id);
	Common::File bfgFile;
	const Common::Path path(Common::String::format("scenes_3d/%s.bfg", name.c_str()));
	if (!bfgFile.open(path)) {
		warning("Vangogh: could not open %s", path.toString().c_str());
		return false;
	}
	BFGFile bfg;
	if (!bfg.load(bfgFile)) {
		warning("Vangogh: failed to parse %s", path.toString().c_str());
		return false;
	}

	if (!_model.load(bfg, name)) {
		_scene = -1;
		return false;
	}

	_scene = id;
	_prevScene = prev;
	_renderer.bindModel(_model);
	registerTextures(bfg);
	loadAnims(bfg);
	_carry = kCarryNone;

	// Collision volumes. BOX.3DI is the default; the museum starts in BOX1 (SetBoxMusee(1)).
	_collision.setActive(nullptr);
	for (int i = 0; i < 5; i++) {
		_boxes[i].clear();
		_boxLoaded[i] = false;
	}
	_boxLoaded[0] = _boxes[0].load(bfg, "BOX.3DI");
	for (int i = 0; i < kMaxBoxVariants && kBoxVariants[id][i]; i++)
		_boxLoaded[i + 1] = _boxes[i + 1].load(bfg, kBoxVariants[id][i]);
	if (!setBox(id == kSceneMusee ? 1 : 0))
		warning("Vangogh: scene %s has no collision volume; the camera will pass through walls", name.c_str());

	_groundProbe = (id == kSceneTerrasse) ? 0xf0 : 500;

	locateObjects();
	_framesInScene = 0;
	_hoverObject = -1;
	_hoverName[0] = '\0';
	_cursorKind = kCursorArrow;
	_glideLeft = 0;
	if (_clipActive) {	// entering a scene by any other route (debug goto) abandons a running clip
		_clip.close();
		_clipActive = false;
	}

	// Camera.
	resetMovement();
	Pose3D p = Pose3D();
	bool havePose = false;
	if (id == kSceneMusee) {
		const MuseumPose &m = kMuseumPoses[(prev > 0 && prev < kNumScenes) ? prev : 0];
		p.x = m.x; p.y = m.y; p.z = m.z; p.rotA = m.a; p.rotB = m.b;
		havePose = true;
	} else if (prev > 0 && prev != id) {
		for (uint i = 0; i < ARRAYSIZE(kTransitions); i++) {
			const PoseRow &r = kTransitions[i];
			if (r.from == prev && r.to == id) {
				p.x = r.x; p.y = r.y; p.z = r.z; p.rotA = r.a; p.rotB = r.b;
				havePose = true;
				break;
			}
		}
	}
	if (!havePose && !defaultPoseForScene(name, p)) {
		const PoseRow &r = kTransitions[kHopiextFallbackRow];
		p.x = r.x; p.y = r.y; p.z = r.z; p.rotA = r.a; p.rotB = r.b;
		warning("Vangogh: scene %s has no default pose, using the 9->2 arrival pose", name.c_str());
	}
	initSceneState(&p);
	setPose(p);

	debug("Vangogh: entered %s (id %d) from %d at %d,%d,%d rot %d,%d", name.c_str(), id, prev, _pos[0], _pos[1], _pos[2], _rotA, _rotB);
	return true;
}

bool World::setBox(int n) {
	if (n < 0 || n > kMaxBoxVariants || !_boxLoaded[n])
		return false;
	_collision.setActive(&_boxes[n]);
	_box = n;
	return true;
}

void World::locateObjects() {
	uint count = 0;
	const SpecEntry *specs = specsForScene(_scene, _state.get(kCbVariant), count);
	_numObjects = 0;
	for (uint i = 0; i < count && _numObjects < kMaxObjects; i++) {
		Object &o = _objects[_numObjects];
		o.name = specs[i].name;
		o.type = specs[i].type;
		o.node = -1;
		for (uint n = 0; n < _model.numNodes(); n++) {
			if (scumm_stricmp(_model.node(n).name, o.name) == 0) {
				o.node = n;
				break;
			}
		}
		if (o.node < 0)
			debug("Vangogh: %s: object '%s' has no node in the model", sceneName(_scene).c_str(), o.name);
		_numObjects++;
	}
}

int World::objectType(const char *name) const {
	for (uint i = 0; i < _numObjects; i++) {
		if (scumm_stricmp(_objects[i].name, name) == 0)
			return _objects[i].type;
	}
	return 0;
}

// ---------------------------------------------------------------------------
// One frame.

void World::cameraPose(Pose3D &p) const {
	p = pose();
}

void World::frame(const WorldInput &in, float dt) {
	if (_scene < 0)
		return;

	if (in.esc)
		_menuRequest = true;

	if (_clipActive) {
		stepClip(in);
		return;
	}

	if (_glideLeft > 0) {
		stepGlide();
		Pose3D p;
		cameraPose(p);
		_renderer.render(_model, p);
		_hoverObject = -1;
		_hoverName[0] = '\0';
		_cursorKind = kCursorArrow;
		if (_glideLeft == 0) {
			char clipName[16];
			if (arrivalClip(_glideDest, clipName, sizeof(clipName)) && playClip(clipName, _glideDest, 0))
				return;
			changeScene(_glideDest, 0);
		}
		return;
	}

	movement(in, dt);
	collide();

	Pose3D p;
	cameraPose(p);
	_renderer.render(_model, p);

	_framesInScene++;
	stepAnims(CLIP((int)(dt + 0.5f), 1, 10));
	sceneTick(CLIP((int)(dt + 0.5f), 1, 10));

	// The inventory bar (Space) takes its own clicks; while the bar is under the pointer or an item is carried the
	// scene does not see them.
	if (in.space)
		_inv.toggle();
	int stored = -1;
	const bool consumed = _inv.update(_state, in.mouseX, in.mouseY, in.click, stored);
	WorldInput sceneIn = in;
	if (consumed || _inv.carried() >= 0 || _inv.inBar(in.mouseX, in.mouseY))
		sceneIn.click = false;

	// The "retour" corner (decomp 7593, 16927, 16951): in a scene with the bar hidden and nothing in hand, the
	// bottom-left corner shows a return button; a click there leaves like Backspace, before the scene sees it.
	_retourShown = _scene != kSceneMusee && _inv.state() == Inventory::kHidden && _inv.carried() < 0 && _carry == kCarryNone
		&& retourHot(in.mouseX, in.mouseY);
	if (_retourShown) {
		sceneIn.click = false;
		if (in.click) {
			debug("Vangogh: retour corner, leaving %s for the museum", sceneName(_scene).c_str());
			_retourShown = false;
			leaveToMuseum();
			return;
		}
	}
	pickAndAct(sceneIn);
	if (_retourShown)
		_cursorKind = kCursorPoint;
	if (_inv.carried() >= 0)
		_cursorKind = -1;
	composeOverlay(in.mouseX, in.mouseY);

	// Scene tick: exits. Camera thresholds first (they are evaluated every frame), then Backspace.
	if (_scene != kSceneMusee) {
		const int dest = cameraExit(_scene, _pos[0], _pos[2]);
		if (dest >= 0) {
			debug("Vangogh: camera exit %s -> scene %d at x=%d z=%d", sceneName(_scene).c_str(), dest, _pos[0], _pos[2]);
			changeScene(dest, _scene);
			return;
		}
		if (in.backspace) {
			debug("Vangogh: leaving %s for the museum", sceneName(_scene).c_str());
			leaveToMuseum();
		}
	}
}

void World::composeOverlay(int mouseX, int mouseY) {
	_overlay = _inv.visible() || _inv.carried() >= 0 || _retourShown;
	if (!_overlay)
		return;
	const Graphics::Surface &src = _renderer.surface();
	if (_comp.w != src.w || _comp.h != src.h || _comp.format != src.format) {
		_comp.free();
		_comp.create(src.w, src.h, src.format);
	}
	for (int y = 0; y < src.h; y++)
		memcpy(_comp.getBasePtr(0, y), src.getBasePtr(0, y), (size_t)src.w * src.format.bytesPerPixel);
	_inv.draw(_comp, _state, mouseX, mouseY);
	if (_retourShown)
		_retour.blit(_comp, 0, kRetourX, kRetourBottom - _retour.height());
}

bool World::retourHot(int x, int y) const {
	return x < _retour.width() + 10 && y > kRetourBottom - _retour.height();
}

bool World::takeFinRequest() {
	const bool r = _finRequest;
	_finRequest = false;
	return r;
}

bool World::takePageRequest(int &page) {
	if (_pageRequest < 0)
		return false;
	page = _pageRequest;
	_pageRequest = -1;
	return true;
}

bool World::returnFromPage(int target) {
	// transition_commit reloads the world: the same scene at the saved pose, or the museum.
	const int scene = _state.getByte(kOfsScene);
	const int prev = _state.getByte(kOfsPrevScene);
	_pageRequest = -1;
	_inv.reset();
	if (target == Page::kExitMuseum)
		return enterScene(kSceneMusee, scene);
	if (!enterScene(scene, prev))
		return false;
	Pose3D p;
	p.x = (int16)_state.get(kOfsCamX);
	p.y = (int16)_state.get(kOfsCamY);
	p.z = (int16)_state.get(kOfsCamZ);
	p.rotA = (int16)_state.get(kOfsCamRotA);
	p.rotB = (int16)_state.get(kOfsCamRotB);
	setPose(p);
	return true;
}

void World::changeScene(int dest, int prev) {
	if (!enterScene(dest, prev))
		warning("Vangogh: scene change %d -> %d failed", prev, dest);
}

void World::movement(const WorldInput &in, float dt) {
	// sub_42a4e0, in the exe's order and with its int16 state.
	if (in.f1)
		_mouseLook = !_mouseLook;

	if (in.up)
		_fwd = (int16)(_fwd + 10);
	if (in.down)
		_fwd = (int16)(_fwd - 5);
	if (in.pgDown)
		_pitchSpd = -15;
	if (in.pgUp)
		_pitchSpd = 15;

	bool strafing = false;
	if (in.alt || _mouseLook) {
		_strafe >>= 1;
		if (in.right)
			_strafe = (int16)(_strafe + 10);
		if (in.left)
			_strafe = (int16)(_strafe - 10);
		strafing = true;
	} else {
		if (in.left)
			_turn = toI16((double)_turn - 7.0 * dt);
		if (in.right)
			_turn = toI16(7.0 * dt + (double)_turn);
	}

	if (_mouseLook) {
		_turn = toI16(((in.mouseX - 320) / 320.0f) * 16.0f);
		_pitchSpd = toI16((-(in.mouseY - 240) / 240.0f) * 16.0f);
	}

	_strafe = (int16)CLIP<int>(_strafe, -kStrafeMax, kStrafeMax);
	_turn = (int16)CLIP<int>(_turn, -kTurnMax, kTurnMax);
	_fwd = (int16)CLIP<int>(_fwd, -kFwdMax, kFwdMax);

	_rotB = toI16((double)_turn * dt + (double)_rotB);
	_rotA = toI16((double)_pitchSpd * dt + (double)_rotA);
	if (_rotA >= 0x801)
		_rotA = (int16)(_rotA - 0x1000);
	if (_rotA < -kPitchLimit)
		_rotA = -kPitchLimit;
	if (_rotA > kPitchLimit)
		_rotA = kPitchLimit;

	const int a = _rotA & 0xfff, b = _rotB & 0xfff;
	const int32 m0 = gCos[b];
	const int32 m2 = (gSin[b] * gCos[a]) >> 15;
	const int32 m6 = -gSin[b];
	const int32 m8 = (gCos[b] * gCos[a]) >> 15;

	const float v = (float)(2.0 * dt);
	double x = _pos[0], z = _pos[2];
	if (strafing) {
		x = toI16((double)m0 * _strafe * v / 32768.0 + x);
		z = toI16((double)m6 * _strafe * v / 32768.0 + z);
	}
	x = toI16((double)m2 * _fwd * v / 32768.0 + x);
	z = toI16((double)m8 * _fwd * v / 32768.0 + z);
	_pos[0] = (int16)x;
	_pos[2] = (int16)z;

	// Ground follow (sub_42ac60).
	const int32 cam[3] = { _pos[0], _pos[1], _pos[2] };
	int32 groundY;
	if (_collision.groundY(cam, _groundProbe, groundY))
		_pos[1] = (int16)groundY;

	_fwd = toI16(_fwd * 0.6);
	_pitchSpd = toI16(_pitchSpd * 0.6);
	_turn = toI16(_turn * 0.6);
}

void World::collide() {
	// sub_425d30: sweep the camera sphere; on a hit the camera snaps to the corrected centre and stops.
	const int32 cur[3] = { _pos[0], _pos[1], _pos[2] };
	if (_collision.moveSphere(cur, _prevPos, kSphereRadius)) {
		for (int k = 0; k < 3; k++)
			_pos[k] = (int16)_prevPos[k];
		_fwd = 0;
	}
}

void World::pickAndAct(const WorldInput &in) {
	_hoverObject = -1;
	_hoverName[0] = '\0';
	_cursorKind = kCursorArrow;

	// Carrying a scene object (dat_54d864): the pick point is the middle of the carried sprite, and the next click
	// goes to whatever node is under it, whatever its type (decomp 418430 "else" branch).
	int px = in.mouseX, py = in.mouseY;
	if (_carry != kCarryNone) {
		const UISprite &c = _ext[_carry - kNumCursors];
		px += c.width() / 2;
		py += c.height() / 2;
	}
	float depth = 0.0f;
	const int node = _renderer.pick(_model, px, py, &depth);
	if (_carry != kCarryNone) {
		_cursorKind = _carry;
		if (in.click)
			onCarryClick(node >= 0 ? _model.node(node).name : "");
		return;
	}
	// The cursor follows the object's type (2, 3, 4, 6, 0x3c); the click itself is dispatched by name for every
	// object of the table and each handler applies its own gate (the exe's click branch never looks at the type).
	float distance = 0.0f;
	int clicked = -1;
	if (node >= 0) {
		for (uint i = 0; i < _numObjects; i++) {
			const Object &o = _objects[i];
			if (o.node != node)
				continue;
			float vp[3];
			_renderer.nodeViewPos(node, vp);
			distance = sqrtf(vp[0] * vp[0] + vp[2] * vp[2]);
			// Museum objects are only live up close (dat_4eee84).
			if (_scene == kSceneMusee && distance >= kPickRange)
				break;
			clicked = i;
			const byte t = o.type;
			if (t == 2 || t == 3 || t == 4 || t == 6 || t == 0x3c) {
				_hoverObject = i;
				Common::strlcpy(_hoverName, o.name, sizeof(_hoverName));
			}
			break;
		}
	}

	if (_hoverObject >= 0) {
		// Cursor by object type (the jump tables at 0x418ca4 and friends).
		switch (_objects[_hoverObject].type) {
		case 2: _cursorKind = kCursorHand; break;
		case 3: _cursorKind = kCursorPuzzle; break;
		case 4: _cursorKind = kCursorPoint; break;
		case 6: _cursorKind = kCursorDoor; break;
		default: _cursorKind = kCursorSolved; break;
		}
	}

	if (in.click && clicked >= 0)
		onObjectClick(_objects[clicked].name, _objects[clicked].type, distance);
}

namespace {

/** A type-2 object: picking it up carries item @a item and sets its "taken" flag (world.md 6.4 item table). */
struct Pickup {
	byte scene;
	const char *object;
	byte item;
	uint flag;
};

const Pickup kPickups[] = {
	{ kSceneMusee, "etoile", 0, kOfsStarTaken },
	{ kSceneMaisonet, "terre", 1, kMeEarthTaken },
	{ kSceneMaisonet, "plume", 2, kMeFeatherTaken },
	{ kSceneMangeurs, "theieres", 3, kMgKettleTaken },
	{ kSceneMangeurs, "patat01", 4, kMgPotatoTaken },
	{ kSceneCafe, "carte", 5, kCfMapTaken },
	{ kSceneCafe, "briket", 6, kCfBriketTaken },
	{ kScenePont, "apple01", 7, kPoAppleTaken },
	{ kSceneChambre, "ombrelle", 8, kCbUmbrellaTaken },
	{ kSceneCafe, "pichet", 9, kCfPitcherTaken },
	{ kSceneChambre, "cadre", 10, kCbFrameTaken },
	{ kSceneMaisonj, "escargot", 11, kMjSnailTaken },
	{ kSceneChambre, "Object02", 12, kCbObject02Taken },
	{ kSceneChambre, "camee", 13, kCbCameoTaken },
	{ kSceneChambre, "shoes", 14, kCbShoesTaken },
	{ kSceneChambre, "shoes01", 14, kCbShoesTaken },
	{ kSceneChambre, "Object06", 14, kCbShoesTaken },
	{ kSceneChambre, "Object09", 15, kCbObject09Taken },
	{ kSceneCafe, "ombre", 16, kCfBilliardDone },
	{ kSceneTerrasse, "lunette", 17, kTeGlassesTaken },
	{ kSceneChambre, "Object07", 18, kCbObject07Taken },
	{ kSceneCafe, "orloge", 19, kCfClockTaken },
	{ kSceneHopiint, "BONNET", 20, kHiBonnetTaken },
	{ kSceneHopiint, "LAMPE", 21, kHiLampTaken },
	{ kSceneHopiint, "BOUGIE", 22, kHiCandleTaken },
	{ kSceneHopiint, "CROIX", 23, kHiCrossTaken },
	{ kSceneChambre, "mirroirmor", 24, kCbMirrorShardTaken },
	{ kSceneCafe, "theiere", 25, kCfTeapotTaken },
	{ kSceneTerrasse, "platode", 26, kTeDieTaken },
	{ kSceneTerrasse, "lettre", 27, kTeLetterTaken },
	{ kScenePont, "encre01", 28, kPoInkTaken },
	{ kSceneJardin, "fuzz", 29, kJaFuzzTaken },
	{ kSceneJardin, "rato", 30, kJaRatoTaken },
	{ kSceneAuberge, "stetoscop", 31, kAuStethoscopeTaken },
	{ kSceneChamp, "gun", 32, kChGunTaken },
	{ kSceneEglise, "cerf", 33, kEgKiteDone },
	{ kSceneEglise, "gerbe", 34, kEgSheafTaken }
};

}

bool World::pickUpObject(const char *name) {
	if (_inv.carried() >= 0)
		return true;
	for (uint i = 0; i < ARRAYSIZE(kPickups); i++) {
		const Pickup &p = kPickups[i];
		if (p.scene != _scene || scumm_stricmp(p.object, name) != 0)
			continue;
		_state.set(p.flag, 1);
		if (p.item == 14) {
			hideObj("shoes");
			hideObj("shoes01");
			hideObj("Object06");
		} else {
			hideObj(p.object);
		}
		pickupEffects(p.object);
		_inv.pickUp(p.item);
		debug("Vangogh: picked up item %d (%s)", p.item, name);
		return true;
	}
	return false;
}

void World::onObjectClick(const char *name, int type, float distance) {
	_clickCount++;
	debug("Vangogh: click %s '%s' (type %d, distance %.0f)", sceneName(_scene).c_str(), name, type, distance);

	if (sceneClick(name, type, distance))
		return;
	if (type == 2 && pickUpObject(name))
		return;
	if (type == 3 || type == 0x3c) {	// a solved launcher (0x3c) still opens its page (the exe tests only "!= 0xff")
		const int page = Page::launcherPage(_scene, name);
		if (page >= 0) {
			_pageRequest = page;
			return;
		}
	}

	// Exits are data (kClickExits).
	for (uint i = 0; i < ARRAYSIZE(kClickExits); i++) {
		const ClickExit &e = kClickExits[i];
		if (e.scene != _scene || scumm_stricmp(e.object, name) != 0)
			continue;
		if (e.maxDist && distance >= (float)e.maxDist) {
			debug("Vangogh: '%s' is %.0f away, needs < %d", name, distance, e.maxDist);
			return;
		}
		if (_scene == kSceneMusee) {
			if (e.dest == kSceneChambre)
				_state.set(kCbVariant, 1);	// m03_02 is the bedroom painting; chambrev is only the pre-game state
			startPaintingClick(e.dest);
		} else {
			changeScene(e.dest, _scene);
		}
		return;
	}
	// Type 255 is the exe's "inert" hotspot byte (world.md 6.2): no hover cursor, and the click falls through unhandled.
	if (type == 255)
		debug("Vangogh: '%s' in %s is inactive (type 255), click ignored", name, sceneName(_scene).c_str());
	else
		debug("Vangogh: no action implemented for '%s' in %s (type %d)", name, sceneName(_scene).c_str(), type);
}

void World::startGlide(int dest) {
	// Mode 4 (sub_427180/sub_427290): the camera glides to the museum pose in front of the painting, then the scene loads.
	cameraPose(_glideFrom);
	const MuseumPose &m = kMuseumPoses[dest];
	_glideTo.x = m.x; _glideTo.y = m.y; _glideTo.z = m.z; _glideTo.rotA = m.a; _glideTo.rotB = m.b;
	_glideDest = dest;
	_glideLeft = kGlideFrames;
}

void World::stepGlide() {
	_glideLeft--;
	const int done = kGlideFrames - _glideLeft;

	// Shortest way round for the angles.
	const int dA = signedAngle(_glideTo.rotA - _glideFrom.rotA);
	const int dB = signedAngle(_glideTo.rotB - _glideFrom.rotB);
	Pose3D p;
	p.x = _glideFrom.x + (_glideTo.x - _glideFrom.x) * done / kGlideFrames;
	p.y = _glideFrom.y + (_glideTo.y - _glideFrom.y) * done / kGlideFrames;
	p.z = _glideFrom.z + (_glideTo.z - _glideFrom.z) * done / kGlideFrames;
	p.rotA = _glideFrom.rotA + dA * done / kGlideFrames;
	p.rotB = _glideFrom.rotB + dB * done / kGlideFrames;
	setPose(p);
}

} // End of namespace Vangogh
