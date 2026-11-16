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
#include "vangogh/pagegames.h"

namespace Vangogh {

namespace {

// ---------------------------------------------------------------------------
// Page 13, slot 0, item 21 (A03_042): hook12 sub_404c40, hook13 sub_404d40, run sub_404f50.
// Three pieces in a column are dragged onto a target; each plays its own clip (0x11 + piece), piece 1 is the right one.

class A03_042Game : public PageGame {
public:
	explicit A03_042Game(Page &page) : PageGame(page), _slot(0), _state(0), _count(0), _abort(false), _sel(0) {
		for (int i = 0; i < 3; i++)
			_present[i] = true;
	}

	void start(int item, int slot) override {
		_slot = slot;
		loadSprite(_drag, "a03_042a", 1);
		loadSprite(_pieces, "a03_042b", 2);
		_sfxWin = loadSfx("reussit");
		_sfxDrop = loadSfx("ha_clic2");
		_sfxPick = loadSfx("ha_clic1");
		startMovie(0x10, 1);
		_state = 0;
		_count = 0;
		_abort = false;
	}

	void onAbort() override { _abort = true; }

	bool run(uint tick, bool &placed) override;

private:
	static const int kPosX[3], kPosY[3], kGrabX[3], kGrabY[3];
	static const PgRect kPick[3];
	static const PgRect kTarget;

	/** sub_404e00 for every piece still on the shelf (the exe saves/restores each cell's background). */
	void draw() {
		restoreFromSnapshot(0, 0, 640, 480);
		for (int i = 0; i < 3; i++) {
			if (_present[i])
				blitCell(_pieces, i, kPosX[i], kPosY[i]);
		}
	}

	/** The page background was reloaded: remember it, all pieces back on the shelf (404e00 x3). */
	void resetShelf() {
		snapshot();
		for (int i = 0; i < 3; i++)
			_present[i] = true;
		draw();
	}

	void finish() { stopStream(); }	// sub_404d70 (sprites and sounds are freed with the handler)

	int _slot;
	int _state, _count;
	bool _abort;
	int _sel;
	bool _present[3];
	PgSprite _drag, _pieces;
	uint16 _sfxWin, _sfxDrop, _sfxPick;
};

const int A03_042Game::kPosX[3] = { 135, 135, 135 };
const int A03_042Game::kPosY[3] = { 76, 185, 296 };
const int A03_042Game::kGrabX[3] = { 168, 169, 169 };
const int A03_042Game::kGrabY[3] = { 109, 216, 327 };
const PgRect A03_042Game::kPick[3] = { { 143, 85, 55, 52 }, { 143, 192, 55, 52 }, { 143, 302, 55, 52 } };
const PgRect A03_042Game::kTarget = { 379, 109, 48, 52 };

bool A03_042Game::run(uint tick, bool &placed) {
	setCursor(0);
	switch (_state) {
	case 0:
		if (!movieStep())
			return false;
		loadBg("a03_042e");
		drawCapsules(_slot);
		resetShelf();
		stream("A03_042");
		_state = 1;
		return false;
	case 1:
		if (!voiceStep())
			return false;
		_count = 0x177;
		_state = 2;
		return false;
	case 2: {
		if (_abort) {
			finish();
			placed = false;
			return true;
		}
		int hit = -1;
		for (int i = 0; i < 3 && hit < 0; i++) {
			if (kPick[i].has(mouseX(), mouseY()))
				hit = i;
		}
		setCursor(hit >= 0 ? 12 : 0);
		if (mouseHeld() && hit >= 0) {
			_present[hit] = false;
			draw();
			dragBegin(mouseX(), mouseY(), kGrabX[hit], kGrabY[hit], _drag, hit);
			_state = 3;
			_sel = hit;
			setCursor(13);
			playSfx(_sfxPick);
		}
		if (_count + 1 <= 0x271) {
			_count++;
			return false;
		}
		_count = 0;
		stream("a03_042b");
		return false;
	}
	case 3: {
		if (mouseHeld())
			return false;
		dragEnd();
		int x, y;
		dragPos(x, y);
		setCursor(9);
		if (kTarget.has(x, y)) {
			playSfx(_sfxDrop);
			startMovie(_sel + 0x11, 1);
			_state = 4;
			return false;
		}
		_present[_sel] = true;
		draw();
		_count = 0;
		_state = 2;
		return false;
	}
	case 4:
		if (!movieStep())
			return false;
		if (_sel == 1) {
			_state = 5;
			playSfx(_sfxWin);
			loadBg("A03_04");
			drawCapsules(_slot);
			stream("A03_042c");
			return false;
		}
		loadBg("a03_042e");
		drawCapsules(_slot);
		resetShelf();
		_count = 0;
		_state = 2;
		return false;
	case 5:
		if (!voiceStep())
			return false;
		_count = 0;
		_state = 6;
		return false;
	case 6:
		setCursor(9);
		_count++;
		if (_count != 0x32)
			return false;
		finish();
		placed = true;
		return true;
	default:
		return false;
	}
}

// ---------------------------------------------------------------------------
// Page 15, slot 0, item 24 (A13_061): hook12 sub_405820, hook13 sub_405990, run sub_405a50.
// A 16 piece jigsaw: pieces come off a stack on the right (in the order of the exe's table), dropped within 4 px of
// their place they lock (flag 1), anywhere else inside the frame they stay loose (flag 0) and can be picked up again.

class A13_061Game : public PageGame {
public:
	explicit A13_061Game(Page &page) : PageGame(page), _state(0), _count(0), _abort(false), _slot(0), _top(15),
		_placedCount(0), _drag(-1) {
		for (int i = 0; i < 16; i++) {
			_flag[i] = -1;
			_listPiece[i] = -1;
			_listX[i] = _listY[i] = 0;
		}
	}

