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

#include "common/str.h"

#include "vangogh/pagegames.h"

namespace Vangogh {

namespace {

/**
 * Page 2 slot 0, item 3: the kettle (A01_032). Hook12 sub_407c40 (item 3 branch), hook13 sub_407d60, run sub_407e60.
 * Five stages: a movie, a voice, then the kettle sprite of the stage is dragged from its hotspot onto the target.
 */
class KettleGame : public PageGame {
public:
	explicit KettleGame(Page &page) : PageGame(page), _slot(0), _stage(0), _state(0), _timer(0), _abort(false),
		_clic1(0), _clic2(0), _clic3(0), _win(0) {}

	void start(int item, int slot) override {
		_stage = 0;						// dat_527c24
		loadBg("a01_032a");
		startMovie(0, 1);
		loadSprite(_spr, "a01_032a", 1);
		_clic1 = loadSfx("pt_clic1");
		_clic2 = loadSfx("pt_clic2");
		_clic3 = loadSfx("pt_clic3");
		_win = loadSfx("reussit");
		_slot = slot;
		_state = 0;
		_timer = 0;
		_abort = false;
	}

	void onAbort() override {
		_abort = true;
	}

	bool run(uint tick, bool &placed) override {
		setCursor(0);
		switch (_state) {
		case 0:
			if (movieStep()) {
				showStageBg(0);
				stream("a01_032");
				_state = 1;
			}
			break;
		case 1:
			if (voiceStep()) {
				stopStream();
				startMovie(1, 1);
				_state = 2;
			}
			break;
		case 2:
			if (movieStep()) {
				showStageBg(0);
				saveDraw(_spr, _stage, kRects[_stage].sx, kRects[_stage].sy);
				_state = 3;
				_timer = 0;
			}
			break;
		case 3: {
			if (_abort) {
				placed = false;
				return true;
			}
			const int mx = mouseX(), my = mouseY();
			_timer++;
			const bool inHot = kRects[_stage].hot.has(mx, my);
			setCursor(inHot ? 0xc : 0);
			if (mouseHeld()) {
				if (inHot) {
					_state = 4;
					erase(_spr);
					dragBegin(mx, my, kRects[_stage].sx, kRects[_stage].sy, _spr, _stage);
					playSfx(_clic1);
					setCursor(0xd);
				}
			} else if (_timer >= 0xfb) {
				stream("a01_032b");
				_timer = 0;
			}
			break;
		}
		case 4:
			setCursor(0xd);
			if (!mouseHeld()) {
				dragEnd();
				int x, y;
				dragPos(x, y);
				if (kRects[_stage].target.has(x, y)) {
					setCursor(9);
					showStageBg(1);
					if (_stage != 4) {
						_stage++;
						startMovie(_stage + 1, 1);
						_state = 2;
						playSfx(_clic2);
					} else {
						_timer = 0;
						_state = 5;
						playSfx(_win);
					}
				} else {
					saveDraw(_spr, _stage, kRects[_stage].sx, kRects[_stage].sy);
					setCursor(0);
					playSfx(_clic3);
					_state = 3;
					_timer = 0;
				}
			}
			break;
		case 5:
			setCursor(9);
			_timer++;
			if (_timer == 0x3c) {
				_state = 6;
				startMovie(6, 1);
			}
			break;
		case 6:
			if (movieStep()) {
				stream("a01_032c");
				_state = 7;
			}
			break;
		case 7:
			if (voiceStep()) {
				placed = true;
				return true;
			}
			break;
		}
		return false;
	}

private:
	/** Background "a01_032<letter>" of the stage (+ @p next), capsules redrawn. */
	void showStageBg(int next) {
		loadBg(Common::String::format("a01_032%c", 'a' + _stage + next).c_str());
		drawCapsules(_slot);
	}

	struct Stage {
		int sx, sy;			///< sprite position (exe table 0x4f33e0, stride 0x28)
		PgRect hot;			///< the kettle part to grab (+8)
		PgRect target;		///< where to drop it (+0x18)
	};
	static const Stage kRects[5];

