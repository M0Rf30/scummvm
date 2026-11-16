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

#ifndef VANGOGH_ENDING_H
#define VANGOGH_ENDING_H

#include "common/scummsys.h"
#include "graphics/surface.h"

#include "vangogh/clip.h"

namespace Vangogh {

struct WorldInput;

/**
 * The end of the game (flow.md 4.5 and 4.7): the movies `cinefin` and `cinefin2`, then the 16 credit screens
 * with the `Credits` music, then the process quits (PostQuitMessage). A non-blocking state machine stepped once
 * per engine tick, so the headless script harness can drive and capture it like any other mode.
 */
class Ending {
public:
	Ending();
	~Ending();

	/**
	 * Starts the sequence. @p fromFin is the museum's `fin` movie (decomp 17893): that clip is byte-identical to
	 * cinefin2 and, because dat_4f956c is set, ends straight in the credits.
	 */
	void start(bool fromFin);
	/** True from start() on, also once the sequence has finished (the engine then quits). */
	bool active() const { return _state != kIdle; }
	/** The sequence ran to its end (or was skipped) and the game should quit. */
	bool finished() const { return _state == kDone; }

	/** One tick: movies advance with the clock, credits count 40 ms ticks. */
	void step(const WorldInput &in);

	/** The screen to present (movie frame or credit page). */
	const Graphics::Surface &surface() const;

	enum Stage { kStageMovie1, kStageMovie2, kStageCredits, kStageRelease };
	/** Stage and credit page, for the script log. */
	int stage() const { return _state == kMovie ? (_movieIdx == 0 ? kStageMovie1 : kStageMovie2) : (_state == kRelease ? kStageRelease : kStageCredits); }
	int creditPage() const { return _page; }

	/** The exe counts 0xfb ticks of 40 ms per credit page. */
	enum { kCreditPages = 16, kCreditTicks = 0xfb };

private:
	enum State { kIdle, kMovie, kCredits, kRelease, kDone };

	void startMovie(int idx);
	void startCredits();
	bool loadCredit(int page);
	void finish();

	State _state;
	int _movieIdx;
	bool _armed;		///< the mouse button was seen released since the movie started
	int _page, _ticks;
	uint16 _music;
	MovieClip _clip;
	Graphics::Surface _credit;
};

} // End of namespace Vangogh

#endif // VANGOGH_ENDING_H