	void start(int item, int slot) override {
		_slot = slot;
		loadSprite(_spr, "A13_061", 2);
		_top = 15;
		for (int i = 0; i < 16; i++)
			_flag[kOrder[i]] = -1;
		_placedCount = 0;
		_sfxWin = loadSfx("reussit");
		_sfxPick = loadSfx("pa_clic1");
		_sfxDrop = loadSfx("pa_clic2");
		_sfxBad = loadSfx("pa_clic3");
		stream("a13_061");
		_state = 0;
		_count = 0;
		_abort = false;
	}

	void onAbort() override { _abort = true; }

	bool run(uint tick, bool &placed) override;

private:
	struct Piece { int sx, sy, tx, ty; };	// stock position and correct position (table 0x4f2d98)
	static const Piece kPiece[16];
	static const int kOrder[16];			// 0x4f2e98: piece id of stock entry n

	/** sub_405490 / sub_40a7e0 loops: everything but @p skip drawn over the saved background. */
	void draw(int skip) {
		restoreFromSnapshot(0, 0, 640, 480);
		for (int i = 0; i <= _top; i++) {
			const int p = kOrder[i];
			if (p != skip)
				blitCell(_spr, p, kPiece[p].sx, kPiece[p].sy);
		}
		for (int i = 0; i < _placedCount; i++) {
			if (_listPiece[i] != skip)
				blitCell(_spr, _listPiece[i], _listX[i], _listY[i]);
		}
	}

	/** sub_405560: the pointer is over the top stock piece. */
	bool hitStock(int x, int y) const {
		if (_top < 0)
			return false;
		const int p = kOrder[_top];
		const int w = _spr.width(p), h = _spr.height(p);
		const int px = kPiece[p].sx - (w >> 1), py = kPiece[p].sy - (h >> 1);
		return px <= x && x < px + w && py <= y && y < py + h;
	}

	/** sub_405630: topmost loose or locked piece under the pointer, or -1. */
	int hitPlaced(int x, int y) const {
		for (int i = _placedCount - 1; i >= 0; i--) {
			const int p = _listPiece[i];
			const int w = _spr.width(p), h = _spr.height(p);
			const int px = _listX[i] - (w >> 1), py = _listY[i] - (h >> 1);
			if (!(x < px || px + w <= x || y < py || py + h <= y))
				return p;
		}
		return -1;
	}

	/** sub_405710: 1 = within 4 px of the place, 0 = inside the frame, -1 = outside. */
	int dropTest(int x, int y) const {
		const int tx = kPiece[_drag].tx, ty = kPiece[_drag].ty;
		if (tx - 4 <= x && x < tx + 4 && ty - 4 <= y && y < ty + 4)
			return 1;
		x -= _spr.width(_drag) >> 1;
		y -= _spr.height(_drag) >> 1;
		if (0x9c <= x && x + _spr.width(_drag) <= 0x1e3 && 0x1d <= y && y + _spr.height(_drag) <= 0x1ac)
			return 0;
		return -1;
	}

