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

#ifndef VANGOGH_CVY_H
#define VANGOGH_CVY_H

#include "common/array.h"
#include "common/scummsys.h"

namespace Common {
class SeekableReadStream;
}

namespace Graphics {
struct Surface;
}

namespace Vangogh {

/**
 * A .CVY file: per-frame 1-bit stencils overlaid on an HNM clip's rectangle
 * (interface-coloured shapes). Layout: u32 frame count, u32 maximum raw frame
 * size, u32 offsets[count] into the LZSS blob that follows. Each frame is a
 * Codec A (see lzss.h) stream decoding to a run-length coded mask over
 * 640-pixel-wide scanlines (320 dwords of 2 pixels each):
 *   - 0x01..0x7F: skip that many whole scanlines;
 *   - 0x80|n: header of one scanline followed by n run bytes; a run byte with
 *     bit 7 set fills (r & 0x7F) + 1 dwords, otherwise skips r + 1 dwords;
 *   - 0x00: repeat the run list of the latest header for one more scanline.
 */
class CVYFile {
public:
	CVYFile();

	/** Reads the whole file; the raw-frame buffer is allocated here once. */
	bool load(Common::SeekableReadStream &s);

	uint32 numFrames() const { return _offsets.size(); }

	/**
	 * Decodes the stencil of @p frame and paints @p colour (in dst's pixel
	 * format) where it is set. The mask is relative to the (x, y) origin of
	 * a w x h rectangle of @p dst; the stream is clipped to that rectangle
	 * and to dst. Returns false, painting nothing, if @p frame is out of range
	 * or the stream is malformed.
	 */
	bool applyMask(uint32 frame, Graphics::Surface &dst, int x, int y, int w, int h, uint32 colour);

private:
	Common::Array<uint32> _offsets;
	Common::Array<byte> _blob;
	Common::Array<byte> _raw;
};

} // End of namespace Vangogh

#endif // VANGOGH_CVY_H
