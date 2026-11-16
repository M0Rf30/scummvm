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

#include "vangogh/pagegames.h"

namespace Vangogh {

namespace {

/** Exe data 0x4f2a30: the two draw positions (x, y, x, y) of each of the 6 pieces of item 14. */
const int kPiecePos[6][4] = {
	{436, 29, 496, 29}, {454, 103, 486, 103}, {364, 146, 408, 145},
	{256, 154, 263, 152}, {118, 29, 554, 29}, {118, 184, 304, 155}
};

/** Exe data 0x4f2a90: the two hit rects of each piece. */
const PgRect kPieceHit[6][2] = {
	{{447, 34, 35, 54}, {502, 28, 34, 51}}, {{459, 106, 20, 35}, {494, 110, 43, 32}},
	{{376, 154, 31, 20}, {416, 149, 28, 19}}, {{253, 153, 9, 18}, {262, 153, 8, 16}},
	{{119, 29, 41, 256}, {557, 29, 33, 285}}, {{120, 206, 77, 135}, {305, 172, 41, 71}}
};

/** Exe data 0x4f2b30: the extra hit rect of piece 4 that selects piece 5. */
const PgRect kPiece5Hit = {120, 206, 77, 135};

/** Exe data 0x4f2b50: the two answer rects of item 13. */
const PgRect kAnswerHit[2] = {{122, 39, 217, 170}, {122, 224, 217, 170}};

/** Exe data 0x4f2b70: the right answer of each of the 6 questions of item 13. */
const int kRightAnswer[6] = {1, 1, 0, 0, 0, 1};

/** Exe data 0x4f2ed8: the four choice rects of item 17. */
const PgRect kChoiceHit[4] = {{255, 76, 112, 39}, {255, 161, 112, 39}, {256, 255, 112, 39}, {255, 346, 112, 39}};

/**
 * Page 8 slot 0, item 13 (hook12 sub_4039f0, hook13 sub_403c10, run sub_403d70): a voice intro, then six
 * two-answer questions, each a sprite pair "a13_031<a..l>" (question animation, answer animation).
 */
class A13_031Game : public PageGame {
public:
	A13_031Game(Page &page) : PageGame(page), _slot(0), _state(0), _ticks(0), _abort(false), _q(0), _answer(0),
		_sndRight(0), _sndWrong(0), _sndQuestion(0), _sndPick(0), _sndDone(0) {}

	void start(int item, int slot) override {
		_q = 0;
		_sndDone = loadSfx("reussit");
		_sndRight = loadSfx("cc_clic2");
		_sndWrong = loadSfx("pt_clic3");
		_sndQuestion = loadSfx("cc_clic4");
		_sndPick = loadSfx("cc_clic1");
		stream("A13_031");
		_slot = slot;
		_state = 0;
		_ticks = 0;
		_abort = false;
	}

	void onAbort() override {
		_abort = true;
	}

	bool run(uint tick, bool &placed) override {
		setCursor(0);
		switch (_state) {
		case 0:
			if (voiceStep()) {
				loadBg("a13_031");
				drawCapsules(_slot);
				stream("a13_031b");
				_state = 1;
			}
			break;
		case 1:
			if (voiceStep())
				_state = 2;
			break;
		case 2:
			for (int i = 0; i < 2; i++) {
				const char name[10] = {'a', '1', '3', '_', '0', '3', '1', (char)('a' + i + _q * 2), 0, 0};
				loadSprite(_spr[i], name, 1);
			}
			animStart(_spr[0], 0, 0, 1);
			setCursor(9);
			_state = 3;
			playSfx(_sndQuestion);
			break;
		case 3:
			setCursor(9);
			if ((tick & 1) && !animStep(_spr[0], 0, 0)) {
				const int last = (int)_spr[0].numCells() - 1;
				saveDraw(_spr[0], last, 0, 0);
				blitCell(_spr[0], last, 0, 0);
				_ticks = 0;
				_state = 4;
			}
			break;
		case 4: {
			if (_abort) {
				stopStream();
				placed = false;
				return true;
			}
			_ticks++;
			const int a = answerAt(mouseX(), mouseY());
			setCursor(a >= 0 ? 11 : 0);
			if (mouseHeld() && a >= 0) {
				erase(_spr[0]);
				animStart(_spr[1], 0, 0, 1);
				_state = 5;
				_answer = a;
				playSfx(_sndPick);
			}
			if (_ticks >= 0xfb) {
				stream("a13_031c");
				_ticks = 0;
			}
			break;
		}
		case 5:
			setCursor(9);
			if (tick & 1) {
				if (animStep(_spr[1], 0, 0)) {
					if (_spr[1].cell == 0xf)
						playSfx(_answer != kRightAnswer[_q] ? _sndWrong : _sndRight);
				} else {
					_q++;
					_state = _q != 6 ? 2 : 6;
				}
			}
			break;
		case 6:
			if (!mouseHeld()) {
				_ticks = 0;
				_state = 7;
			}
			break;
		case 7:
			setCursor(9);
			_ticks++;
			if (_ticks == 0x3c) {
				stopStream();
				placed = true;
				return true;
			}
			break;
		default:
			break;
		}
		return false;
	}

private:
	/** sub_403980 */
	int answerAt(int x, int y) const {
		for (int i = 0; i < 2; i++)
			if (kAnswerHit[i].has(x, y))
				return i;
		return -1;
	}