	int _slot, _stage, _state, _timer;
	bool _abort;
	PgSprite _spr;
	uint16 _clic1, _clic2, _clic3, _win;
};

const KettleGame::Stage KettleGame::kRects[5] = {
	{ 573, 403, { 532, 361, 84, 85 }, { 205, 122, 71, 89 } },
	{ 553, 384, { 489, 295, 130, 177 }, { 124, 112, 75, 180 } },
	{ 572, 405, { 549, 356, 47, 97 }, { 309, 114, 40, 78 } },
	{ 573, 404, { 532, 337, 83, 132 }, { 284, 198, 54, 97 } },
	{ 566, 384, { 527, 299, 103, 173 }, { 393, 110, 90, 184 } }
};

/**
 * The rectangle tables of A03_012 in the order of the exe's data (0x4f2f88): the 3 button rects, then the 12 + 12 rects of
 * the two animation tables. sub_4025a0 indexes the second table with cell - 4 where cell 2 and 3 give -2 and -1, which
 * the exe resolves into the preceding table; one flat array reproduces that.
 */
const PgRect kA03_012Rects[27] = {
	{ 190, 76, 28, 48 }, { 370, 76, 29, 45 }, { 530, 70, 37, 46 },
	{ 281, 24, 15, 10 }, { 280, 24, 23, 18 }, { 290, 24, 26, 29 }, { 305, 35, 28, 28 }, { 327, 47, 29, 25 },
	{ 347, 53, 34, 23 }, { 371, 54, 33, 21 }, { 393, 48, 35, 20 }, { 418, 36, 22, 24 }, { 434, 24, 18, 24 },
	{ 445, 24, 18, 13 }, { 452, 24, 11, 7 },
	{ 447, 24, 12, 8 }, { 443, 24, 22, 13 }, { 435, 24, 25, 19 }, { 427, 28, 23, 28 }, { 403, 39, 33, 27 },
	{ 378, 47, 40, 28 }, { 354, 53, 45, 24 }, { 331, 47, 44, 29 }, { 312, 37, 38, 34 }, { 298, 28, 30, 32 },
	{ 284, 24, 27, 24 }, { 282, 24, 17, 13 }
};

/**
 * Page 4 slot 1, item 6 (A03_012): hook12 sub_402410 (item 6 branch), hook13 sub_402500, run sub_402620.
 * Click each of the three things (movies 8..10), then watch the looping animation of the last one and click its
 * hot rect to finish.
 */
class A03_012Game : public PageGame {
public:
	explicit A03_012Game(Page &page) : PageGame(page), _slot(0), _state(0), _timer(0), _last(0), _abort(false),
		_sfx(0) {
		for (int i = 0; i < 3; i++)
			_seen[i] = false;
	}

	void start(int item, int slot) override {
		_sfx = loadSfx("A03_012e");
		for (int i = 0; i < 3; i++)
			_seen[i] = false;
		startMovie(7, 1);
		_slot = slot;
		_state = 0;
		_timer = 0;
		_abort = false;
	}

	void onAbort() override {
		_abort = true;
	}

	bool run(uint tick, bool &placed) override {
		setCursor(0);
		switch (_state) {
		case 0:
			if (movieStep()) {
				background();
				stream("a03_012");
				_state = 1;
			}
			break;
		case 1:
			if (voiceStep())
				_state = 2;
			break;
		case 2: {
			if (_abort) {
				stopSfx(_sfx);
				placed = false;
				return true;
			}
			int idx = -1;
			for (int i = 0; i < 3; i++) {
				if (kA03_012Rects[i].has(mouseX(), mouseY())) {
					idx = i;
					break;
				}
			}
			setCursor(idx >= 0 ? 0xb : 0);
			_timer++;
			if (mouseHeld()) {
				if (idx >= 0) {
					_seen[idx] = true;
					_last = idx;
					_state = 3;
					stopStream();
					startMovie(_last + 8, 1);
				}
			} else if (_timer >= 0xfb) {
				stream("A03_012b");
				_timer = 0;
			}
			break;
		}
		case 3:
			if (movieStep()) {
				_timer = 0;
				_state = 4;
			}
			break;
		case 4:
			setCursor(9);
			_timer++;
			if (_timer >= 0x3d)
				_state = 5;
			break;
		case 5:
			if (!mouseHeld()) {
				int i = 0;
				while (i <= 2 && _seen[i])
					i++;
				if (i != 3) {
					stream("A03_012c");
					background();
					_state = 2;
				} else {
					const Common::String n = Common::String::format("A03_012%c", 'e' + _last);
					loadBg(n.c_str());
					loadSprite(_anim, n.c_str(), 1);
					drawCapsules(_slot);
					animStart(_anim, 0, 0, 0);
					playSfx(_sfx, true);
					_state = 6;
				}
			}
			break;
		case 6: {
			if (_abort) {
				stopSfx(_sfx);
				erase(_anim);
				animStop(_anim);
				placed = false;
				return true;
			}
			if (tick & 1)
				animStep(_anim, 0, 0);
			setCursor(0);
			const int c = _anim.cell;
			const int mx = mouseX(), my = mouseY();
			// sub_4025a0 (cells 2..13, rect c - 4) / sub_4025e0 (cells 16..27, rect c - 16 of the third table)
			const PgRect *hit = nullptr;
			if (c >= 2 && c <= 13)
				hit = &kA03_012Rects[3 + c - 4];
			else if (c >= 0x10 && c <= 0x1b)
				hit = &kA03_012Rects[15 + c - 0x10];
			if (hit && hit->has(mx, my)) {
				setCursor(0xb);
				if (mouseHeld()) {
					erase(_anim);
					animStop(_anim);
					stopSfx(_sfx);
					startMovie(0xe, 1);
					_state = 7;
					return false;
				}
			}
			break;
		}
		case 7:
			if (movieStep()) {
				_timer = 0;
				_state = 8;
				stream("A03_012d");
			}
			break;
		case 8:
			if (!mouseHeld())
				_state = 9;
			break;
		case 9:
			if (voiceStep())
				_state = 10;
			break;
		case 10:
			setCursor(9);
			_timer++;
			if (_timer == 0x3c) {
				stopSfx(_sfx);
				placed = true;
				_state = 0;
				return true;
			}
			break;
		}
		return false;
	}

private:
	void background() {
		loadBg("a03_012a");
		drawCapsules(_slot);
	}

