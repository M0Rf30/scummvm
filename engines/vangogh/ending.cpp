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

#include "vangogh/ending.h"
#include "vangogh/inventory.h"
#include "vangogh/sound.h"
#include "vangogh/vangogh.h"
#include "vangogh/world.h"

#include "common/debug.h"
#include "common/system.h"
#include "common/textconsole.h"

namespace Vangogh {

namespace {

const char *const kMovies[] = { "cinefin", "cinefin2" };

}

Ending::Ending() : _state(kIdle), _movieIdx(0), _armed(false), _page(0), _ticks(0), _music(0) {
}

Ending::~Ending() {
	_credit.free();
}

void Ending::start(bool fromFin) {
	if (_state != kIdle)
		return;
	debug("Vangogh: ending sequence starts%s", fromFin ? " (museum 'fin')" : "");
	// movie_play stops the voice and every sound of the world (decomp 17893 and the page-21 transition).
	if (g_engine && g_engine->_sound)
		g_engine->_sound->stopAll();
	startMovie(fromFin ? 1 : 0);
}

void Ending::startMovie(int idx) {
	_movieIdx = idx;
	_armed = false;
	if (!_clip.open(kMovies[idx])) {
		warning("Vangogh: could not play %s", kMovies[idx]);
		if (idx == 0)
			startMovie(1);
		else
			startCredits();
		return;
	}
	_state = kMovie;
	debug("Vangogh: ending movie %s", kMovies[idx]);
}

void Ending::startCredits() {
	// dat_4fa444 = 0, mode 1, credits_init (0x4079f0): the `Credits` music loops, page counters reset.
	_clip.close();
	_state = kCredits;
	_page = 0;
	_ticks = 0;
	if (g_engine && g_engine->_sound) {
		_music = g_engine->_sound->load("credits");
		if (_music)
			g_engine->_sound->play(_music, true);
	}
	loadCredit(0);
	debug("Vangogh: credits start");
}

bool Ending::loadCredit(int page) {
	// credits_frame state 0: Credit%02u, a full-screen TGP.
	Graphics::Surface img;
	const Common::String name = Common::String::format("credit%02d", page);
	_credit.free();
	_credit.create(640, 480, g_system->getScreenFormat());
	memset(_credit.getPixels(), 0, (size_t)_credit.pitch * _credit.h);
	if (!loadTgpImage(name, img))
		return false;
	Graphics::Surface *conv = img.convertTo(g_system->getScreenFormat());
	if (conv) {
		const int w = MIN(conv->w, _credit.w), h = MIN(conv->h, _credit.h);
		const int dx = (_credit.w - w) / 2, dy = (_credit.h - h) / 2;
		for (int y = 0; y < h; y++)
			memcpy(_credit.getBasePtr(dx, dy + y), conv->getBasePtr(0, y), (size_t)w * _credit.format.bytesPerPixel);
		conv->free();
		delete conv;
	}
	img.free();
	return true;
}

void Ending::finish() {
	if (g_engine && g_engine->_sound && _music)
		g_engine->_sound->stop(_music);
	_state = kDone;
	debug("Vangogh: ending finished, quitting");
}

void Ending::step(const WorldInput &in) {
	switch (_state) {
	case kMovie:
		// movie_play skips on the left button held; the keyboard is ignored (flow.md 5, input table). The button
		// that dismissed page 21 may still be down: it only counts once it was seen released.
		if (!in.mouseDown)
			_armed = true;
		if (in.click || (_armed && in.mouseDown) || _clip.done()) {
			debug("Vangogh: movie %s %s", kMovies[_movieIdx], _clip.done() ? "ended" : "skipped");
			if (_movieIdx == 0)
				startMovie(1);
			else
				startCredits();
			return;
		}
		_clip.step();
		break;

	case kCredits:
		// A click or Space skips ALL the remaining credits (call site 0x4143bd).
		if (in.click || in.space) {
			debug("Vangogh: credits skipped on page %d", _page);
			_state = kRelease;
			return;
		}
		if (++_ticks >= kCreditTicks) {
			_ticks = 0;
			if (++_page >= kCreditPages) {
				_state = kRelease;
				return;
			}
			loadCredit(_page);
		}
		break;

	case kRelease:
		// State 3: wait for the button to be released, then stop the music and quit.
		if (!in.mouseDown)
			finish();
		break;

	default:
		break;
	}
}

const Graphics::Surface &Ending::surface() const {
	return _state == kMovie ? _clip.surface() : _credit;
}

} // End of namespace Vangogh
