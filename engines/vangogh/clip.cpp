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

#include "common/archive.h"
#include "common/stream.h"
#include "common/system.h"

#include "vangogh/clip.h"
#include "vangogh/movietable.h"

namespace Vangogh {

enum {
	kScreenW = 640,
	kScreenH = 480
};

// Dark navy interface colour, RGB565 0x116A
enum {
	kInterfaceR5 = 0x02,
	kInterfaceG6 = 0x0B,
	kInterfaceB5 = 0x0A
};

MovieClip::MovieClip() : _hasMask(false), _x(0), _y(0), _w(0), _h(0), _maskColour(0) {
}

MovieClip::~MovieClip() {
	close();
}

void MovieClip::close() {
	_hnm.close();
	_surface.free();
	_hasMask = false;
}

bool MovieClip::open(const Common::String &name) {
	close();

	if (!_hnm.load(Common::Path("movies/" + name + ".hnm"), false))
		return false;

	const Graphics::PixelFormat format = g_system->getScreenFormat();
	_surface.create(kScreenW, kScreenH, format);
	memset(_surface.getPixels(), 0, (size_t)_surface.pitch * _surface.h);

	_x = _y = 0;
	_w = MIN<int>(_hnm.width(), kScreenW);
	_h = MIN<int>(_hnm.height(), kScreenH);

	const MovieEntry *e = findMovie(name);
	if (e) {
		_x = e->x;
		_y = e->y;
		_w = MIN<int>(e->w, MIN<int>(_hnm.width(), kScreenW - _x));
		_h = MIN<int>(e->h, MIN<int>(_hnm.height(), kScreenH - _y));
		if (e->hasCvy) {
			Common::SeekableReadStream *s = SearchMan.createReadStreamForMember(Common::Path("movies/" + name + ".cvy"));
			if (s) {
				_hasMask = _cvy.load(*s);
				delete s;
			}
			if (!_hasMask)
				warning("Vangogh: cannot load stencil for movie '%s'", name.c_str());
		}
	}
	_maskColour = format.RGBToColor((kInterfaceR5 << 3) | (kInterfaceR5 >> 2),
	                                (kInterfaceG6 << 2) | (kInterfaceG6 >> 4),
	                                (kInterfaceB5 << 3) | (kInterfaceB5 >> 2));
	return true;
}

bool MovieClip::step() {
	if (!_hnm.isLoaded() || !_hnm.decodeNextFrame())
		return false;

	const Graphics::Surface *src = _hnm.currentSurface();
	if (!src || src->format.bytesPerPixel != _surface.format.bytesPerPixel)
		return false;

	const int bpp = _surface.format.bytesPerPixel;
	const int w = MIN<int>(_w, src->w);
	const int h = MIN<int>(_h, src->h);
	for (int row = 0; row < h; row++)
		memcpy(_surface.getBasePtr(_x, _y + row), src->getBasePtr(0, row), (size_t)w * bpp);

	if (_hasMask)
		_cvy.applyMask(_hnm.getCurFrame(), _surface, _x, _y, _w, _h, _maskColour);
	return true;
}

bool MovieClip::done() const {
	return _hnm.endOfVideo();
}

} // End of namespace Vangogh