	bool allLocked() const {
		for (int i = 0; i < 16; i++) {
			if (_flag[i] != 1)
				return false;
		}
		return true;
	}

	/** Drop in state 4 (stock piece) or 5 (placed piece): @p r from dropTest(); the exe's two copies differ in the list. */
	void drop(int r, int x, int y, bool fromList);

	void finish() { stopStream(); }	// sub_4059c0

	int _state, _count;
	bool _abort;
	int _slot;
	int _top;			// dat_5279a4: topmost stock entry, -1 once the stack is empty
	int _placedCount;	// dat_527ae8
	int _drag;			// dat_527af0
	int _flag[16];		// dat_527a64
	int _listPiece[16];	// dat_527b5c
	int _listX[16], _listY[16];	// dat_5279e0/e4
	PgSprite _spr;
	uint16 _sfxWin, _sfxPick, _sfxDrop, _sfxBad;
};

const A13_061Game::Piece A13_061Game::kPiece[16] = {
	{ 575, 348, 205, 87 }, { 576, 329, 311, 71 }, { 577, 157, 201, 330 }, { 577, 118, 340, 369 },
	{ 577, 278, 425, 96 }, { 577, 262, 214, 207 }, { 577, 212, 263, 141 }, { 577, 232, 336, 158 },
	{ 580, 288, 425, 152 }, { 577, 361, 429, 325 }, { 576, 355, 253, 384 }, { 588, 184, 413, 243 },
	{ 567, 345, 188, 384 }, { 577, 164, 296, 302 }, { 577, 170, 289, 244 }, { 577, 137, 429, 378 }
};
const int A13_061Game::kOrder[16] = { 15, 3, 13, 12, 2, 14, 11, 9, 10, 7, 8, 6, 5, 4, 1, 0 };

void A13_061Game::drop(int r, int x, int y, bool fromList) {
	if (r == -1) {
		// Back to where it came from (sub_40aa10 + sub_40a8f0 on the stack, or the loose pieces redrawn).
		draw(-1);
		_state = 3;
		playSfx(_sfxBad);
		return;
	}
	if (fromList) {
		int n = 0;
		for (int i = 0; i < _placedCount; i++) {
			if (_listPiece[i] != _drag) {
				_listPiece[n] = _listPiece[i];
				_listX[n] = _listX[i];
				_listY[n] = _listY[i];
				n++;
			}
		}
		_placedCount = n;
	}
	if (r == 1) {
		playSfx(_sfxDrop);
		x = kPiece[_drag].tx;
		y = kPiece[_drag].ty;
	}
	_listPiece[_placedCount] = _drag;
	_listX[_placedCount] = x;
	_listY[_placedCount] = y;
	_placedCount++;
	_flag[_drag] = r;
	if (!fromList)
		_top--;
	draw(-1);
	if (r == 0)
		playSfx(_sfxBad);
	if (r == 1 && _top <= -1 && allLocked()) {
		_count = 0;
		_state = 6;
		playSfx(_sfxWin);
		return;
	}
	_state = 3;
}

bool A13_061Game::run(uint tick, bool &placed) {
	setCursor(0);
	switch (_state) {
	case 0:
		if (voiceStep()) {
			stopStream();
			startMovie(0x37, 1);
			_state = 1;
		}
		break;
	case 1:
		if (movieStep()) {
			startMovie(0x38, 1);
			_state = 2;
		}
		break;
	case 2:
		if (movieStep()) {
			loadBg("a13_061a");
			drawCapsules(_slot);
			snapshot();
			draw(-1);
			_state = 3;
		}
		break;
	case 3: {
		if (_abort) {
			finish();
			placed = false;
			return true;
		}
		const int mx = mouseX(), my = mouseY();
		const bool stock = hitStock(mx, my);
		const int loose = hitPlaced(mx, my);
		setCursor(stock || loose >= 0 ? 12 : 0);
		if (mouseHeld()) {
			if (stock) {
				_drag = kOrder[_top];
				draw(_drag);
				dragBegin(mx, my, kPiece[_drag].sx, kPiece[_drag].sy, _spr, _drag);
				_state = 4;
				setCursor(13);
				playSfx(_sfxPick);
			} else if (loose >= 0) {
				int idx = 0;
				draw(loose);
				for (int i = 0; i < _placedCount; i++) {
					if (_listPiece[i] == loose)
						idx = i;
				}
				dragBegin(mx, my, _listX[idx], _listY[idx], _spr, loose);
				_drag = loose;
				_state = 5;
				setCursor(13);
				playSfx(_sfxPick);
			}
		}
		break;
	}
	case 4:
	case 5: {
		setCursor(13);
		if (mouseHeld())
			break;
		dragEnd();
		int x, y;
		dragPos(x, y);
		setCursor(0);
		const int r = dropTest(x, y);
		drop(r, x, y, _state == 5);
		break;
	}
	case 6:
		setCursor(9);
		_count++;
		if (_count == 0x3c) {
			finish();
			placed = true;
			return true;
		}
		break;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Page 16, slot 0, item 26 (A03_05): hook12 sub_4066b0, hook13 sub_406810, run sub_406a30.
// Six pieces at the right are dropped onto six slots; when all are down the wrong ones are taken back, and the
// handler ends once every piece sits in its own slot.

class A03_05Game : public PageGame {
public:
	explicit A03_05Game(Page &page) : PageGame(page), _state(0), _count(0), _tries(0), _abort(false), _slot(0), _sel(0) {
		clearSlots();
	}

	void start(int item, int slot) override {
		_slot = slot;
		loadSprite(_spr, "a03_05", 2);
		_sel = 0;
		_sfxPick = loadSfx("cf_clic1");
		_sfxDrop = loadSfx("cf_clic2");
		_sfxBad = loadSfx("cf_clic3");
		_sfxWin = 0;
		_state = 0;
		_count = 0;
		_tries = 0;
		_abort = false;
		startMovie(0x33, 0);
	}

	void onAbort() override { _abort = true; }

	bool run(uint tick, bool &placed) override;

private:
	/** One row of the table at 0x4f2b88 (stride 0x38) and the rect pair of 0x4f2cd8 (stride 0x20). */
	struct Piece {
		int sx, sy;			// where it waits (and the drag origin)
		int slotX[6], slotY[6];	// where it is drawn in each of the six slots
		PgRect pick, target;	// pick-up rect, and the slot rect of the same index
	};
	static const Piece kPiece[6];

	void clearSlots() {
		for (int i = 0; i < 6; i++) {
			_slotOf[i] = -1;
			_isPlaced[i] = false;
			_slotTaken[i] = false;
		}
	}

	/** sub_4068d0 / sub_406830 over every piece except @p skip. */
	void draw(int skip) {
		restoreFromSnapshot(0, 0, 640, 480);
		for (int i = 0; i < 6; i++) {
			if (i == skip)
				continue;
			if (_isPlaced[i])
				blitCell(_spr, i, kPiece[i].slotX[_slotOf[i]], kPiece[i].slotY[_slotOf[i]]);
			else
				blitCell(_spr, i, kPiece[i].sx, kPiece[i].sy);
		}
	}

	void finish() { stopStream(); }	// sub_406790

	int _state, _count, _tries;
	bool _abort;
	int _slot, _sel;
	int _slotOf[6];		// dat_527b3c: slot a piece sits in
	bool _isPlaced[6];	// dat_527ac0
	bool _slotTaken[6];	// dat_527b24
	PgSprite _spr;
	uint16 _sfxWin, _sfxPick, _sfxDrop, _sfxBad;
};

const A03_05Game::Piece A03_05Game::kPiece[6] = {
	{ 532, 129, { 290, 381, 430, 401, 253, 452 }, { 313, 288, 275, 254, 276, 259 }, { 507, 114, 52, 36 }, { 274, 294, 33, 38 } },
	{ 609, 127, { 289, 380, 429, 400, 252, 451 }, { 316, 291, 278, 257, 279, 262 }, { 600, 112, 20, 27 }, { 370, 276, 20, 28 } },
	{ 522, 216, { 289, 380, 429, 400, 252, 451 }, { 309, 284, 271, 250, 272, 255 }, { 513, 204, 20, 32 }, { 421, 258, 17, 30 } },
	{ 606, 217, { 289, 380, 429, 400, 252, 451 }, { 313, 288, 275, 254, 276, 259 }, { 598, 206, 17, 25 }, { 392, 243, 17, 23 } },
	{ 521, 312, { 286, 377, 426, 397, 249, 448 }, { 321, 296, 283, 262, 284, 267 }, { 515, 294, 19, 26 }, { 245, 268, 15, 27 } },
	{ 607, 315, { 288, 379, 428, 399, 251, 450 }, { 324, 299, 286, 265, 287, 270 }, { 600, 293, 17, 26 }, { 444, 257, 14, 26 } }
};

bool A03_05Game::run(uint tick, bool &placed) {
	setCursor(0);
	switch (_state) {
	case 0:
		if (movieStep()) {
			loadBg("a03_05");
			drawCapsules(_slot);
			stream("A03_05b");
			_state = 1;
		}
		break;
	case 1:
		if (voiceStep()) {
			_count = 0;
			_state = 2;
		}
		break;
	case 2:
		setCursor(9);
		_count++;
		if (_count == 0x4b) {
			loadBg("a03_05e");
			drawCapsules(_slot);
			startMovie(0x35, 1);
			_state = 3;
		}
		break;
	case 3:
		if (movieStep()) {
			loadBg("a03_05e");
			drawCapsules(_slot);
			snapshot();	// sub_47683e(0x9e, 0xd2, 0x142, 0x96)
			clearSlots();
			_state = 4;
		}
		break;
	case 4:
		draw(-1);
		setCursor(9);
		_state = 5;
		_count = 0;
		break;
	case 5: {
		if (_abort) {
			finish();
			placed = false;
			return true;
		}
		int hit = -1;
		for (int i = 0; i < 6 && hit < 0; i++) {
			if (kPiece[i].pick.has(mouseX(), mouseY()) && !_isPlaced[i])
				hit = i;
		}
		setCursor(hit >= 0 ? 12 : 0);
		if (mouseHeld() && hit >= 0) {
			draw(hit);
			dragBegin(mouseX(), mouseY(), kPiece[hit].sx, kPiece[hit].sy, _spr, hit);
			_state = 6;
			_sel = hit;
			setCursor(13);
			playSfx(_sfxPick);
		}
		_count++;
		if (_count >= 0xfb) {
			stream("a03_05d");
			_count = 0;
		}
		break;
	}
	case 6: {
		setCursor(13);
		if (mouseHeld())
			break;
		dragEnd();
		int x, y;
		dragPos(x, y);
		int s = -1;
		for (int i = 0; i < 6 && s < 0; i++) {
			if (kPiece[i].target.has(x, y) && !_slotTaken[i])
				s = i;
		}
		if (s >= 0) {
			// sub_4068d0(piece, slot): drawn in that slot of the piece's own table row.
			_slotOf[_sel] = s;
			_isPlaced[_sel] = true;
			_slotTaken[s] = true;
			bool all = true;
			for (int i = 0; i < 6; i++)
				all = all && _isPlaced[i];
			_state = all ? 7 : 5;
			_count = 0;
			playSfx(_sfxDrop);
			draw(-1);
		} else {
			draw(-1);
			_count = 0;
			_state = 5;
			playSfx(_sfxBad);
		}
		break;
	}
	case 7:
		_count++;
		if (_count == 0x28) {
			bool right = true;
			for (int i = 0; i < 6; i++) {
				if (_slotOf[i] != i) {
					_isPlaced[i] = false;
					_slotTaken[_slotOf[i]] = false;
					_slotOf[i] = -1;
					right = false;
				}
			}
			if (right) {
				loadBg("a03_05c");
				drawCapsules(_slot);
				_count = 0;
				_state = 8;
				_sfxWin = loadSfx(_tries >= 2 ? (_tries >= 4 ? "a03_05h" : "a03_05g") : "a03_05f");
				playSfx(_sfxWin);
			} else {
				_state = 4;
				_tries++;
				stream("a03_05e");
			}
		}
		setCursor(9);
		break;
	case 8:
		setCursor(9);
		_count++;
		if (_count == 0x32) {
			finish();
			placed = true;
			return true;
		}
		break;
	}
	return false;
}

} // End of anonymous namespace

PageGame *createA03_042Game(Page &page) {
	return new A03_042Game(page);
}

PageGame *createA13_061Game(Page &page) {
	return new A13_061Game(page);
}

PageGame *createA03_05Game(Page &page) {
	return new A03_05Game(page);
}

} // End of namespace Vangogh