	int _slot, _state, _timer, _last;
	bool _abort;
	bool _seen[3];
	PgSprite _anim;
	uint16 _sfx;
};

/**
 * Page 7 slot 1, item 12 (A03_023): hook12 sub_402dc0 (item 12 branch), hook13 sub_402ed0, run sub_4030c0.
 * Three buttons at the right edge, each once; the movie played depends on which ones were already done.
 */
class A03_023Game : public PageGame {
public:
	explicit A03_023Game(Page &page) : PageGame(page), _slot(0), _state(0), _mask(0), _pick(0), _abort(false) {
		for (int i = 0; i < 3; i++)
			_done[i] = false;
	}

	void start(int item, int slot) override {
		loadBg("A03_023a");
		drawCapsules(slot);
		for (int i = 0; i < 3; i++)
			_done[i] = false;
		_mask = 0;
		stream("A03_023");
		_slot = slot;
		_state = 0;
		_abort = false;
	}

	void onAbort() override {
		_abort = true;
	}

	bool run(uint tick, bool &placed) override {
		setCursor(0);
		switch (_state) {
		case 0:
			if (voiceStep())
				_state = 1;
			break;
		case 1: {
			if (_abort) {
				placed = false;
				return true;
			}
			int idx = -1;
			for (int i = 0; i < 3; i++) {
				if (kButtons[i].has(mouseX(), mouseY()) && !_done[i]) {		// sub_403040
					idx = i;
					break;
				}
			}
			setCursor(idx >= 0 ? 0xb : 0);
			if (mouseHeld() && idx >= 0) {
				_pick = idx;
				_state = 2;
			}
			break;
		}
		case 2:
			if (!mouseHeld()) {
				startMovie(kMovies[_mask][_pick], 1);
				_state = 3;
			}
			break;
		case 3:
			if (movieStep()) {
				const int bg = kBgs[_mask][_pick];
				_mask |= 1 << _pick;
				_done[_pick] = true;
				if (_mask != 7) {
					loadBg(Common::String::format("A03_023%c", 'a' + bg).c_str());
					drawCapsules(_slot);
					_state = 1;
				} else {
					loadBg("A03_02");
					drawCapsules(_slot);
					stream("A03_023e");
					_state = 4;
				}
			}
			break;
		case 4:
			if (voiceStep()) {
				stream("A03_023f");
				_state = 5;
			}
			break;
		case 5:
			if (voiceStep()) {
				placed = true;
				_state = 0;
				return true;
			}
			break;
		}
		return false;
	}

private:
	static const PgRect kButtons[3];
	static const int kBgs[8][3];
	static const int kMovies[8][3];

	int _slot, _state, _mask, _pick;
	bool _abort;
	bool _done[3];
};

const PgRect A03_023Game::kButtons[3] = {
	{ 597, 187, 39, 38 }, { 597, 258, 39, 37 }, { 597, 324, 39, 38 }
};

// Exe table 0x4f3198, stride 0x18: three background letters then three movie indices, per mask of finished buttons.
const int A03_023Game::kBgs[8][3] = {
	{ 2, 3, 4 }, { 0, 13, 11 }, { 7, 0, 12 }, { 0, 0, 10 }, { 6, 5, 0 }, { 0, 9, 0 }, { 8, 0, 0 }, { 0, 0, 0 }
};

const int A03_023Game::kMovies[8][3] = {
	{ 39, 40, 41 }, { 0, 48, 47 }, { 45, 0, 44 }, { 0, 0, 50 }, { 43, 42, 0 }, { 0, 49, 0 }, { 46, 0, 0 }, { 0, 0, 0 }
};

} // End of anonymous namespace

PageGame *createKettleGame(Page &page) {
	return new KettleGame(page);
}

PageGame *createA03_012Game(Page &page) {
	return new A03_012Game(page);
}

PageGame *createA03_023Game(Page &page) {
	return new A03_023Game(page);
}

} // End of namespace Vangogh
