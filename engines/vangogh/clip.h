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

#ifndef VANGOGH_CLIP_H
#define VANGOGH_CLIP_H

#include "common/str.h"
#include "graphics/surface.h"

#include "vangogh/cvy.h"
#include "vangogh/hnmplayer.h"

namespace Vangogh {

/**
 * Plays one of the data/movies/<name>.hnm clips, composed on a persistent
 * 640x480 surface in the screen format. Clips listed in the movie table are
 * blitted from their HNM frame's top-left region to the table's rectangle,
 * with the optional .cvy stencil painted over it; other clips (arrival and
 * departure sequences) are full-frame with no stencil.
 */
class MovieClip {
public:
	MovieClip();
	~MovieClip();

	MovieClip(const MovieClip &) = delete;
	MovieClip &operator=(const MovieClip &) = delete;

	/** Opens @p name (no extension), clearing surface() to black. */
	bool open(const Common::String &name);
	void close();

	/** Decodes/composes the next frame if due; true if surface() changed. */
	bool step();

	/** True when nothing is open or the clip has played out. */
	bool done() const;

	const Graphics::Surface &surface() const { return _surface; }

	/** 0-based index of the last composed frame, -1 before the first. */
	int frame() const { return _hnm.getCurFrame(); }

	bool hasMask() const { return _hasMask; }

private:
	HNMPlayer _hnm;
	CVYFile _cvy;
	Graphics::Surface _surface;
	bool _hasMask;
	int _x, _y, _w, _h;
	uint32 _maskColour;
};

} // End of namespace Vangogh

#endif // VANGOGH_CLIP_H
