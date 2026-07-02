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

#include "vangogh/lzss.h"

#include "common/endian.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/util.h"

namespace Vangogh {

namespace {

/**
 * MSB-first bit reader over successive little-endian 32-bit words, matching
 * the x86 `stc; adc reg,reg` idiom fcn.00434070 uses to fold a "1" sentinel
 * into each freshly-loaded word: the register naturally reads back as zero
 * once all 32 real bits and the sentinel have been shifted out, which is
 * what signals "reload".
 */
class BitReader {
public:
	explicit BitReader(Common::ReadStream &src) : _src(src), _bitBuf(0), _haveBits(false) {}

	/** Returns 0 or 1, or -1 if the stream ran out before a whole word/bit could be read. */
	int getBit() {
		if (!_haveBits) {
			_haveBits = true;
			return reload();
		}
		const uint32 carry = (_bitBuf >> 31) & 1;
		_bitBuf <<= 1;
		if (_bitBuf == 0)
			return reload();
		return (int)carry;
	}

private:
	int reload() {
		byte buf[4];
		if (_src.read(buf, 4) != 4)
			return -1;
		const uint32 word = READ_LE_UINT32(buf);
		const uint32 carry = (word >> 31) & 1;
		_bitBuf = (word << 1) | 1;
		return (int)carry;
	}

	Common::ReadStream &_src;
	uint32 _bitBuf;
	bool _haveBits;
};

} // end of anonymous namespace

uint32 decompressLZSS(Common::ReadStream &src, byte *dst, uint32 dstSize) {
	BitReader bits(src);
	uint32 outPos = 0;
	bool truncated = false;

	while (outPos < dstSize) {
		const int bit = bits.getBit();
		if (bit < 0) {
			truncated = true;
			break;
		}

		if (bit == 1) {
			// Literal byte: costs its own flag bit, no run-length batching.
			byte b;
			if (src.read(&b, 1) != 1) {
				truncated = true;
				break;
			}
			dst[outPos++] = b;
			continue;
		}

		// Match: one more control bit picks the "long" or "short" form.
		const int bit2 = bits.getBit();
		if (bit2 < 0) {
			truncated = true;
			break;
		}

		uint32 distance, length;
		if (bit2 == 1) {
			// "Long" match: u16 LE word packs a 13-bit distance code and a
			// 3-bit length code. A length code of 0 means the real length
			// is in one more byte, where the value 0 is the explicit
			// end-of-stream marker.
			byte buf[2];
			if (src.read(buf, 2) != 2) {
				truncated = true;
				break;
			}
			const uint16 w = READ_LE_UINT16(buf);
			distance = 8192 - (w >> 3);
			const uint32 lencode = w & 7;
			if (lencode != 0) {
				length = lencode + 2;
			} else {
				byte b;
				if (src.read(&b, 1) != 1) {
					truncated = true;
					break;
				}
				if (b == 0)
					break; // Explicit end-of-stream marker.
				length = (uint32)b + 2;
			}
		} else {
			// "Short" match: 2 more control bits form a 0..3 length code,
			// followed by a 1-byte distance (1..256).
			const int a = bits.getBit();
			const int c = bits.getBit();
			if (a < 0 || c < 0) {
				truncated = true;
				break;
			}
			const uint32 lencode = ((uint32)a << 1) | (uint32)c;
			byte b;
			if (src.read(&b, 1) != 1) {
				truncated = true;
				break;
			}
			distance = 256 - b;
			length = lencode + 2;
		}

		if (distance > outPos) {
			warning("Vangogh: LZSS back-reference distance %u exceeds decoded output so far (%u bytes) - stream is corrupt, stopping early",
					distance, outPos);
			truncated = true;
			break;
		}

		// Byte-by-byte copy (not memcpy/memmove): overlapping back-references
		// (distance < length) are used deliberately to encode short runs.
		length = MIN(length, dstSize - outPos);
		for (uint32 i = 0; i < length; ++i, ++outPos)
			dst[outPos] = dst[outPos - distance];
	}

	if (truncated) {
		warning("Vangogh: LZSS stream ran out of input after %u/%u bytes decoded - source data is truncated",
				outPos, dstSize);
	}

	return outPos;
}

} // End of namespace Vangogh
