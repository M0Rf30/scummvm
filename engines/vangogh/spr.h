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

#ifndef VANGOGH_SPR_H
#define VANGOGH_SPR_H

#include "common/array.h"
#include "common/scummsys.h"
#include "graphics/palette.h"

namespace Common {
class SeekableReadStream;
}

namespace Graphics {
struct Surface;
}

namespace Vangogh {

/**
 * Decoder for Cryo's .SPR sprite-cell container, as used for character/prop
 * animation cels in "Mission Sunlight" / "Missione van Gogh" (1998,
 * PEINTRE.exe). Reverse-engineered from the decompiled Sprite::Load
 * (0x0040b500) and the compressed-cell blit dispatcher (0x0047751a and two
 * sibling entry points) -- see spr-bfg-cvy.md and milestone2/spr-codec.md
 * in the reversing notes for the full derivation; short version:
 *
 *   SPR file (no magic number):
 *     u32 paletteHeader        -- bit31=flagA (1=cells stored raw, 0=cells
 *                                  are "Codec C" compressed), bit30=flagB
 *                                  (raw-cell 565-vs-555 selection in the
 *                                  original engine; unused by this decoder,
 *                                  which always emits RGB565),
 *                                  bits[29:0]=paletteChunkSize (includes
 *                                  this u32 itself)
 *     paletteChunkSize-4 bytes -- numColors RGB888 triples (0, or up to 256)
 *   cell-table chunk, starting at paletteChunkSize:
 *     numCells x u32 cellOffsets -- self-referential: entry 0 IS the byte
 *                                    length of this table (numCells*4), so
 *                                    numCells = cellOffsets[0]/4; every
 *                                    entry is a byte offset relative to the
 *                                    start of this chunk (i.e. relative to
 *                                    paletteChunkSize), pointing at one
 *                                    cell's [w,h,payload] record
 *     numCells x cell record:
 *       u16 w, u16 h              -- LE. w==h==0 is a valid empty
 *                                     placeholder cell (blank in-between
 *                                     animation frame).
 *       pixel payload             -- raw bpp*w*h bytes (bpp=1 palette-index
 *                                     when the file has a palette, bpp=2
 *                                     RGB565 direct-color otherwise), OR
 *                                     "Codec C" scanline transparent-skip
 *                                     RLE (see decodeCodecCCell() in the
 *                                     .cpp for the opcode stream); either
 *                                     way the whole record (header+payload)
 *                                     is padded to a 4-byte boundary, which
 *                                     is why cell records need no explicit
 *                                     length field: the next cell's own
 *                                     table offset already gives the end.
 *
 * flagA/flagB are whole-file properties: every cell in one SPR is either
 * all-raw or all-Codec-C-compressed. Compressed files always ship with a
 * full (or, per the decompiled loader, at least partial) palette in this
 * corpus. This decoder eagerly reads the whole file into memory at load()
 * time (mirroring Sprite::Load's own "one heap buffer" scheme) but decodes
 * cell pixel data lazily, on demand, via decodeCell() -- the original
 * engine only decompresses at blit time too.
 */
class SPRFile {
public:
	SPRFile();
	~SPRFile();

	/**
	 * Parses the palette chunk and the self-referential cell-offset table.
	 * Does not decode any cell pixel data (see decodeCell()). Returns false,
	 * after a warning(), if the stream is too small to even contain a valid
	 * header/offset table.
	 */
	bool load(Common::SeekableReadStream &stream);

	/** Frees all decoded state, as if freshly constructed. */
	void destroy();

	uint32 numCells() const { return _cells.size(); }

	/** True if this file carries an on-disk RGB888 palette. */
	bool hasPalette() const { return _palette.size() > 0; }

	/** The on-disk RGB888 palette, or an empty (size 0) palette if hasPalette() is false. */
	const Graphics::Palette &getPalette() const { return _palette; }

	/** True if cell pixel data is Codec-C compressed (whole-file flag); false means raw, always-opaque cells. */
	bool isCompressed() const { return !_flagA; }

	/**
	 * Decodes cell @p index into a freshly allocated Graphics::Surface;
	 * the caller takes ownership (Surface::free(), then delete). Returns
	 * nullptr, after a warning(), only when @p index is out of range or the
	 * cell's own 4-byte [w,h] header can't be read (table/file truncated).
	 * Any other structural problem -- a malformed opcode stream, a payload
	 * that runs past the next cell's offset (or end of file), per-row
	 * widths that disagree with each other -- is logged with warning() and
	 * decoded as far as possible: undecodable rows/columns come back fully
	 * transparent (raw cells: zero-filled black/index 0). This never
	 * throws or crashes on malformed input.
	 *
	 * The surface is CLUT8 (see getPalette()) for every Codec-C compressed
	 * cell and every raw cell in a file that has a palette; it is RGB565
	 * (5-6-5, little-endian -- same convention as TGPDecoder) for raw cells
	 * in a palette-less file. Cells with w==0 and h==0 are a valid empty
	 * placeholder and decode to a 0x0 surface.
	 *
	 * Row width caveat (Codec-C cells only): the on-disk header's w field is
	 * NOT a reliable predictor of the decoded row stride -- see
	 * milestone2/spr-codec.md section 2. This decoder derives the true
	 * width from the bitstream itself (every row must sum to the same
	 * total); header.h, by contrast, IS authoritative for the row count.
	 *
	 * Transparency: on-disk pixel data never carries a dedicated alpha
	 * channel. Following the reference spr_codec_c.py PoC's own documented
	 * convention, index/color value kTransparentColor marks "no pixel here"
	 * -- exactly what Codec C's transparent-skip opcode decodes to, since
	 * skipped columns are simply left at the surface's zero-initialized
	 * default. As the PoC notes, this is a genuine, unavoidable ambiguity
	 * for compressed cells (a literal run can legitimately encode a real
	 * palette-index-0 pixel too, indistinguishable from "skipped" once
	 * decoded) -- inherent to a single-channel color-key, not a bug here.
	 * Raw cells (isCompressed() == false) carry no transparency information
	 * at all on disk and always decode fully opaque; callers should blit
	 * them without a color key.
	 */
	Graphics::Surface *decodeCell(uint32 index) const;

	/** Color-key value decodeCell()'s surfaces use for "no pixel here" -- see decodeCell(). */
	static const uint32 kTransparentColor = 0;

private:
	/** One entry of the parsed cell-offset table. */
	struct CellEntry {
		uint32 absOffset; ///< Absolute byte offset of this cell's [u16 w][u16 h] header within _data.
	};

	bool decodeRawCell(uint32 index, Graphics::Surface *surface) const;
	bool decodeCodecCCell(uint32 index, Graphics::Surface *surface) const;

	Common::Array<byte> _data;
	Graphics::Palette _palette;
	bool _flagA; ///< 1 = cells stored raw; 0 = Codec-C compressed.
	bool _flagB; ///< Raw-cell 565-vs-555 selection in the original engine; unused here.
	Common::Array<CellEntry> _cells;
};

} // End of namespace Vangogh

#endif // VANGOGH_SPR_H
