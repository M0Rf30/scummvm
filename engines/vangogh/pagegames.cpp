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
#include "common/textconsole.h"

#include "vangogh/page.h"
#include "vangogh/pagegames.h"
#include "vangogh/sound.h"
#include "vangogh/world.h"

namespace Vangogh {

/*
 * The slot handlers of the exe's page records (flow.md 5.3, state machine of sub_414080, states 0xe / 0xd):
 *
 *   page+0x48  start hook   (item, slot)   called in state 0xe, also when a placed capsule is clicked
 *   page+0x4c  abort hook                  called when Backspace is pressed while a handler runs
 *   page+0x50+slot  run     (tick, &flag)  called every tick in state 0xd until it returns non-zero
 *
 * A handler that leaves dat_52a4c0 (the flag argument) unset after a drop sends the item back to the bar; the flag
 * keeps its value between handlers, exactly as the exe's global does (Page::_gameOk).
 *
 * The 25 pages' records (0x4f3950, stride 0x6c) name these functions:
 *   shared movie handler  (start hook = sub_417380(m, 0), run = sub_4177b0): pages 1 3 5 6 9 11 12 14 17 22 23 and the
 *                         item 7, 16, 18, 22, 23, 25, 27, 30, 32, 33 slots of the mixed pages
 *   shared voice handler  (start hook = sub_42dac0, run = sub_4177f0): items 2, 5, 20, 29
 *   voice then movie      (sub_402f00 / sub_402fa0): items 10 and 11 of page 7
 *   bespoke               items 3, 6, 12, 13, 14, 17, 21, 24, 26, 28, 31, 34 (pagegames_*.cpp)
 */

// ---------------------------------------------------------------------------
// Base class: the exe's primitives on the page's back buffer.

PageGame::PageGame(Page &page) : _p(page), _bgReloaded(false), _dragOn(false), _dragSpr(nullptr), _dragCell(0),
	_dragOffX(0), _dragOffY(0), _voice(0), _voiceStart(0), _in(nullptr) {
}

PageGame::~PageGame() {
	if (_p._sound) {
		if (_voice)
			_p._sound->stop(_voice);
		for (uint i = 0; i < _sfx.size(); i++)
			_p._sound->stop(_sfx[i]);
	}
}

int PageGame::mouseX() const {
	return _in ? _in->mouseX : 0;
}

int PageGame::mouseY() const {
	return _in ? _in->mouseY : 0;
}

bool PageGame::mouseHeld() const {
	return _in && _in->mouseDown;
}

void PageGame::setCursor(int kind) {
	_p._cursor = kind;
}

void PageGame::stream(const char *name) {
	if (!_p._sound)
		return;
	if (_voice)
		_p._sound->stop(_voice);
	_voice = _p._sound->load(Common::String("!") + name);
	if (_voice)
		_p._sound->play(_voice, false);
	_voiceStart = _p._tick;
}

void PageGame::stopStream() {
	if (_voice && _p._sound)
		_p._sound->stop(_voice);
	_voice = 0;
}

bool PageGame::streamDone() const {
	if (!_voice || !_p._sound)
		return true;
	// The mixer reports a freshly started stream as inactive for a tick or two.
	return !_p._sound->isPlaying(_voice) && _p._tick > _voiceStart + 2;
}

uint16 PageGame::loadSfx(const char *name) {
	if (!_p._sound)
		return 0;
	const uint16 h = _p._sound->load(name);
	if (h)
		_sfx.push_back(h);
	return h;
}

void PageGame::playSfx(uint16 h, bool loop) {
	if (h && _p._sound)
		_p._sound->play(h, loop);
}

void PageGame::stopSfx(uint16 h) {
	if (h && _p._sound)
		_p._sound->stop(h);
}

bool PageGame::voiceStep() {
	if (!streamDone() && !mouseHeld())
		return false;
	stopStream();
	setCursor(0);
	return true;
}

void PageGame::startMovie(int index, int noSkip) {
	_p.startMovie(index, noSkip != 0);
}

bool PageGame::movieDone() const {
	return _p._movieIndex < 0 || _p._clip.done();
}

bool PageGame::movieStep() {
	if (_p._movieSkippable && mouseHeld())
		return true;
	if (_p._movieIndex < 0)
		return true;
	_p._clip.step();
	return _p._clip.done();
}

void PageGame::loadBg(const char *name) {
	_p.loadBackground(name);
	_p._movieLayer = false;
	_bgReloaded = true;
	for (uint i = 0; i < _sprites.size(); i++)
		_sprites[i]->shown = false;
}

void PageGame::drawCapsules(int openSlot) {
	_p.bakeCapsules(openSlot);
}

void PageGame::snapshot() {
	_snap.free();
	_snap.copyFrom(_p._bg);
}

void PageGame::restoreFromSnapshot(int x, int y, int w, int h) {
	if (!_snap.getPixels())
		return;
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	w = MIN(w, MIN(_snap.w, _p._bg.w) - x);
	h = MIN(h, MIN(_snap.h, _p._bg.h) - y);
	if (w <= 0 || h <= 0)
		return;
	const int bpp = _p._bg.format.bytesPerPixel;
	for (int row = 0; row < h; row++)
		memcpy(_p._bg.getBasePtr(x, y + row), _snap.getBasePtr(x, y + row), (size_t)w * bpp);
}

bool PageGame::loadSprite(PgSprite &s, const char *name, int mode) {
	s.mode = mode;
	s.running = false;
	s.cell = 0;
	s.delay = 0;
	s.shown = false;
	if (!s.spr.loadSpr(name))
		return false;
	bool known = false;
	for (uint i = 0; i < _sprites.size(); i++)
		known |= _sprites[i] == &s;
	if (!known)
		_sprites.push_back(&s);
	return true;
}

void PageGame::blitCell(PgSprite &s, int cell, int x, int y) {
	s.spr.blit(_p._bg, cell, x, y);
}

void PageGame::saveDraw(PgSprite &s, int cell, int x, int y) {
	if (s.mode == 0) {
		blitCell(s, cell, x, y);
		return;
	}
	s.shown = true;
	s.shownCell = cell;
	s.shownX = x;
	s.shownY = y;
}

void PageGame::erase(PgSprite &s) {
	s.shown = false;
}

void PageGame::animStart(PgSprite &s, int x, int y, int delay) {
	s.cell = 0;
	s.running = true;
	s.delay = delay;
	if (s.mode)
		s.shown = false;
	animStep(s, x, y);
}

bool PageGame::animStep(PgSprite &s, int x, int y) {
	if (!s.running)
		return false;
	// sub_40ad00 / sub_40ad70: the previous cell goes away first (sprites with a saved background only).
	if (s.mode && s.shown)
		erase(s);
	if (s.cell == (int)s.numCells()) {
		if (s.delay) {
			s.delay--;
			if (!s.delay)
				s.running = false;
			return false;
		}
		s.cell = 0;
	}
	const int c = s.cell++;
	if (s.mode == 1)
		saveDraw(s, c, x, y);
	else
		blitCell(s, c, x, y);
	return true;
}

void PageGame::dragBegin(int mx, int my, int sx, int sy, PgSprite &s, int cell) {
	_dragOffX = sx - mx;
	_dragOffY = sy - my;
	_dragSpr = &s;
	_dragCell = cell;
	_dragOn = true;
}

void PageGame::dragEnd() {
	_dragOn = false;
}

void PageGame::dragPos(int &x, int &y) const {
	x = mouseX() + _dragOffX;
	y = mouseY() + _dragOffY;
}

void PageGame::drawOverlays(Graphics::Surface &fb) const {
	for (uint i = 0; i < _sprites.size(); i++) {
		const PgSprite &s = *_sprites[i];
		if (s.shown)
			s.spr.blit(fb, s.shownCell, s.shownX, s.shownY);
	}
	if (_dragOn && _dragSpr)
		_dragSpr->spr.blit(fb, _dragCell, mouseX() + _dragOffX, mouseY() + _dragOffY);
}

// ---------------------------------------------------------------------------
// The shared handlers.

namespace {

/** Start hook sub_417380(movie, 0) + run sub_4177b0. */
class MovieGame : public PageGame {
public:
	MovieGame(Page &page, int movie) : PageGame(page), _movie(movie) {}
	void start(int item, int slot) override { startMovie(_movie, 0); }
	bool run(uint tick, bool &placed) override {
		placed = true;
		return movieStep();
	}
private:
	int _movie;
};

/** Start hook sub_42dac0(voice) + run sub_4177f0. */
class VoiceGame : public PageGame {
public:
	VoiceGame(Page &page, const char *voice) : PageGame(page), _voice(voice) {}
	void start(int item, int slot) override { stream(_voice); }
	bool run(uint tick, bool &placed) override {
		placed = true;
		return voiceStep();
	}
private:
	const char *_voice;
};

/** Page 7 items 10 and 11 (sub_402f00, sub_402fa0): the voice, then the item's clip. */
class VoiceMovieGame : public PageGame {
public:
	VoiceMovieGame(Page &page, const char *voice, int movie) : PageGame(page), _voice(voice), _movie(movie), _state(0) {}
	void start(int item, int slot) override {
		stream(_voice);
		_state = 0;
	}
	bool run(uint tick, bool &placed) override {
		if (_state == 0) {
			placed = true;
			if (voiceStep()) {
				startMovie(_movie, 0);
				_state = 1;
			}
		} else if (_state == 1 && (placed = true, movieStep())) {
			return true;
		}
		return false;
	}
private:
	const char *_voice;
	int _movie;
	int _state;
};

} // End of anonymous namespace

PageGame *PageGame::create(Page &page, int pageIndex, int slot) {
	switch (pageIndex) {
	case 1: return new MovieGame(page, 29);
	case 2: return slot == 0 ? createKettleGame(page) : new VoiceGame(page, "A01_031");
	case 3: return new MovieGame(page, 60);
	case 4:
		if (slot == 0)
			return new VoiceGame(page, "A03_011");
		return slot == 1 ? createA03_012Game(page) : new MovieGame(page, 15);
	case 5: return new MovieGame(page, 21);
	case 6: return new MovieGame(page, 61);
	case 7:
		if (slot == 0)
			return new VoiceMovieGame(page, "A03_021", 30);
		return slot == 1 ? createA03_023Game(page) : new VoiceMovieGame(page, "A03_022", 31);
	case 8: return slot == 0 ? createA13_031Game(page) : createA13_032Game(page);
	case 9: return new MovieGame(page, 59);
	case 10: return slot == 0 ? new MovieGame(page, 20) : createA13_052Game(page);
	case 11: return new MovieGame(page, 58);
	case 12: return new MovieGame(page, 62);
	case 13: return slot == 0 ? createA03_042Game(page) : new VoiceGame(page, "A03_041");
	case 14: return new MovieGame(page, slot == 0 ? 37 : 38);
	case 15: return slot == 0 ? createA13_061Game(page) : new MovieGame(page, 66);
	case 16: return createA03_05Game(page);
	case 17: return new MovieGame(page, 63);
	case 18: return createA03_06Game(page);
	case 19: return slot == 0 ? static_cast<PageGame *>(new VoiceGame(page, "A04_01")) : new MovieGame(page, 67);
	case 20: return createA14_01Game(page);
	case 22: return new MovieGame(page, 35);
	case 23: return new MovieGame(page, 36);
	case 24: return createA14_02Game(page);
	default:
		break;
	}
	error("Vangogh: page %d has no slot handler", pageIndex);
	return nullptr;
}

} // End of namespace Vangogh
