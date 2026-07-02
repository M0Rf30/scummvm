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

#ifndef VANGOGH_LZSS_H
#define VANGOGH_LZSS_H

#include "common/scummsys.h"

namespace Common {
class ReadStream;
}

namespace Vangogh {

/**
 * Decompresses a Cryo LZSS-compressed stream ("Codec A") into @p dst.
 *
 * This is the shared LZSS variant used by both .TGP still images and .CVY
 * movie chunks in "Mission Sunlight" / "Missione van Gogh" (fcn.00434070 in
 * PEINTRE.exe): control bits are read MSB-first out of successive
 * little-endian 32-bit words, a "1" sentinel bit folded into the bottom of
 * each freshly-loaded word so the shift register reads back to exactly zero
 * once every real bit (and the sentinel) has been consumed -- that all-zero
 * state is the "load the next word" signal, avoiding a separate bit
 * counter. Each control bit then selects:
 *   - a literal byte (bit == 1); or
 *   - a "short" match (next bit == 0): 2 more control bits form a 0..3
 *     length code, followed by a distance byte (1..256); or
 *   - a "long" match (next bit == 1): a little-endian u16 packs a 13-bit
 *     distance (1..8192) and a 3-bit length code; a length code of 0 means
 *     the real length follows in one more byte, where the value 0 is the
 *     explicit end-of-stream marker.
 * Matches copy byte-by-byte (not memcpy/memmove) since overlapping
 * back-references (distance < length) are used deliberately for runs.
 *
 * See tgp-format.md in the reversing notes for the full derivation and
 * cross-validation against the disassembly.
 *
 * At most @p dstSize bytes are written. Decoding also stops -- with a
 * warning(), never a crash or an exception -- if @p src runs out before the
 * explicit end marker is seen; a handful of shipped .TGP files are
 * truncated by 1-16 bytes right at the end of their compressed stream.
 *
 * @return The number of bytes actually written to dst (<= dstSize).
 */
uint32 decompressLZSS(Common::ReadStream &src, byte *dst, uint32 dstSize);

} // End of namespace Vangogh

#endif // VANGOGH_LZSS_H
