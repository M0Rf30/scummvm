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

#include "common/array.h"
#include "common/str.h"

#include "vangogh/page.h"
#include "vangogh/pagegames.h"

namespace Vangogh {

namespace {

// ---------------------------------------------------------------------------
// Page 18 slot 0, item 28 (A03_06): hook12 sub_407260, hook13 sub_407380, run sub_407400.
// Four speakers sprites a03_06b..e cycle; a click on the highlighted one plays its answer, the third one (index 2)
// ends the game.

class A03_06Game : public PageGame {
public:
	explicit A03_06Game(Page &page) : PageGame(page), _slot(0), _state(0), _next(0), _cur(0), _sfx(0), _abort(false),
		_wait(0) {}

	void start(int item, int slot) override;
	void onAbort() override { _abort = true; }	// sub_407380
	bool run(uint tick, bool &placed) override;

private:
	void freeSprites();			// sub_407330
	bool hit(int idx) const;	// sub_4073a0

	int _slot, _state, _next, _cur;
	uint16 _sfx;
	bool _abort;
	int _wait;
	PgSprite _spr[4];
};

void A03_06Game::start(int item, int slot) {
	static const char *const kNames[4] = { "a03_06b", "a03_06c", "a03_06d", "a03_06e" };
	for (int i = 0; i < 4; i++)
		loadSprite(_spr[i], kNames[i], 1);
	startMovie(0x1b, 1);
	_cur = 0;
	_slot = slot;
	_state = 0;
	_wait = 0;
	_abort = false;
}

void A03_06Game::freeSprites() {
	for (int i = 0; i < 4; i++)
		erase(_spr[i]);
}

bool A03_06Game::hit(int idx) const {
	// Cell windows (0x4f3138) in which the speaker answers, and the single hot rectangle (0x4f3158).
	static const int kRange[4][2] = { { 0x17, 0x1f }, { 0x23, 0x29 }, { 0x24, 0x2c }, { 0x13, 0x19 } };
	static const PgRect kHot = { 0x125, 0x85, 0x7c, 0x23 };
	const int cell = _spr[idx].cell;
	if (kRange[idx][0] <= cell && cell <= kRange[idx][1])
		return kHot.has(mouseX(), mouseY());
	return false;
}

bool A03_06Game::run(uint tick, bool &placed) {
	setCursor(0);
	switch (_state) {
	case 0:
		if (movieStep()) {
			loadBg("a03_06f");
			drawCapsules(_slot);
			stream("a03_06b");
			_state = 1;
		}
		break;
	case 1:
		if (voiceStep()) {
			_state = 2;
			_next = 3;
		}
		break;
	case 2:
		_sfx = loadSfx(Common::String::format("a03_06%c", _cur + 0x69).c_str());
		animStart(_spr[_cur], 0, 0, 1);
		playSfx(_sfx);
		_state = _next;
		break;
	case 3:
		if (_abort) {
			stopSfx(_sfx);
			stopStream();
			freeSprites();
			placed = false;
			return true;
		}
		if ((tick & 1) && !animStep(_spr[_cur], 0, 0)) {
			stopSfx(_sfx);
			_cur = (_cur + 1) & 3;
			_state = 2;
			_next = 3;
		} else {
			const bool over = hit(_cur);
			if (over)
				setCursor(11);
			if (mouseHeld() && over) {
				if (_cur != 2) {
					stream(Common::String::format("a03_06%c", _cur + 99).c_str());
					_state = 4;
				} else {
					stopSfx(_sfx);
					loadBg("a03_06");
					drawCapsules(_slot);
					_state = 5;
				}
			}
		}
		break;
	case 4: {
		const bool over = hit(_cur);
		if (over)
			setCursor(11);
		if ((tick & 1) && !animStep(_spr[_cur], 0, 0)) {
			stopSfx(_sfx);
			_cur = (_cur + 1) & 3;
			_state = 2;
			_next = 4;
		} else if (!mouseHeld()) {
			_state = 3;
		}
		break;
	}
	case 5:
		if (!mouseHeld()) {
			stream("a03_06e");
			_state = 6;
		}
		break;
	case 6:
		if (voiceStep()) {
			stream("a03_06g");
			_state = 7;
		}
		break;
	case 7:
		if (voiceStep()) {
			stream("a03_06h");
			_state = 8;
		}
		break;
	case 8:
		if (voiceStep()) {
			_wait = 0;
			_state = 9;
		}
		break;
	case 9:
		setCursor(9);
		_wait++;
		if (_wait == 0x3c) {
			freeSprites();
			placed = true;
			return true;
		}
		break;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Page 20 slot 0, item 31 (A14_01): hook12 sub_421b90, hook13 sub_421dc0, run sub_421e50.
// Three rows of three-state pictures (rows at 0x4f8de0); clicking a row advances it; solved when every row shows
// state 0 (the exe compares with the zero-initialised table at 0x52a820).

class A14_01Game : public PageGame {
public:
	explicit A14_01Game(Page &page) : PageGame(page), _slot(0), _state(0), _next(0), _row(0), _wait(0), _abort(false),
		_clickSfx(0), _winSfx(0) {
		_pos[0] = 2;
		_pos[1] = 2;
		_pos[2] = 1;
	}

	void start(int item, int slot) override;
	void onAbort() override { _abort = true; }	// sub_421dc0
	bool run(uint tick, bool &placed) override;

private:
	void cleanup();				// sub_421cf0
	int rowAt(int x, int y) const;	// sub_421de0
	static const char *name(int row, int col);

	int _slot, _state, _next, _row, _wait;
	bool _abort;
	uint16 _clickSfx, _winSfx;
	int _pos[3];
	PgSprite _s[3][3];
};

const char *A14_01Game::name(int row, int col) {
	// s_4f8e10: three names per row.
	static const char *const kNames[3][3] = {
		{ "a14_01h", "a14_01i", "a14_01j" },
		{ "a14_01b", "a14_01c", "a14_01d" },
		{ "a14_01e", "a14_01f", "a14_01g" }
	};
	return kNames[row][col];
}

void A14_01Game::start(int item, int slot) {
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 3; c++)
			_s[r][c].spr.clear();
	loadSprite(_s[0][2], name(0, 2), 0);
	loadSprite(_s[1][0], name(1, 0), 0);
	loadSprite(_s[1][1], name(1, 1), 0);
	loadSprite(_s[1][2], name(1, 2), 0);
	loadSprite(_s[2][1], name(2, 1), 0);
	_clickSfx = loadSfx("a14_01e");
	_winSfx = loadSfx("reussit");
	_pos[0] = 2;
	_pos[1] = 2;
	_pos[2] = 1;
	_slot = slot;
	_state = 0;
	_wait = 0;
	_abort = false;
	startMovie(0x36, 1);
}

void A14_01Game::cleanup() {
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			if (_s[r][c].loaded()) {
				erase(_s[r][c]);
				_s[r][c].spr.clear();
			}
		}
	}
}

int A14_01Game::rowAt(int x, int y) const {
	static const PgRect kRows[3] = { { 0x99, 0x1d, 0x14f, 0x8c }, { 0x99, 0xaa, 0x14f, 0xb9 }, { 0x99, 0x164, 0x14f, 0x49 } };
	for (int i = 0; i < 3; i++)
		if (kRows[i].has(x, y))
			return i;
	return -1;
}

bool A14_01Game::run(uint tick, bool &placed) {
	static const int kRowX[3] = { 0x99, 0x99, 0x99 };
	static const int kRowY[3] = { 0x1d, 0xaa, 0x164 };

	setCursor(0);
	switch (_state) {
	case 0:
		if (movieStep()) {
			loadBg("a14_010a");
			drawCapsules(_slot);
			stream("a14_01b");
			_state = 1;
		}
		break;
	case 1:
		if (voiceStep()) {
			stopStream();
			_state = 2;
		}
		break;
	case 2: {
		if (_abort) {
			cleanup();
			placed = false;
			return true;
		}
		const int r = rowAt(mouseX(), mouseY());
		setCursor(r >= 0 ? 11 : 0);
		if (mouseHeld() && r >= 0) {
			animStart(_s[r][_pos[r]], kRowX[r], kRowY[r], 1);
			playSfx(_clickSfx);
			setCursor(9);
			_row = r;
			_state = 3;
		}
		break;
	}
	case 3:
		setCursor(9);
		if ((tick & 1) && !animStep(_s[_row][_pos[_row]], kRowX[_row], kRowY[_row])) {
			const int old = _pos[_row];
			_pos[_row] = (_pos[_row] + 1) % 3;
			if (_pos[0] != 0 || _pos[1] != 0 || _pos[2] != 0) {
				_next = 2;
				if (!_s[_row][_pos[_row]].loaded()) {
					erase(_s[_row][old]);
					_s[_row][old].spr.clear();
					loadSprite(_s[_row][_pos[_row]], name(_row, _pos[_row]), 0);
				}
			} else {
				playSfx(_winSfx);
				_wait = 0;
				_next = 5;
			}
			_state = 4;
		}
		break;
	case 4:
		setCursor(9);
		if (!mouseHeld())
			_state = _next;
		break;
	case 5:
		setCursor(9);
		_wait++;
		if (_wait >= 0x33) {
			setCursor(9);
			loadBg("a14_01");
			drawCapsules(_slot);
			stream("a14_01c");
			_state = 6;
		}
		break;
	case 6:
		if (voiceStep()) {
			setCursor(9);
			stopStream();
			_state = 7;
		}
		break;
	case 7:
		_wait++;
		setCursor(9);
		if (_wait == 0x3c) {
			cleanup();
			placed = true;
			return true;
		}
		break;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Page 24 slot 0, item 34 (A14_02): hook12 sub_422470, hook13 sub_422720, run sub_422bb0.
// First a scratch-card: the cursor erases the a14_02 picture to the a14_020a one underneath (mask buffer of 479x387
// bytes), done at 80% scratched. Then five pieces (a14_021a cells, tray at 0x4f8e58) are dragged onto their places
// (rects 0x4f8e68) where they are drawn with a14_021b.

const int kMaskW = 0x1df;
const int kMaskSize = 0x2d420;

class A14_02Game : public PageGame {
public:
	explicit A14_02Game(Page &page) : PageGame(page), _slot(0), _state(0), _next(0), _wait(0), _abort(false),
		_scratching(false), _lastX(0), _lastY(0), _cw(0), _ch(0), _piece(0), _scratchSfx(0), _winSfx(0), _sfx2(0),
		_sfx3(0), _sfx4(0), _sfxBad(0) {
		for (int i = 0; i < 5; i++)
			_done[i] = false;
	}

	void start(int item, int slot) override;
	void onAbort() override { _abort = true; }	// sub_422720
	bool run(uint tick, bool &placed) override;

private:
	void cleanup();							// sub_422660
	int pick(int x, int y) const;			// sub_422740
	int target(int x, int y) const;			// sub_4227c0
	bool scratch(int x, int y);				// sub_422830
	void drawPiece(int i);					// sub_422a80
	void drawPlaced(int i);					// sub_422b20
	void stopScratchSfx();
	static bool inBoard(int x, int y) { return x >= 0x70 && x <= 0x24e && y >= 0x1d && y <= 0x19f; }

	int _slot, _state, _next, _wait;
	bool _abort;
	bool _scratching;
	int _lastX, _lastY;
	int _cw, _ch;		// dat_52a794 / dat_52a790: size of cursor kind 10
	int _piece;
	uint16 _scratchSfx, _winSfx, _sfx2, _sfx3, _sfx4, _sfxBad;
	bool _done[5];
	Common::Array<byte> _mask;
	UISprite _cursor;
	PgSprite _pieces[5];
	PgSprite _placed;
};

// Tray rects (x, y, w, h) at 0x4f8e58, stride 0x20: x 569, y 15 + 80 * i, 65x65.
// Target rects at 0x4f8e68, stride 0x20.
static const PgRect kTray[5] = {
	{ 569, 15, 65, 65 }, { 569, 95, 65, 65 }, { 569, 175, 65, 65 }, { 569, 255, 65, 65 }, { 569, 335, 65, 65 }
};
static const PgRect kTarget[5] = {
	{ 460, 356, 65, 65 }, { 125, 169, 65, 65 }, { 469, 234, 65, 65 }, { 114, 32, 65, 65 }, { 147, 326, 65, 65 }
};
// Where a placed piece is drawn (0x4f8ef8).
static const int kPlaced[5][2] = { { 475, 365 }, { 181, 211 }, { 492, 258 }, { 183, 92 }, { 184, 347 } };

void A14_02Game::start(int item, int slot) {
	_mask.resize(kMaskSize);
	memset(_mask.data(), 0, kMaskSize);
	for (int i = 0; i < 3; i++)
		_mask[0x2d41d + i] = 0xff;
	for (int i = 0; i < 5; i++) {
		loadSprite(_pieces[i], "a14_021a", 2);
		_done[i] = false;
	}
	loadSprite(_placed, "a14_021b", 0);
	// sub_42d450(10, ...): the size of cursor cell 10.
	_cw = _ch = 0;
	if (_cursor.loadSpr("curseurs")) {
		_cw = _cursor.width(10);
		_ch = _cursor.height(10);
	}
	_slot = slot;
	_state = 0;
	_wait = 0;
	_abort = false;
	_scratching = false;
	_lastX = _lastY = 0;
	_winSfx = loadSfx("reussit");
	_sfx2 = loadSfx("p_clic2");
	_sfx3 = loadSfx("p_clic3");
	_sfx4 = loadSfx("p_clic4");
	_scratchSfx = loadSfx("p_clic1");
	_sfxBad = loadSfx("cf_clic3");
	startMovie(0x1c, 0);
}

void A14_02Game::cleanup() {
	dragEnd();
	for (int i = 0; i < 5; i++)
		erase(_pieces[i]);
	erase(_placed);
	stopStream();
}

int A14_02Game::pick(int x, int y) const {
	for (int i = 0; i < 5; i++)
		if (kTray[i].has(x, y) && !_done[i])
			return i;
	return -1;
}

int A14_02Game::target(int x, int y) const {
	for (int i = 0; i < 5; i++)
		if (kTarget[i].has(x, y))
			return i;
	return -1;
}

bool A14_02Game::scratch(int x, int y) {
	int x0 = x - (_cw >> 1);
	int y0 = y - (_ch >> 1);
	int w = _cw;
	int h = _ch;
	if (x0 <= 0x6f) {
		w -= 0x70 - x0;
		x0 = 0x70;
	}
	if (y0 <= 0x1c) {
		h -= 0x1d - y0;
		y0 = 0x1d;
	}
	if (0x250 <= x0 + w) {
		w = 0x24f - x0;
		if (w <= 0)
			return false;
	}
	if (0x1a1 <= y0 + h) {
		h = 0x1a0 - y0;
		if (h <= 0)
			return false;
	}
	if (w > 0 && h > 0) {
		// The pixels under the cursor come back from the snapshot of a14_020a.
		restoreFromSnapshot(x0, y0, w, h);
		for (int row = y0 - 0x1d; row < y0 - 0x1d + h; row++)
			memset(_mask.data() + row * kMaskW + x0 - 0x70, 0xff, w);
	}
	int untouched = 0;
	for (int i = 0; i <= 0xb507; i++) {
		const byte *d = _mask.data() + i * 4;
		if (!(d[0] == 0xff && d[1] == 0xff && d[2] == 0xff && d[3] == 0xff))
			untouched++;
	}
	return untouched < 0x2423;
}

void A14_02Game::drawPiece(int i) {
	saveDraw(_pieces[i], i, kTray[i].x, kTray[i].y);
}

void A14_02Game::drawPlaced(int i) {
	saveDraw(_placed, i, kPlaced[i][0], kPlaced[i][1]);
}

void A14_02Game::stopScratchSfx() {
	if (_scratching) {
		stopSfx(_scratchSfx);
		_scratching = false;
	}
}

bool A14_02Game::run(uint tick, bool &placed) {
	setCursor(0);
	switch (_state) {
	case 0:
		if (movieStep()) {
			loadBg("a14_020a");
			snapshot();
			loadBg("a14_02");
			drawCapsules(_slot);
			_state = 2;
			_next = 1;
		}
		break;
	case 1: {
		if (_abort) {
			cleanup();
			placed = false;
			return true;
		}
		const int mx = mouseX();
		const int my = mouseY();
		const bool in = inBoard(mx, my);
		setCursor(in ? 10 : 0);
		if (mouseHeld()) {
			if (in) {
				if (mx != _lastX || my != _lastY) {
					if (!_scratching) {
						playSfx(_scratchSfx, true);
						_scratching = true;
					}
				} else if (_scratching) {
					stopSfx(_scratchSfx);
					_scratching = false;
				}
				_lastX = mx;
				_lastY = my;
				if (scratch(mx, my)) {
					_state = 2;
					_next = 3;
					stopScratchSfx();
					playSfx(_sfx2);
				}
			} else {
				stopScratchSfx();
			}
		} else {
			stopScratchSfx();
		}
		break;
	}
	case 2:
		if (!mouseHeld())
			_state = _next;
		break;
	case 3:
		startMovie(0x39, 1);
		_state = 4;
		break;
	case 4:
		if (movieStep()) {
			loadBg("a14_020a");
			drawCapsules(_slot);
			for (int i = 0; i < 5; i++)
				drawPiece(i);
			stream("a14_02c");
			_state = 5;
		}
		break;
	case 5:
		if (voiceStep()) {
			stopStream();
			_wait = 0x7d;
			_state = 6;
		}
		break;
	case 6: {
		if (_abort) {
			cleanup();
			placed = false;
			return true;
		}
		const int mx = mouseX();
		const int my = mouseY();
		const int p = pick(mx, my);
		setCursor(p >= 0 ? 12 : 0);
		if (mouseHeld() && p >= 0) {
			_piece = p;
			erase(_pieces[p]);
			dragBegin(mx, my, kTray[p].x, kTray[p].y, _pieces[p], p);
			_state = 7;
			setCursor(13);
			playSfx(_sfx3);
		}
		_wait++;
		if (_wait >= 0x1f5) {
			stream("a14_02d");
			_wait = 0;
		}
		break;
	}
	case 7:
		setCursor(13);
		if (!mouseHeld()) {
			dragEnd();
			int dx, dy;
			dragPos(dx, dy);
			dx += 0x20;
			dy += 0x20;
			setCursor(0);
			if (target(dx, dy) != _piece) {
				drawPiece(_piece);
				playSfx(_sfxBad);
				_wait = 0;
				_state = 6;
			} else {
				playSfx(_sfx4);
				drawPlaced(_piece);
				_done[_piece] = true;
				int i = 0;
				while (i <= 4 && _done[i])
					i++;
				_state = (i != 5) ? 6 : 8;
				_wait = 0x7d;
			}
		}
		break;
	case 8:
		setCursor(9);
		_wait++;
		if (_wait >= 0x1a) {
			playSfx(_winSfx);
			_wait = 0;
			_state = 9;
		}
		break;
	case 9:
		_wait++;
		setCursor(9);
		if (_wait == 0x3c) {
			cleanup();
			placed = true;
			return true;
		}
		break;
	}
	return false;
}

} // End of anonymous namespace

PageGame *createA03_06Game(Page &page) {
	return new A03_06Game(page);
}

PageGame *createA14_01Game(Page &page) {
	return new A14_01Game(page);
}

PageGame *createA14_02Game(Page &page) {
	return new A14_02Game(page);
}

} // End of namespace Vangogh
