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

#include "common/memstream.h"
#include "common/stream.h"
#include "graphics/surface.h"

#include "vangogh/cvy.h"
#include "vangogh/lzss.h"

namespace Vangogh {

enum {
	kMaskWidth = 640,
	kMaskDwords = kMaskWidth / 2
};

CVYFile::CVYFile() {
}

bool CVYFile::load(Common::SeekableReadStream &s) {
	_offsets.clear();
	_blob.clear();
	_raw.clear();

	uint32 count = s.readUint32LE();
	uint32 maxRaw = s.readUint32LE();
	if (s.err() || s.eos() || maxRaw == 0 || (int64)count * 4 > s.size() - 8)
		return false;

	_offsets.resize(count);
	for (uint32 i = 0; i < count; i++)
		_offsets[i] = s.readUint32LE();

	int32 blobSize = (int32)(s.size() - s.pos());
	if (s.err() || blobSize < 0)
		return false;
	_blob.resize(blobSize);
	if (blobSize && s.read(&_blob[0], blobSize) != (uint32)blobSize)
		return false;
	for (uint32 i = 0; i < count; i++) {
		if (_offsets[i] >= (uint32)blobSize)
			return false;
	}

	_raw.resize(maxRaw);
	return true;
}

bool CVYFile::applyMask(uint32 frame, Graphics::Surface &dst, int x, int y, int w, int h, uint32 colour) {
	if (frame >= _offsets.size())
		return false;

	uint32 off = _offsets[frame];
	Common::MemoryReadStream stream(&_blob[off], _blob.size() - off);
	uint32 rawSize = decompressLZSS(stream, &_raw[0], _raw.size());

	const byte *buf = &_raw[0];
	uint32 pos = 0;
	uint32 saved = 0xFFFFFFFF; // position of the latest scanline header
	int row = 0;
	const int bpp = dst.format.bytesPerPixel;
	if (bpp != 2 && bpp != 4)
		return false;

	// Visible area in dst coordinates
	int maxRow = MIN(h, dst.h - y);
	int maxCol = MIN(w, dst.w - x);
	if (x < 0 || y < 0)
		return false;

	while (row < h && pos < rawSize) {
		byte b = buf[pos];
		uint32 runs;
		uint32 runPos;
		if (b == 0) {
			if (saved == 0xFFFFFFFF)
				return false;
			runs = buf[saved] & 0x7F;
			runPos = saved + 1;
			pos++;
		} else if (b < 0x80) {
			row += b;
			pos++;
			continue;
		} else {
			saved = pos;
			runs = b & 0x7F;
			runPos = pos + 1;
			pos = runPos + runs;
		}
		if (pos > rawSize)
			return false;

		if (row < maxRow) {
			byte *line = (byte *)dst.getBasePtr(x, y + row);
			int dword = 0;
			for (uint32 i = 0; i < runs && dword < kMaskDwords; i++) {
				byte r = buf[runPos + i];
				int c = (r & 0x7F) + 1;
				if (r & 0x80) {
					int px = dword * 2;
					int end = MIN(px + c * 2, maxCol);
					if (bpp == 2) {
						uint16 *p = (uint16 *)line;
						for (; px < end; px++)
							p[px] = (uint16)colour;
					} else {
						uint32 *p = (uint32 *)line;
						for (; px < end; px++)
							p[px] = colour;
					}
				}
				dword += c;
			}
			if (dword != kMaskDwords)
				return false;
		}
		row++;
	}
	return true;
}

} // End of namespace Vangogh
