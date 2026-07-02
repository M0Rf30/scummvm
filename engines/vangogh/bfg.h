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

#ifndef VANGOGH_BFG_H
#define VANGOGH_BFG_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

namespace Common {
class SeekableReadStream;
}

namespace Vangogh {

/**
 * Decompresses one scenes_3d/ record's raw byte range (i.e.
 * INCLUDING its own 4-byte sub-header) using "Codec B", a nibble-packed
 * byte-oriented LZSS variant private to the BFG container (PEINTRE.exe
 * fcn.0046de53; NOT shared with .TGP/.CVY's own "Codec A" -- see lzss.h and
 * spr-bfg-cvy.md sec.0/sec.2 for both codecs and how they were told apart).
 * Ported from analyze_bfg.py's bfg_record_decompress():
 *
 * @p src[0]: 1 = payload stored raw (bytes [4:] copied as-is verbatim --
 * the shipped corpus never actually exercises this path, see
 * spr-bfg-cvy.md's open questions); 0 = LZ-compressed, below. src[1..3] are
 * uninitialised MSVC debug-heap filler (0xCD) and carry no information.
 *
 * Compressed body (starting at @p src[4]): a stream of 16-bit LE control
 * words, bit 0 (LSB) first. bit=0: copy one literal byte. bit=1: read 2
 * bytes (A, B); dist = ((A & 0xF0) << 4) | B (12-bit, 4096-byte window);
 * length = (A & 0x0F) + 1 (1..16); copy `length` bytes from
 * output_pos - dist, one byte at a time (dist can be < length, i.e.
 * deliberately overlapping/self-referential runs, so this is NOT a
 * memmove). Decoding stops exactly when the input cursor reaches
 * @p srcSize -- there is no in-stream end marker (unlike Codec A).
 *
 * Unlike the Python reference (which raises on a malformed stream), this
 * stops early with a warning() -- consistent with every other decoder in
 * this engine -- if @p src runs out or a backreference would read before
 * the start of @p dst. This has never been observed on real shipped data
 * (343/343 records validated across all 8 files, see spr-bfg-cvy.md).
 */
void decompressBFGRecord(const byte *src, uint32 srcSize, Common::Array<byte> &dst);

/**
 * Reader for Cryo's scenes_3d/ small archive container ("*.bfg"), as used to
 * bundle a scene's `.3DC` main mesh alongside its `.3DM`/`.3DI`/`.3DA`
 * companion records in "Mission Sunlight" / "Missione van Gogh" (1998,
 * PEINTRE.exe). Reverse-engineered from the decompiled C_Monde::LoadScene
 * (0x00429240), the whole-file loader (0x00431f90), and the by-name record
 * lookup (0x00431b30) -- see spr-bfg-cvy.md sec.2 for the full derivation:
 *
 * @code
 * u32 recordCount
 * recordCount x 36-byte record entries:
 *     28-byte NUL-padded ASCII name (e.g. "eglise.3DC", "BOX.3DI")
 *     u32 dataOffset  -- relative to the fixed base 0xE18
 *     u32 dataSize
 * zero padding out to the fixed offset 0xE18 (a hardcoded capacity
 *     reservation, NOT computed from recordCount -- always exactly 0xE18
 *     in every shipped file)
 * recordCount data blobs, concatenated with no gaps starting at 0xE18,
 *     each is its own decompressBFGRecord() input
 * @endcode
 *
 * This eagerly reads the whole file into memory at load() time, mirroring
 * the original engine's own single-heap-buffer loader.
 */
class BFGFile {
public:
	/**
	 * Reads and parses the record table (does not decompress any record
	 * payload -- see getRecord()). Returns false, after a warning(), if
	 * the stream is too small to even hold a record count, or couldn't be
	 * read in full.
	 */
	bool load(Common::SeekableReadStream &stream);

	/** Frees all state, as if freshly constructed. */
	void destroy();

	uint32 numRecords() const { return _records.size(); }
	bool hasRecord(const Common::String &name) const;

	/**
	 * Case-insensitive lookup by on-disk name (e.g. "BOX.3DI", including
	 * the extension -- that's how records are actually named on disk).
	 * Decompresses the first match into @p out. Returns false, after a
	 * warning(), if no record has that name or its data range doesn't fit
	 * inside the file.
	 */
	bool getRecord(const Common::String &name, Common::Array<byte> &out) const;

private:
	struct RecordEntry {
		Common::String name;
		uint32 dataOffset = 0;
		uint32 dataSize = 0;
	};

	Common::Array<byte> _data;
	Common::Array<RecordEntry> _records;
};

} // End of namespace Vangogh

#endif // VANGOGH_BFG_H
