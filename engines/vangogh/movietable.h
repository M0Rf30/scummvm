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

#ifndef VANGOGH_MOVIETABLE_H
#define VANGOGH_MOVIETABLE_H

#include "common/scummsys.h"
#include "common/str.h"

namespace Vangogh {

/**
 * One row of PEINTRE.exe's static movie table (.data at VA 0x4f4840, stride
 * 0x24): the name of an in-game HNM clip, whether a same-named .cvy stencil
 * accompanies it, and the destination rectangle on the 640x480 screen. The
 * clip's top-left w x h region is blitted at (x, y).
 */
struct MovieEntry {
	char name[13];
	bool hasCvy;
	int16 x, y, w, h;
};

/**
 * Looks up @p name (without extension, case-insensitive) in the movie table.
 * Returns nullptr if the clip isn't listed (arrival/departure clips are
 * full-screen and have no stencil).
 */
const MovieEntry *findMovie(const Common::String &name);

/** The exe's movie index (sub_417380's first argument, 0..0x43), or nullptr if out of range. */
const MovieEntry *movieByIndex(uint index);

} // End of namespace Vangogh

#endif // VANGOGH_MOVIETABLE_H