	PgSprite _spr[2];
	int _slot, _state, _ticks;
	bool _abort;
	int _q, _answer;
	uint16 _sndRight, _sndWrong, _sndQuestion, _sndPick, _sndDone;
};

/**
 * Page 8 slot 1, item 14 (hook12 sub_4039f0, hook13 sub_403c10, run sub_4042a0): six pieces of a picture on the
 * background "a13_032c", each to be clicked; a clicked piece blinks for 25 ticks and is then removed for good.
 */
class A13_032Game : public PageGame {
public:
	A13_032Game(Page &page) : PageGame(page), _slot(0), _state(0), _next(0), _ticks(0), _abort(false), _sel(-1),
		_side(0), _playing(false), _sndClick(0), _sndDone(0) {
		for (int i = 0; i < 6; i++)
			_done[i] = false;
	}

	void start(int item, int slot) override {
		loadBg("a13_032c");
		drawCapsules(slot);
		snapshot();
		loadSprite(_spr, "a13_032", 0);
		_sel = -1;
		for (int i = 0; i < 6; i++)
			_done[i] = false;
		for (int i = 0; i < 6; i++)
			drawPiece(i);
		_sndClick = loadSfx("cc1_clic");
		_sndDone = loadSfx("reussit");
		_playing = false;
		stream("a13_032");
		_slot = slot;
		_state = 0;
		_ticks = 0;
		_abort = false;
	}

	void onAbort() override {
		_abort = true;
	}

	bool run(uint tick, bool &placed) override {
		setCursor(0);
		switch (_state) {
		case 0:
			if (voiceStep()) {
				_ticks = 0;
				_state = 1;
			}
			return false;
		case 1:
			break;
		case 2:
			setCursor(0);
			if (mouseHeld())
				return false;
			_ticks = 0;
			_state = _next;
			return false;
		case 3: {
			setCursor(9);
			const int s = _sel;
			_ticks++;
			if (_ticks != 0x19) {
				if (!_playing) {
					_playing = true;
					playSfx(_sndClick);
				}
				_side = tick & 1;
				drawPiece(s);
				return false;
			}
			stopSfx(_sndClick);
			_done[_sel] = true;
			restoreFromSnapshot(0x6e, 0x1d, 0x1e2, 0x182);
			_sel = -1;
			for (int i = 0; i < 6; i++)
				drawPiece(i);
			_playing = false;
			int n = 0;
			while (n < 6 && _done[n])
				n++;
			_ticks = 0;
			if (n == 6) {
				_state = 4;
				playSfx(_sndDone);
			} else {
				_state = 1;
			}
			return false;
		}
		case 4:
			_ticks++;
			setCursor(9);
			if (_ticks == 0x32) {
				_state = 5;
				stream("A13_032c");
			}
			return false;
		case 5:
			if (voiceStep()) {
				stopStream();
				placed = true;
				return true;
			}
			return false;
		default:
			return false;
		}

		// State 1 (label_4042fe)
		if (_abort) {
			stopStream();
			placed = false;
			return true;
		}
		_ticks++;
		if (mouseHeld()) {
			int piece, side = 0;
			pieceAt(mouseX(), mouseY(), piece, side);
			if (piece >= 0) {
				if (piece != _sel) {
					if (_sel != -1) {
						const int old = _sel;
						_sel = -1;
						drawPiece(old);
					}
					_sel = piece;
					_side = side;
					drawPiece(piece);
					_state = 2;
					_next = 1;
				} else {
					if (side != _side) {
						_ticks = 0;
						_next = 3;
					} else {
						_sel = -1;
						drawPiece(piece);
						_next = 1;
					}
					_state = 2;
				}
			}
		}
		if (_ticks + 1 <= 0x271) {
			_ticks++;
			return false;
		}
		_ticks = 0;
		stream("A13_032b");
		return false;
	}

private:
	/** sub_4033c0: draws the two sprite cells of piece a0. */
	void drawPiece(int a0) {
		if (a0 < 0 || a0 > 5)
			return;
		int c1, c2, c3 = -1;
		if (a0 < 4) {
			if (_done[a0])
				return;
			if (a0 != _sel) {
				c1 = a0 * 2;
				c2 = a0 * 2 + 1;
			} else {
				c1 = (_side ^ 1) * 14 + a0 * 2;
				c2 = _side * 14 + 1 + a0 * 2;
			}
		} else if (a0 == 5) {
			if (_done[5])
				return;
			if (_sel != 5) {
				if (_sel != 4) {
					c1 = _done[4] ? 12 : 10;
					c2 = 13;
				} else {
					c1 = (_side ^ 1) + 10;
					c2 = _side * 14 + 13;
				}
			} else {
				c1 = (_side ^ 1) * 14 + (_done[4] ? 12 : 10);
				c2 = _side * 14 + 13;
			}
		} else {
			if (_done[4])
				return;
			if (_sel != 4) {
				if (_sel != 5) {
					c1 = 8;
					c2 = 9;
					c3 = _done[5] ? -1 : 10;
				} else {
					c1 = (_side ^ 1) * 14 + 8;
					c2 = _side * 14 + 9;
					c3 = (_side ^ 1) * 14 + 10;
				}
			} else {
				c3 = _done[5] ? -1 : (_side ^ 1) + 10;
				c1 = (_side ^ 1) * 14 + 8;
				c2 = _side * 14 + 9;
			}
		}
		blitCell(_spr, c1, kPiecePos[a0][0], kPiecePos[a0][1]);
		blitCell(_spr, c2, kPiecePos[a0][2], kPiecePos[a0][3]);
		if (c3 > -1)
			blitCell(_spr, c3, kPiecePos[5][0], kPiecePos[5][1]);
	}

