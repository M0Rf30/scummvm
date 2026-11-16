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

#include "common/debug.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "vangogh/gamestate.h"
#include "vangogh/movietable.h"
#include "vangogh/page.h"
#include "vangogh/pagegames.h"
#include "vangogh/sound.h"
#include "vangogh/vangogh.h"
#include "vangogh/world.h"

namespace Vangogh {

/*
 * Notes on the page set (world.md sec.7, flow.md sec.5.3).
 *
 * Act sizes. The sunflower counter of a group is NbTournesols while the player is in that wing: the exe resets it at
 * load when it equals 1, 3 or 15 and the wing's "current act" flag is not set (load_game fix-up), and the auberge
 * opens at 5. flow.md (1/3/15/5) and world.md (1/3/15/6) differ only for the last act: its pages are 19..24, but page
 * 21 is the ending tableau, which has no item and no sunflower, so the collectible count of group 3 is the five
 * pages 19, 20, 22, 23 and 24 -- the counter tops out at 5 and 6 counts the tableau.
 *
 * Slot handlers. After the CapsOP animation the exe runs the slot's handler every tick (state 0xd): the item's movie or
 * voice, or one of the twelve mini games (items 3, 6, 12, 13, 14, 17, 21, 24, 26, 28, 31, 34). They are PageGame
 * subclasses (pagegames*.cpp); a click on a placed capsule replays its handler (state 0xb -> 0xe). Movies and voices
 * end on a held left button, never on a click.
 */

namespace {

/** One 2D puzzle descriptor (0x6c bytes at 0x4f3950): world.md 7.1. */
struct PageDesc {
	const char *bg;			// +0x14 background TGP
	const char *zoom;		// base of the "a" (painting animation) and "b" (big view) names
	byte numItems;
	int8 items[3];			// capsule slots, top to bottom
	int16 loupeX, loupeY;	// +0x38, +0x3c
	byte sun;				// +0x40 TOURN<n> variant
	byte group;				// +0x44 group counter
	const char *pa;			// +0x5c PA<n> sprite base (a = growing, b = empty, c = after)
};

const PageDesc kPages[Page::kNumPages] = {
	{ "A14_031a", "A14_03", 1, { 0, -1, -1 }, 325, 430, 1, 0, nullptr },
	{ "A01_02", "A01_02", 1, { 1, -1, -1 }, 436, 290, 2, 1, "pa01_02" },
	{ "A01_03", "A01_03", 2, { 3, 2, -1 }, 436, 373, 0, 1, "pa01_03" },
	{ "A11_01", "A11_01", 1, { 4, -1, -1 }, 436, 398, 0, 1, "pa11_01" },
	{ "A03_01", "A03_01", 3, { 5, 6, 7 }, 441, 417, 0, 2, "pa03_01" },
	{ "A13_08", "A13_08", 1, { 8, -1, -1 }, 329, 430, 0, 2, "pa13_08" },
	{ "A13_04", "A13_04", 1, { 9, -1, -1 }, 436, 413, 0, 2, "pa13_04" },
	{ "A03_02", "A03_02", 3, { 10, 12, 11 }, 436, 412, 0, 2, "pa03_02" },
	{ "A13_03", "A13_03", 2, { 13, 14, -1 }, 436, 404, 0, 2, "pa13_03" },
	{ "A13_02", "A13_02", 1, { 15, -1, -1 }, 436, 331, 2, 2, "pa13_02" },
	{ "A13_05", "A13_05", 2, { 16, 17, -1 }, 333, 430, 0, 2, "pa13_05" },
	{ "A13_07", "A13_07", 1, { 18, -1, -1 }, 326, 430, 0, 2, "pa13_07" },
	{ "A03_03", "A03_03", 1, { 19, -1, -1 }, 436, 406, 0, 2, "pa03_03" },
	{ "A03_04", "A03_04", 2, { 21, 20, -1 }, 436, 411, 0, 2, "pa03_04" },
	{ "A13_01", "A13_01", 2, { 22, 23, -1 }, 436, 412, 0, 2, "pa13_01" },
	{ "A13_06", "A13_06", 2, { 24, 25, -1 }, 333, 430, 0, 2, "pa13_06" },
	{ "A03_05", "A03_05", 1, { 26, -1, -1 }, 331, 430, 0, 2, "pa03_05" },
	{ "A13_09", "A13_09", 1, { 27, -1, -1 }, 332, 430, 0, 2, "pa13_09" },
	{ "A03_06", "A03_06", 1, { 28, -1, -1 }, 441, 429, 0, 2, "pa03_06" },
	{ "A04_01", "A04_01", 2, { 29, 30, -1 }, 436, 271, 2, 3, "pa04_01" },
	{ "A14_01", "A14_01", 1, { 31, -1, -1 }, 338, 430, 0, 3, "pa14_01" },
	{ "A14_032a", "A14_03", 0, { -1, -1, -1 }, 325, 430, 0, 3, nullptr },
	{ "A04_02", "A04_02", 1, { 32, -1, -1 }, 436, 270, 2, 3, "pa04_02" },
	{ "A04_03", "A04_03", 1, { 33, -1, -1 }, 331, 430, 0, 3, "pa04_03" },
	{ "A14_02", "A14_02", 1, { 34, -1, -1 }, 436, 417, 0, 3, "pa14_02" }
};

/** World click -> page (type 3 objects). */
struct Launcher {
	byte scene;
	const char *object;
	byte page;
};

const Launcher kLaunchers[] = {
	{ kSceneMusee, "vase", 0 }, { kSceneMaisonet, "nid", 1 }, { kSceneMangeurs, "bersso", 2 }, { kSceneMangeurs, "POT", 3 },
	{ kSceneCafe, "lampe4", 4 }, { kSceneCafe, "gant", 5 }, { kSceneCafe, "mirroir", 6 },
	{ kSceneChambre, "oreiller", 7 }, { kSceneChambre, "tabpay", 8 }, { kSceneChambre, "TIROIR", 9 },
	{ kSceneChambre, "tab02", 10 }, { kSceneChambre, "tab01", 11 }, { kSceneMaisonj, "pendule", 12 },
	{ kSceneHopiint, "FOU", 13 }, { kSceneHopiint, "ECHIK", 13 }, { kSceneHopiext, "arbre11", 14 },
	{ kSceneHopiint, "MIRROIRVG", 15 }, { kSceneTerrasse, "drapo", 16 }, { kSceneTerrasse, "KASKET", 17 },
	{ kScenePont, "chemise", 18 }, { kSceneJardin, "partoche", 19 }, { kSceneAuberge, "casquette", 20 },
	{ kSceneAuberge, "tableau", 21 }, { kSceneChamp, "coclicot", 22 }, { kSceneEglise, "eglise10", 23 },
	{ kSceneChamp, "sabots", 24 }
};

// Items whose placement sound is OP<nn> instead of OP_GENE.
const byte kOwnOpSound[] = { 3, 6, 8, 12, 14, 15, 18, 19, 21, 22, 25, 27, 28, 29, 32, 34 };

struct R {
	int x, y, w, h;
	bool has(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

const R kSlotDrop[3] = { { 12, 53, 59, 58 }, { 12, 143, 59, 58 }, { 12, 233, 59, 58 } };	// 0x4f3810
const int kSlotSprite[3][2] = { { 0, 29 }, { 0, 116 }, { 0, 222 } };				// 0x4f3840
const int kSlotFly[3][2] = { { 40, 83 }, { 40, 172 }, { 40, 263 } };				// 0x4f3888: where a refused item starts flying back
const R kArrowLeft = { 13, 435, 23, 30 }, kArrowRight = { 509, 435, 22, 30 };		// 0x4f3730/40
const R kCounter = { 590, 400, 45, 80 };											// 0x4f38e0
const R kSunRect[3] = { { 127, 313, 30, 26 }, { 111, 244, 24, 32 }, { 148, 290, 30, 26 } };	// 0x4f38f0
const int kSunSprite[3][2] = { { 122, 344 }, { 132, 295 }, { 166, 333 } };		// 0x4f3920
const R kChromeRight = { 605, 437, 24, 29 }, kChromeLeft = { 11, 437, 24, 29 };	// 0x4f38a0/b0
const R kEndClick = { 121, 325, 41, 45 };											// 0x4f3940

const int kBarTop = 420;			// bar y when shown (0x1df - 0x3b)
const int kBarStep = 12;
const int kBarIconX = 67, kBarIconStride = 70;		// 0x4f37b0
const int kBarHitX = 88, kBarHitY = 436;			// 0x4f3750, 28x27 each
const int kRetourRightX = 600, kRetourRightY = 435, kRetourLeftX = 8, kRetourLeftY = 432;
const int kBadgeX = 558, kBadgeY = 21;
const int kPotX = 558, kPotY = 422;
const int kCellTicks = 2;			// sprite animations advance every other 40 ms tick (dat_52a458 & 1)
const int kFlyTicks = 12;
const int kP0LoupeTicks = 251;		// sub_413550 state 1 time-out
const int kHoldTicks = 40;
const int kEndHoldTicks = 60;
const int kMovieP0A = 32, kMovieP0B = 33, kMovieP0C = 34, kMovieEndA = 64, kMovieEndB = 65;

// Cursor kinds of curseurs.spr.
const int kCurArrow = 0, kCurBusy = 9, kCurFinger = 11, kCurGrab = 12, kCurDrag = 13;

/** Copies the rect of @p src into @p dst (identical pixel formats), clipped. */
void copyRect(Graphics::Surface &dst, const Graphics::Surface &src, int x, int y, int w, int h) {
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	w = MIN(w, MIN(src.w, dst.w) - x);
	h = MIN(h, MIN(src.h, dst.h) - y);
	if (w <= 0 || h <= 0 || src.format != dst.format)
		return;
	const int bpp = dst.format.bytesPerPixel;
	for (int row = 0; row < h; row++)
		memcpy(dst.getBasePtr(x, y + row), src.getBasePtr(x, y + row), (size_t)w * bpp);
}

}

// ---------------------------------------------------------------------------

Page::Page() : _st(nullptr), _sound(nullptr), _active(false), _page(0), _exit(0), _ending(false), _menuRequest(false),
	_state(kIdle), _next(kNextNone), _tick(0), _stateTicks(0), _scrollX(0), _scrollY(0), _movieIndex(-1), _movieSkippable(false), _movieLayer(false), _game(nullptr), _gameSlot(-1),
	_gameDropped(false), _gameOk(false), _animSlot(-1),
	_barState(kBarHidden), _barY(480), _barCount(0), _barScroll(0), _dragItem(-1), _dragSlot(-1),
	_dragIndex(0), _dragStart(0), _flyTick(0), _flyItem(-1), _flyIndex(0), _loupe(kLoupeHidden), _hoverSlot(-1),
	_retourCell(0), _lastPlaced(-1), _cursor(0), _mx(0), _my(0), _p0Cell(-1), _kx(0), _ky(0), _returnState(kIdle) {
	memset(_barList, 0, sizeof(_barList));
	memset(_flyFrom, 0, sizeof(_flyFrom));
	memset(_flyTo, 0, sizeof(_flyTo));
}

Page::~Page() {
	close();
}

int Page::launcherPage(int scene, const char *name) {
	for (uint i = 0; i < ARRAYSIZE(kLaunchers); i++) {
		if (kLaunchers[i].scene == scene && scumm_stricmp(kLaunchers[i].object, name) == 0)
			return kLaunchers[i].page;
	}
	return -1;
}

const char *Page::pageName(int page) {
	return page >= 0 && page < kNumPages ? kPages[page].bg : "";
}

bool Page::takeMenuRequest() {
	const bool r = _menuRequest;
	_menuRequest = false;
	return r;
}

int Page::slotOfItem(int item) const {
	const PageDesc &d = kPages[_page];
	for (int s = 0; s < d.numItems; s++) {
		if (d.items[s] == item)
			return s;
	}
	return -1;
}

const char *Page::stateName() const {
	static const char *const names[] = {
		"idle", "drag", "reject", "place-open", "playback", "game", "place-close", "click-open", "complete", "sun-ready",
		"sun-drag", "pot", "after", "zoom", "paint", "p0-loupe", "p0-hold", "end-wait", "end-hold", "done"
	};
	return names[_state];
}

// ---------------------------------------------------------------------------
// Opening and closing.

void Page::loadBackground(const Common::String &name) {
	_bgName = name;
	if (!loadTgpImage(name, _bg)) {
		_bg.free();
		_bg.create(640, 480, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
	}
}

bool Page::open(int page, GameState &state, Sound *sound) {
	if (page < 0 || page >= kNumPages)
		return false;
	close();
	_st = &state;
	_sound = sound;
	_page = page;
	const PageDesc &d = kPages[page];

	if (!_invent.loadSpr("invent") || !_opi.loadSpr("opi") || !_op.loadSpr("op") || !_count.loadSpr("count") ||
		!_loupeOut.loadSpr("loupeout") || !_retour.loadSpr("retour") || !_retourM.loadSpr("retourm") || !_pot.loadSpr("pot")) {
		warning("Vangogh: page %d: shared sprites are missing", page);
		return false;
	}
	_tourn.loadSpr(Common::String::format("tourn%u", d.sun));

	for (int s = 0; s < d.numItems; s++) {
		const int item = d.items[s];
		_capsE[s].loadSpr(Common::String::format("capse%02d", item));
		_capsOP[s].loadSpr(Common::String::format("capsop%02d", item));
		_capsAO[s].loadSpr(Common::String::format("capsao%02d", item));
		_capsAC[s].loadSpr(Common::String::format("capsac%02d", item));
	}
	if (d.pa) {
		_paA.loadSpr(Common::String(d.pa) + "a");
		_paB.loadSpr(Common::String(d.pa) + "b");
		_paC.loadSpr(Common::String(d.pa) + "c");
	}
	_paint.loadSpr(Common::String(d.zoom) + "a");
	if (page == 0)
		_p0.loadSpr("a14_031a");

	_fb.free();
	_fb.create(640, 480, g_system->getScreenFormat());

	loadBackground(d.bg);

	_active = true;
	_exit = 0;
	_ending = false;
	_menuRequest = false;
	_tick = 0;
	_anim = Anim();
	_layer = Anim();
	_animSlot = -1;
	_loupeAnim = Anim();
	_dragItem = -1;
	_flyItem = -1;
	_p0Cell = -1;
	_kx = _ky = 0;
	_next = kNextNone;
	_movieIndex = -1;
	_barState = kBarHidden;
	_barY = 480;
	_lastPlaced = -1;
	_st->setByte(kOfsPage, (byte)page);

	buildBar();

	bool anyPlaced = false;
	for (int s = 0; s < d.numItems; s++)
		anyPlaced |= _st->itemPlaced[d.items[s]] != 0;
	_loupe = anyPlaced ? kLoupeVisible : kLoupeHidden;
	if (anyPlaced)
		_lastPlaced = d.items[d.numItems - 1];

	if (_st->pageDone[page] && _paC.loaded()) {
		_layer.spr = &_paC;
		_layer.cell = _paC.numCells() - 1;
	}

	setState(kIdle);
	if (page == 21) {
		// sub_413d30: the ending page opens with its movie.
		startMovie(kMovieEndA, false);
		_next = kNextEndWait;
		setState(kPlayback);
	}
	return true;
}

void Page::close() {
	delete _game;
	_game = nullptr;
	_clip.close();
	_movieLayer = false;
	_active = false;
	_big.free();
	_bg.free();
	_fb.free();
	_invent.clear(); _opi.clear(); _op.clear(); _count.clear(); _loupeOut.clear(); _retour.clear(); _retourM.clear();
	_pot.clear(); _tourn.clear(); _paA.clear(); _paB.clear(); _paC.clear(); _paint.clear(); _p0.clear();
	for (int s = 0; s < 3; s++) {
		_capsE[s].clear(); _capsOP[s].clear(); _capsAO[s].clear(); _capsAC[s].clear();
	}
}

void Page::setState(State s) {
	_state = s;
	_stateTicks = 0;
}

// ---------------------------------------------------------------------------
// Bar list: the owned items that are not placed (sub_411a10 builds it from the inventory).

void Page::buildBar() {
	_barCount = 0;
	for (int i = 0; i < GameState::kNumItems; i++) {
		if (_st->item(i) == 1 && !_st->itemPlaced[i])
			_barList[_barCount++] = i;
	}
	_barScroll = 0;
}

void Page::removeFromBar(int index) {
	for (int i = index; i + 1 < _barCount; i++)
		_barList[i] = _barList[i + 1];
	_barCount--;
	_barScroll = CLIP(_barScroll, 0, MAX(0, _barCount - 6));
}

void Page::insertInBar(int item, int index) {
	index = CLIP(index, 0, _barCount);
	for (int i = _barCount; i > index; i--)
		_barList[i] = _barList[i - 1];
	_barList[index] = item;
	_barCount++;
}

// ---------------------------------------------------------------------------
// Animation and sound helpers.

void Page::startAnim(Anim &a, const UISprite *spr, int x, int y) {
	a.spr = spr;
	a.cell = 0;
	a.x = x;
	a.y = y;
	a.active = spr && spr->loaded();
}

bool Page::stepAnim(Anim &a) {
	if (!a.active)
		return true;
	if (_tick % kCellTicks)
		return false;
	if (a.cell + 1 >= (int)a.spr->numCells()) {
		a.active = false;
		return true;
	}
	a.cell++;
	return false;
}

void Page::play(const char *name) {
	if (!_sound)
		return;
	const uint16 h = _sound->load(name);
	if (h)
		_sound->play(h, false);
}

void Page::playItemSound(int item) {
	bool own = false;
	for (uint i = 0; i < ARRAYSIZE(kOwnOpSound); i++)
		own |= kOwnOpSound[i] == item;
	play(own ? Common::String::format("OP%02d", item).c_str() : "OP_GENE");
}

void Page::startMovie(int index, bool noSkip) {
	_clip.close();
	const MovieEntry *e = movieByIndex(index);
	_movieIndex = -1;
	_movieLayer = false;
	if (!e || !_clip.open(e->name))
		return;
	_movieIndex = index;
	_movieSkippable = !noSkip;
	_movieLayer = true;
	debug("Vangogh: page movie %d '%s' at %d,%d %dx%d", index, e->name, e->x, e->y, e->w, e->h);
}

void Page::autosave() {
	if (g_engine && _lastPlaced >= 0)
		g_engine->autosaveAfterPlacement(_lastPlaced);
}

// ---------------------------------------------------------------------------
// The placement sequence: drop -> CapsOP -> movie/voice -> CapsAC -> completion check.

void Page::placeItem(int slot) {
	const int item = kPages[_page].items[slot];
	_st->itemPlaced[item] = 1;
	_st->setItem(item, 0);
	_lastPlaced = item;
	_dragItem = -1;
	playItemSound(item);
	_animSlot = slot;
	startAnim(_anim, &_capsOP[slot], kSlotSprite[slot][0], kSlotSprite[slot][1]);
	setState(kPlaceOpen);
}

void Page::startPlayback() {
	// sub_413550: the star's movie, then the painting with its loupe (page 0 only; every other slot runs a PageGame).
	startMovie(kMovieP0A, true);
	_next = kNextP0Loupe;
	setState(kPlayback);
}

void Page::endPlayback() {
	_clip.close();
	_movieIndex = -1;
	_movieLayer = false;
	const Next next = _next;
	_next = kNextNone;
	switch (next) {
	case kNextP0Loupe:
		loadBackground("a14_031b");
		_loupe = kLoupeVisible;
		setState(kP0Loupe);
		break;
	case kNextP0Sun:
		loadBackground("a14_031c");
		_p0Cell = 1;
		setState(kSunReady);
		break;
	case kNextP0Hold:
		loadBackground("a14_031d");
		_p0Cell = -1;
		setState(kP0Hold);
		break;
	case kNextEndWait:
		loadBackground("a14_032b");
		setState(kEndWait);
		break;
	case kNextEndHold:
		setState(kEndHold);
		break;
	default:
		setState(kIdle);
		break;
	}
}

void Page::afterPlayback() {
	// State 0xf: the capsule closes.
	play("fermcaps");
	if (_animSlot >= 0) {
		startAnim(_anim, &_capsAC[_animSlot], kSlotSprite[_animSlot][0], kSlotSprite[_animSlot][1]);
		setState(kPlaceClose);
	} else {
		finishPlacement();
	}
}

// State 0xe: the slot handler's start hook, then the capsule is drawn open and the handler runs every tick (state 0xd).
void Page::enterGame(bool replay) {
	const int item = kPages[_page].items[_animSlot];
	delete _game;
	_gameSlot = _animSlot;
	_gameDropped = !replay;
	_game = PageGame::create(*this, _page, _gameSlot);
	_game->start(item, _gameSlot);
	_game->drawCapsules(_gameSlot);
	_cursor = kCurBusy;
	setState(kGame);
}

void Page::tickGame(const WorldInput &in) {
	_game->_in = &in;
	if (in.backspace)
		_game->onAbort();
	if (!_game->run(_tick & 3, _gameOk))
		return;
	endGame();
}

// State 0xd, handler done -> state 0xf: the page background comes back, then the capsule closes (or an item the handler
// refused flies back to the bar).
void Page::endGame() {
	delete _game;
	_game = nullptr;
	_movieLayer = false;
	_clip.close();
	_movieIndex = -1;
	loadBackground(kPages[_page].bg);
	_cursor = kCurArrow;
	if (_gameOk || !_gameDropped) {
		afterPlayback();
		return;
	}
	rejectPlacement();
}

// State 0xf, refused item: the placement is undone and the item returns to its place in the bar.
void Page::rejectPlacement() {
	const int item = kPages[_page].items[_gameSlot];
	_st->itemPlaced[item] = 0;
	_st->setItem(item, 1);
	play("cf_clic3");
	_flyItem = item;
	_flyIndex = _dragIndex;
	_flyFrom[0] = kSlotFly[_gameSlot][0];
	_flyFrom[1] = kSlotFly[_gameSlot][1];
	const int k = CLIP(_dragIndex - _barScroll, 0, 5);
	_flyTo[0] = 101 + kBarIconStride * k;
	_flyTo[1] = kBarTop + 30;
	_flyTick = 0;
	_animSlot = -1;
	_anim = Anim();
	_returnState = kIdle;
	setState(kReject);
}

void Page::finishPlacement() {
	_anim = Anim();
	_animSlot = -1;
	const PageDesc &d = kPages[_page];

	if (_loupe == kLoupeHidden) {
		_loupe = kLoupeAppearing;
		startAnim(_loupeAnim, &_loupeOut, d.loupeX, d.loupeY);
	}

	bool all = true;
	for (int s = 0; s < d.numItems; s++)
		all &= _st->itemPlaced[d.items[s]] != 0;
	if (all && !_st->pageDone[_page] && _paA.loaded()) {
		_st->pageDone[_page] = 1;
		play("pas_VG");
		startAnim(_layer, &_paA, 0, 0);
		setState(kComplete);
		return;
	}
	if (_gameDropped)	// state 0xc: dat_52a4a8 is only set for an item that was just placed
		autosave();
	setState(kIdle);
}

void Page::dropSunflower(bool inCounter) {
	if (inCounter) {
		play("vase");
		startAnim(_anim, &_pot, kPotX, kPotY);
		setState(kPot);
		return;
	}
	play("cf_clic3");
	if (_page != 0) {
		_layer.spr = &_paA;
		_layer.cell = _paA.numCells() - 1;
	} else {
		_p0Cell = 1;
	}
	setState(kSunReady);
}

void Page::finishCounter() {
	const PageDesc &d = kPages[_page];
	_st->groupCounter[d.group]++;
	_anim = Anim();
	if (_page == 0) {
		startMovie(kMovieP0C, false);
		_next = kNextP0Hold;
		setState(kPlayback);
		return;
	}
	startAnim(_layer, &_paC, 0, 0);
	if (!_layer.active) {
		autosave();
		setState(kIdle);
		return;
	}
	setState(kAfter);
}

void Page::commitAndExit(int target) {
	const PageDesc &d = kPages[_page];
	// transition_commit: NbTournesols = the group counter, the page is solved if it rose, per-page flags.
	const byte old = _st->getByte(kOfsSunflowers);
	const byte now = (byte)_st->groupCounter[d.group];
	_st->setByte(kOfsSunflowers, now);
	if (old < now)
		_st->setPuzzleSolved(_page, true);
	_st->setByte(kOfsPage, (byte)_page);
	switch (_page) {
	case 0:
		_st->set(kOfsPage0Done, 1);
		_st->set(kOfsRobotOff, 1);
		break;
	case 7:
		_st->set(kCbPillowDoneA, 1);
		_st->set(kCbPillowDoneB, 1);
		break;
	case 8:
		_st->set(kCbShoesEnabled, 1);
		break;
	case 11:
		_st->set(kCbMirrorBroken, 1);
		if (!_st->isSet(kCbMirrorSoundPlayed))
			_st->mirrorSoundPending = true;	// dat_52a77c = 1
		break;
	case 21:
		_st->set(kOfsEnding, 1);
		_ending = true;
		break;
	default:
		break;
	}
	_exit = target;
}

// ---------------------------------------------------------------------------
// Per tick.

void Page::tick(const WorldInput &in) {
	if (!_active || _exit)
		return;
	_tick++;
	_stateTicks++;
	_mx = in.mouseX;
	_my = in.mouseY;
	if (_state != kGame)
		_cursor = kCurBusy;
	if (in.esc)
		_menuRequest = true;

	// Bar slide.
	if (_barState == kBarIn) {
		_barY -= kBarStep;
		if (_barY <= kBarTop) {
			_barY = kBarTop;
			_barState = kBarShown;
		}
	} else if (_barState == kBarOut) {
		_barY += kBarStep;
		if (_barY >= 480) {
			_barY = 480;
			_barState = kBarHidden;
		}
	}

	// The loupe icon appears and disappears on its own.
	if (_loupe == kLoupeAppearing && stepAnim(_loupeAnim))
		_loupe = kLoupeVisible;

	switch (_state) {
	case kIdle:
		tickIdle(in);
		break;
	case kP0Loupe:
		tickIdle(in);
		if (_state == kP0Loupe && _stateTicks >= (uint32)kP0LoupeTicks) {
			// Nobody clicked the loupe: the painting's second movie plays by itself.
			startMovie(kMovieP0B, true);
			_next = kNextP0Sun;
			setState(kPlayback);
		}
		break;
	case kDrag:
		tickDrag(in);
		break;
	case kReject:
		_flyTick++;
		if (_flyTick >= kFlyTicks) {
			insertInBar(_flyItem, _flyIndex);
			_flyItem = -1;
			setState(_returnState);
		}
		break;
	case kPlaceOpen:
		if (stepAnim(_anim)) {
			if (_page == 0)
				startPlayback();
			else
				enterGame(false);
		}
		break;
	case kPlayback:
		// sub_417520: a skippable clip ends on a held left button (flow.md 7.3), otherwise when it has played out.
		if (_movieSkippable && in.mouseDown) {
			endPlayback();
		} else {
			_clip.step();
			if (_clip.done())
				endPlayback();
		}
		break;
	case kGame:
		tickGame(in);
		break;
	case kPlaceClose:
		if (stepAnim(_anim))
			finishPlacement();
		break;
	case kClickOpen:
		if (stepAnim(_anim)) {
			_anim = Anim();
			if (_page == 0) {
				_animSlot = -1;
				setState(kIdle);
			} else {
				enterGame(true);
			}
		}
		break;
	case kComplete:
		if (stepAnim(_layer)) {
			beginSunflower();
		}
		break;
	case kSunReady:
	case kSunDrag:
		tickSunflower(in);
		break;
	case kPot:
		if (stepAnim(_anim))
			finishCounter();
		break;
	case kAfter:
		if (stepAnim(_layer)) {
			autosave();
			setState(kIdle);
		}
		break;
	case kZoom:
		tickScroller(in);
		break;
	case kPaint:
		if (stepAnim(_anim) || in.click)
			leaveView();
		break;
	case kP0Hold:
		if (_stateTicks >= (uint32)kHoldTicks) {
			_st->pageDone[0] = 1;
			commitAndExit(kExitResume);
		}
		break;
	case kEndWait:
		_cursor = kEndClick.has(_mx, _my) ? kCurFinger : kCurArrow;
		if (in.click && kEndClick.has(_mx, _my)) {
			startMovie(kMovieEndB, true);
			_next = kNextEndHold;
			setState(kPlayback);
		}
		break;
	case kEndHold:
		if (_stateTicks >= (uint32)kEndHoldTicks)
			commitAndExit(kExitResume);
		break;
	default:
		break;
	}

	compose();
}

void Page::beginSunflower() {
	_layer.spr = &_paA;
	_layer.cell = _paA.numCells() - 1;
	_layer.active = false;
	setState(kSunReady);
}

int Page::barSlotAt(int x, int y) const {
	if (_barState != kBarShown)
		return -1;
	for (int k = 0; k < 6; k++) {
		const R r = { kBarHitX + kBarIconStride * k, kBarHitY, 28, 27 };
		if (r.has(x, y) && _barScroll + k < _barCount)
			return _barScroll + k;
	}
	return -1;
}

void Page::tickIdle(const WorldInput &in) {
	const PageDesc &d = kPages[_page];
	const bool barHidden = _barState == kBarHidden;
	const bool barShown = _barState == kBarShown;
	// Opening the bar plays LoupeIn (sub_413410): the loupe is gone while the bar is out, so its rectangles (which overlap
	// the right-hand bar slots on the pages with the loupe at x ~330) do not take the clicks.
	const bool loupeUsable = _loupe == kLoupeVisible && barHidden;

	if (in.backspace && _state == kIdle) {
		commitAndExit(kExitResume);
		return;
	}
	if (in.space && _state == kIdle && (barHidden || barShown) && _loupe != kLoupeAppearing) {
		_barState = barShown ? kBarOut : kBarIn;
		play(barShown ? "bar_outi" : "bar_obj");
	}

	// Hit tests.
	const int barIdx = barSlotAt(_mx, _my);
	const bool arrowL = barShown && kArrowLeft.has(_mx, _my);
	const bool arrowR = barShown && kArrowRight.has(_mx, _my);
	int placed = -1;
	for (int s = 0; s < d.numItems; s++) {
		if (_st->itemPlaced[d.items[s]] && kSlotDrop[s].has(_mx, _my))
			placed = s;
	}
	const bool loupeL = loupeUsable && R{ d.loupeX + 2, d.loupeY, 65, 36 }.has(_mx, _my);
	const bool loupeR = loupeUsable && R{ d.loupeX + 0x4e, d.loupeY, 65, 36 }.has(_mx, _my);
	const bool chromeR = barHidden && kChromeRight.has(_mx, _my) && _state == kIdle;
	const bool chromeL = barHidden && kChromeLeft.has(_mx, _my) && _state == kIdle;
	_hoverSlot = _state == kIdle ? placed : -1;
	_retourCell = chromeR ? (_retourCell + 1) % MAX<int>(1, _retour.numCells()) : 0;

	_cursor = kCurArrow;
	if (barIdx >= 0)
		_cursor = kCurGrab;
	else if (arrowL || arrowR || placed >= 0 || loupeL || loupeR || chromeR || chromeL)
		_cursor = kCurFinger;

	if (!in.click)
		return;

	if (loupeL) {
		enterZoom();
	} else if (loupeR) {
		enterPaint();
	} else if (chromeR) {
		commitAndExit(kExitResume);
	} else if (chromeL) {
		commitAndExit(kExitMuseum);
	} else if (arrowL) {
		if (_barScroll > 0)
			_barScroll--;
		play("fleche");
	} else if (arrowR) {
		if (_barCount - _barScroll > 6)
			_barScroll++;
		play("fleche");
	} else if (barIdx >= 0 && _state == kIdle) {
		beginDrag(barIdx, in.mouseDown);
	} else if (placed >= 0 && _state == kIdle) {
		// State 0xb: reopen the capsule and replay what the item did.
		const int item = d.items[placed];
		play("ouvrcaps");
		_animSlot = placed;
		_lastPlaced = item;
		startAnim(_anim, &_capsAO[placed], kSlotSprite[placed][0], kSlotSprite[placed][1]);
		setState(kClickOpen);
	}
}

void Page::beginDrag(int barIndex, bool held) {
	_dragHeld = held;
	_dragItem = _barList[barIndex];
	_dragIndex = barIndex;
	_dragSlot = slotOfItem(_dragItem);
	_dragStart = _tick;
	removeFromBar(barIndex);
	play("bar_obj");
	setState(kDrag);
}

void Page::tickDrag(const WorldInput &in) {
	_cursor = kCurDrag;
	if (_tick == _dragStart)
		return;
	// Dropped on release of the button, or on the next click when the pick-up was a plain click (keyboard).
	if (_dragHeld ? (in.mouseDown && !in.click) : !in.click)
		return;

	const int item = _dragItem;
	if (_dragSlot >= 0 && !_st->itemPlaced[item] && kSlotDrop[_dragSlot].has(_mx, _my)) {
		placeItem(_dragSlot);
		return;
	}
	// Rejected: it flies back to its place in the bar.
	play("cf_clic3");
	_flyItem = item;
	_flyIndex = _dragIndex;
	const int k = CLIP(_dragIndex - _barScroll, 0, 5);
	_flyFrom[0] = _mx;
	_flyFrom[1] = _my;
	_flyTo[0] = 101 + kBarIconStride * k;
	_flyTo[1] = kBarTop + 30;
	_flyTick = 0;
	_dragItem = -1;
	_returnState = kIdle;
	setState(kReject);
}

void Page::tickSunflower(const WorldInput &in) {
	const PageDesc &d = kPages[_page];
	if (_state == kSunReady) {
		const bool over = kSunRect[d.sun].has(_mx, _my);
		_cursor = over ? kCurGrab : kCurArrow;
		if (over && in.click) {
			play("tourneso");
			if (_page != 0) {
				_layer.spr = &_paB;
				_layer.cell = 0;
			} else {
				_p0Cell = 0;
			}
			_dragStart = _tick;
			_dragHeld = in.mouseDown;
			setState(kSunDrag);
		}
		return;
	}
	_cursor = kCurDrag;
	if (_tick == _dragStart || (_dragHeld ? (in.mouseDown && !in.click) : !in.click))
		return;
	dropSunflower(kCounter.has(_mx, _my));
}

// ---------------------------------------------------------------------------
// The two full-screen views behind the loupe.

void Page::enterZoom() {
	const PageDesc &d = kPages[_page];
	if (!loadTgpImage(Common::String(d.zoom) + "b", _big))
		return;
	_scrollX = MAX(0, (_big.w - 640) / 2);
	_scrollY = MAX(0, (_big.h - 480) / 2);
	_kx = _ky = 0;
	_returnState = _state;
	setState(kZoom);
}

void Page::enterPaint() {
	const PageDesc &d = kPages[_page];
	loadBackground(Common::String(d.zoom) + "a");
	play("pas_VG2");
	_returnState = _state;
	startAnim(_anim, &_paint, 0, 0);
	setState(kPaint);
}

void Page::leaveView() {
	_anim = Anim();
	_big.free();
	loadBackground(_returnState == kP0Loupe ? "a14_031b" : kPages[_page].bg);
	setState(_returnState);
}

void Page::tickScroller(const WorldInput &in) {
	// sub_40f1e0: a 64 px band along each edge scrolls at half the distance into it; the arrow keys accelerate to 32.
	int dx = 0, dy = 0;
	const int band = 64;
	int ex = 0, ey = 0;
	if (_mx < band)
		ex = _mx - band;
	else if (_mx >= 640 - band)
		ex = _mx - (640 - band) + 1;
	if (_my < band)
		ey = _my - band;
	else if (_my >= 480 - band)
		ey = _my - (480 - band) + 1;
	dx = ex >> 1;
	dy = ey >> 1;

	if (in.left)
		_kx = MAX(_kx - 1, -32);
	else if (in.right)
		_kx = MIN(_kx + 1, 32);
	else
		_kx = 0;
	if (in.up)
		_ky = MAX(_ky - 1, -32);
	else if (in.down)
		_ky = MIN(_ky + 1, 32);
	else
		_ky = 0;
	dx += _kx;
	dy += _ky;

	_scrollX = CLIP(_scrollX + dx, 0, MAX(0, _big.w - 640));
	_scrollY = CLIP(_scrollY + dy, 0, MAX(0, _big.h - 480));

	// Edge cursors 1..8 by the sign of the band offsets.
	const int sx = ex < 0 ? -1 : (ex > 0 ? 1 : 0), sy = ey < 0 ? -1 : (ey > 0 ? 1 : 0);
	static const int kEdgeCursor[3][3] = { { 5, 3, 6 }, { 1, 0, 2 }, { 7, 4, 8 } };
	_cursor = kEdgeCursor[sy + 1][sx + 1];

	if (in.click)
		leaveView();
}

// ---------------------------------------------------------------------------
// Composition.

void Page::drawCapsules() {
	const PageDesc &d = kPages[_page];
	for (int s = 0; s < d.numItems; s++) {
		const int x = kSlotSprite[s][0], y = kSlotSprite[s][1];
		if (_st->itemPlaced[d.items[s]])
			(s == _hoverSlot ? _capsAC[s] : _capsAO[s]).blit(_fb, 0, x, y);
		else
			_capsE[s].blit(_fb, 0, x, y);
	}
}

// sub_4130f0: the page's capsules drawn into the back buffer, @p openSlot's capsule open.
void Page::bakeCapsules(int openSlot) {
	const PageDesc &d = kPages[_page];
	for (int s = 0; s < d.numItems; s++) {
		const int x = kSlotSprite[s][0], y = kSlotSprite[s][1];
		if (!_st->itemPlaced[d.items[s]])
			_capsE[s].blit(_bg, 0, x, y);
		else
			(s == openSlot ? _capsAC[s] : _capsAO[s]).blit(_bg, 0, x, y);
	}
}

void Page::drawBar() {
	if (_barState == kBarHidden)
		return;
	const PageDesc &d = kPages[_page];
	_invent.blit(_fb, 0, 0, _barY);
	for (int k = 0; k < 6 && _barScroll + k < _barCount; k++)
		_opi.blit(_fb, _barList[_barScroll + k], kBarIconX + kBarIconStride * k, _barY);
	if (_barState == kBarShown) {
		_invent.blit(_fb, kArrowLeft.has(_mx, _my) ? 3 : 1, 13, _barY);
		_invent.blit(_fb, kArrowRight.has(_mx, _my) ? 4 : 2, 504, _barY);
	}
	const int count = _st->groupCounter[d.group];
	if (count > 0)
		_count.blit(_fb, MIN(count, 15), kBadgeX, _barY + kBadgeY);
}

void Page::compose() {
	const PageDesc &d = kPages[_page];

	if (_state == kZoom) {
		blitRect(_fb, _big, _scrollX, _scrollY, 640, 480, 0, 0);
		return;
	}

	if (_state == kGame) {
		// The handler owns the picture: its back buffer, the last movie frame, the sprites it keeps over it, the dragged one.
		blitSurface(_fb, _bg, 0, 0);
		if (_movieLayer && _movieIndex >= 0 && _clip.frame() >= 0) {
			const MovieEntry *e = movieByIndex(_movieIndex);
			if (e)
				copyRect(_fb, _clip.surface(), e->x, e->y, e->w, e->h);
		}
		_game->drawOverlays(_fb);
		if (_loupe != kLoupeHidden && !_game->bgReloaded())
			_loupeOut.blit(_fb, _loupe == kLoupeAppearing ? _loupeAnim.cell : _loupeOut.numCells() - 1, d.loupeX, d.loupeY);
		return;
	}

	blitSurface(_fb, _bg, 0, 0);
	if (_state == kPaint) {
		if (_anim.spr)
			_anim.spr->blit(_fb, _anim.cell, 0, 0);
		return;
	}

	if (_layer.spr)
		_layer.spr->blit(_fb, _layer.cell, 0, 0);
	if (_p0Cell >= 0 && _p0.loaded())
		_p0.blit(_fb, _p0Cell, 31, 198);

	if (_page != 21)
		drawCapsules();

	if (_state == kPlayback && _movieIndex >= 0 && _clip.frame() >= 0) {
		const MovieEntry *e = movieByIndex(_movieIndex);
		if (e)
			copyRect(_fb, _clip.surface(), e->x, e->y, e->w, e->h);
	}

	if (_anim.spr)
		_anim.spr->blit(_fb, _anim.cell, _anim.x, _anim.y);

	// Loupe icon.
	if (_loupe == kLoupeAppearing)
		_loupeOut.blit(_fb, _loupeAnim.cell, d.loupeX, d.loupeY);
	else if (_loupe == kLoupeVisible)
		_loupeOut.blit(_fb, _loupeOut.numCells() - 1, d.loupeX, d.loupeY);

	drawBar();

	// Chrome icons are only there with the bar away and nothing going on.
	if (_barState == kBarHidden && (_state == kIdle || _state == kDrag || _state == kReject)) {
		_retour.blit(_fb, _retourCell, kRetourRightX, kRetourRightY);
		_retourM.blit(_fb, 0, kRetourLeftX, kRetourLeftY);
	}

	// Things on the pointer.
	if (_state == kDrag && _dragItem >= 0)
		_op.blit(_fb, _dragItem, _mx, _my);
	else if (_state == kReject && _flyItem >= 0) {
		const int x = _flyFrom[0] + (_flyTo[0] - _flyFrom[0]) * _flyTick / kFlyTicks;
		const int y = _flyFrom[1] + (_flyTo[1] - _flyFrom[1]) * _flyTick / kFlyTicks;
		_op.blit(_fb, _flyItem, x, y);
	} else if (_state == kSunDrag)
		_tourn.blit(_fb, 0, _mx, _my);
}

} // End of namespace Vangogh
