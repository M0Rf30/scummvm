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

#include "vangogh/tgp.h"
#include "vangogh/lzss.h"

#include "common/stream.h"
#include "common/textconsole.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"

namespace Vangogh {

namespace {

// "a"-type header: 44 bytes total, all fields little-endian, immediately
// followed by the compressed pixel stream. See tgp-format.md for the full
// field-by-field derivation.
const uint32 kHeaderSize = 44;
const uint32 kHeaderSizeFieldValue = 36; // Value of the offset-8 "headerSize" field for "a"-type files.
// Sanity clamp for width/height read from a (possibly corrupt) header;
// every real "a"-type sample is exactly 640x480.
const uint32 kMaxDimension = 4096;

} // end of anonymous namespace

TGPDecoder::TGPDecoder() : _surface(nullptr) {
}

TGPDecoder::~TGPDecoder() {
	destroy();
}

void TGPDecoder::destroy() {
	if (_surface) {
		_surface->free();
		delete _surface;
		_surface = nullptr;
	}
	_palette.clear();
}

bool TGPDecoder::loadStream(Common::SeekableReadStream &stream) {
	destroy();

	if (stream.size() < (int64)kHeaderSize) {
		warning("Vangogh: TGP stream too small for a header (%d bytes)", (int)stream.size());
		return false;
	}

	const uint32 width = stream.readUint32LE();
	const uint32 height = stream.readUint32LE();
	const uint32 headerSizeField = stream.readUint32LE();

	byte magic[4];
	stream.read(magic, sizeof(magic));

	if (headerSizeField != kHeaderSizeFieldValue || memcmp(magic, "LZWC", 4) != 0) {
		warning("Vangogh: not an 'a'-type TGP (headerSize=%u, magic='%c%c%c%c') - chunked 'b'-type images are not supported",
				headerSizeField, magic[0], magic[1], magic[2], magic[3]);
		return false;
	}

	// Offsets 16-39: constant tag "RYO\0" plus three fields of unconfirmed
	// purpose (0, 0, 256, 1 in every known sample). The original loader
	// never validates them either -- skip straight past to decodedSize.
	stream.skip(20);

	const uint32 decodedSize = stream.readUint32LE();
	// compressedSize (next u32) is informational only: 7 shipped files are
	// short by a handful of bytes at the very end, so it is never used to
	// bound the read -- decompressLZSS() decodes to EOF/end-marker instead.
	stream.skip(4);

	if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension) {
		warning("Vangogh: implausible TGP dimensions %ux%u", width, height);
		return false;
	}

	if (decodedSize != width * height * 2) {
		warning("Vangogh: TGP decodedSize %u does not match %ux%u RGB565 (expected %u) - header looks corrupt",
				decodedSize, width, height, width * height * 2);
		return false;
	}

	_surface = new Graphics::Surface();
	// RGB565, little-endian: bytesPerPixel=2, 5-6-5 bits, shifts 11/5/0.
	_surface->create(width, height, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));

	// decodedSize == width*height*2 == pitch*height exactly (no row padding
	// for a freshly create()d surface), so we can decode straight into the
	// surface's own pixel buffer instead of bouncing through a temporary.
	decompressLZSS(stream, (byte *)_surface->getPixels(), decodedSize);
	// Any undecoded tail from a truncated stream is left as-is: create()
	// calloc()s the pixel buffer, so it already reads as black rather than
	// uninitialized memory.

	return true;
}

} // End of namespace Vangogh