	/** sub_403850: piece (-1 = none) and side under the pointer. */
	void pieceAt(int x, int y, int &piece, int &side) const {
		for (int i = 0; i < 6; i++) {
			for (int j = 0; j <= 1; j++) {
				if (kPieceHit[i][j].has(x, y) && !_done[i]) {
					if (i == 4 && j == 0 && !_done[5] && kPiece5Hit.has(x, y)) {
						piece = 5;
						side = 0;
						return;
					}
					piece = i;
					side = j;
					return;
				}
			}
		}
		piece = -1;
	}

	PgSprite _spr;
	bool _done[6];
	int _slot, _state, _next, _ticks;
	bool _abort;
	int _sel, _side;
	bool _playing;
	uint16 _sndClick, _sndDone;
};

/**
 * Page 10 slot 1, item 17 (hook12 sub_4047a0, hook13 sub_404880, run sub_404920): a menu of four choices on the
 * background "a13_052a"; each plays its clip (0x17 + choice), the fourth then ends the game with a voice.
 */
class A13_052Game : public PageGame {
public:
	A13_052Game(Page &page) : PageGame(page), _slot(0), _state(0), _next(0), _sel(0), _abort(false) {}

	void start(int item, int slot) override {
		loadBg("a13_052a");
		drawCapsules(slot);
		startMovie(0x16, 1);
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
			if (movieStep()) {
				loadBg("a13_052a");
				drawCapsules(_slot);
				_state = 1;
			}
			break;
		case 1: {
			if (_abort) {
				stopStream();
				placed = false;
				return true;
			}
			int c = -1;
			for (int i = 0; i < 4; i++) {
				if (kChoiceHit[i].has(mouseX(), mouseY())) {
					c = i;
					break;
				}
			}
			setCursor(c >= 0 ? 11 : 0);
			if (mouseHeld() && c >= 0) {
				_sel = c;
				_state = 2;
				_next = 3;
			}
			break;
		}
		case 2:
			setCursor(9);
			if (!mouseHeld())
				_state = _next;
			break;
		case 3:
			startMovie(_sel + 0x17, 1);
			_state = 4;
			break;
		case 4:
			if (movieStep()) {
				if (_sel != 3) {
					loadBg("a13_052a");
					drawCapsules(_slot);
					_state = 1;
				} else {
					loadBg("A13_05");
					drawCapsules(_slot);
					stream("A13_052f");
					_state = 5;
				}
			}
			break;
		case 5:
			if (voiceStep()) {
				stopStream();
				placed = true;
				return true;
			}
			break;
		default:
			break;
		}
		return false;
	}

private:
	int _slot, _state, _next, _sel;
	bool _abort;
};

} // End of anonymous namespace

PageGame *createA13_031Game(Page &page) {
	return new A13_031Game(page);
}

PageGame *createA13_032Game(Page &page) {
	return new A13_032Game(page);
}

PageGame *createA13_052Game(Page &page) {
	return new A13_052Game(page);
}

} // End of namespace Vangogh
