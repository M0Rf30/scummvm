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

#include "vangogh/spr.h"

#include "common/endian.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/util.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"

namespace Vangogh {

namespace {

// RGB565, little-endian: bytesPerPixel=2, 5-6-5 bits, shifts 11/5/0 -- same
// convention TGPDecoder uses (tgp.cpp), and the same on-disk layout raw,
// palette-less SPR cells use for their direct-color pixels.
const Graphics::PixelFormat kRGB565Format(2, 5, 6, 5, 0, 11, 5, 0, 0);

// Codec C's only "no operand" opcode; see spr.h / milestone2/spr-codec.md.
const byte kOpEndOfRow = 0x80;

// Sanity clamp against implausible/corrupt on-disk dimensions (every real
// cell in the shipped corpus is at most 640x480). Mirrors TGPDecoder's own
// kMaxDimension clamp for the same reason: refuse to act on a header value
// that would otherwise drive a huge allocation.
const uint32 kMaxDimension = 4096;

} // end of anonymous namespace

SPRFile::SPRFile() : _flagA(false), _flagB(false) {
}

SPRFile::~SPRFile() {
	destroy();
}

void SPRFile::destroy() {
	_data.clear();
	_palette.clear();
	_cells.clear();
	_flagA = false;
	_flagB = false;
}

bool SPRFile::load(Common::SeekableReadStream &stream) {
	destroy();

	const int64 streamSize = stream.size();
	// Smallest possible file: a 4-byte palette header with no palette
	// (paletteChunkSize==4) followed by a 4-byte cell-offset table with
	// zero cells (cellOffsets[0]==0).
	if (streamSize < 8) {
		warning("Vangogh: SPR stream too small for a header (%d bytes)", (int)streamSize);
		return false;
	}
	const uint32 fileSize = (uint32)streamSize;

	_data.resize(fileSize);
	if (stream.read(_data.data(), fileSize) != fileSize) {
		warning("Vangogh: short read loading SPR data (wanted %u bytes)", fileSize);
		destroy();
		return false;
	}

	const uint32 raw0 = READ_LE_UINT32(_data.data());
	_flagA = (raw0 & 0x80000000) != 0;
	_flagB = (raw0 & 0x40000000) != 0;
	const uint32 paletteChunkSize = raw0 & 0x3FFFFFFF;

	if (paletteChunkSize < 4 || paletteChunkSize + 4 > fileSize) {
		warning("Vangogh: SPR palette chunk size %u is out of range for a %u-byte file", paletteChunkSize, fileSize);
		destroy();
		return false;
	}

	uint32 numColors = (paletteChunkSize == 4) ? 0 : (paletteChunkSize - 4) / 3;
	if (numColors > 256) {
		warning("Vangogh: SPR palette has an implausible %u colors, clamping to 256", numColors);
		numColors = 256;
	}
	if (numColors > 0)
		_palette = Graphics::Palette(_data.data() + 4, numColors);

	// Cell-table chunk: a self-referential offset table starts right here
	// (entry 0 IS the table's own byte length) followed by the cell records
	// themselves -- see spr.h for the full layout.
	const uint32 cellChunkOff = paletteChunkSize;
	if ((uint64)cellChunkOff + 4 > fileSize) {
		warning("Vangogh: SPR file too small for a cell-offset table (chunk at %u, file is %u bytes)", cellChunkOff, fileSize);
		destroy();
		return false;
	}

	const uint32 tableByteSize = READ_LE_UINT32(_data.data() + cellChunkOff); // == cellOffsets[0]
	uint32 numCells = tableByteSize / 4;

	const uint64 tableEnd = (uint64)cellChunkOff + (uint64)numCells * 4;
	if (tableEnd > fileSize) {
		const uint32 maxCells = (fileSize > cellChunkOff) ? (fileSize - cellChunkOff) / 4 : 0;
		warning("Vangogh: SPR cell-offset table claims %u cells but only %u fit in a %u-byte file; truncating",
				numCells, maxCells, fileSize);
		numCells = maxCells;
	}

	_cells.reserve(numCells);
	for (uint32 i = 0; i < numCells; ++i) {
		const uint32 relOffset = READ_LE_UINT32(_data.data() + cellChunkOff + i * 4);
		const uint64 absOffset = (uint64)cellChunkOff + relOffset;
		if (absOffset + 4 > fileSize) {
			warning("Vangogh: SPR cell %u offset %u is out of range, dropping it and the remaining %u cell(s)",
					i, relOffset, numCells - i - 1);
			break;
		}
		CellEntry entry;
		entry.absOffset = (uint32)absOffset;
		_cells.push_back(entry);
	}

	return true;
}

Graphics::Surface *SPRFile::decodeCell(uint32 index) const {
	if (index >= _cells.size()) {
		warning("Vangogh: SPR cell index %u out of range (%u cell(s))", index, (uint32)_cells.size());
		return nullptr;
	}

	Graphics::Surface *surface = new Graphics::Surface();
	const bool ok = _flagA ? decodeRawCell(index, surface) : decodeCodecCCell(index, surface);
	if (!ok) {
		surface->free();
		delete surface;
		return nullptr;
	}
	return surface;
}

bool SPRFile::decodeRawCell(uint32 index, Graphics::Surface *surface) const {
	const uint32 headerOff = _cells[index].absOffset;
	const uint16 w = READ_LE_UINT16(_data.data() + headerOff);
	const uint16 h = READ_LE_UINT16(_data.data() + headerOff + 2);

	const Graphics::PixelFormat format = hasPalette() ? Graphics::PixelFormat::createFormatCLUT8() : kRGB565Format;

	if (w == 0 || h == 0) {
		// Valid empty placeholder cell (see spr.h).
		surface->create(0, 0, format);
		return true;
	}

	if (w > kMaxDimension || h > kMaxDimension) {
		warning("Vangogh: SPR raw cell %u has implausible dimensions %ux%u, refusing to decode", index, w, h);
		return false;
	}

	const uint32 bytesPerPixel = format.bytesPerPixel;
	const uint64 payloadOff = (uint64)headerOff + 4;
	const uint64 payloadSize = (uint64)bytesPerPixel * w * h;
	const uint64 available = (payloadOff < _data.size()) ? (_data.size() - payloadOff) : 0;
	const uint64 toCopy = MIN(payloadSize, available);

	if (toCopy < payloadSize) {
		warning("Vangogh: SPR raw cell %u payload truncated (wanted %u bytes, only %u available), decoding partially",
				index, (uint32)payloadSize, (uint32)toCopy);
	}

	surface->create(w, h, format);
	// A freshly create()d surface's pitch is exactly w*bytesPerPixel (no row
	// padding), matching the raw on-disk layout exactly, so this is a
	// straight one-shot copy -- same trick TGPDecoder uses for its own
	// (always-uncompressed-after-LZSS) RGB565 pixel buffer.
	if (toCopy > 0)
		memcpy(surface->getPixels(), _data.data() + payloadOff, toCopy);
	// Any truncated tail is left at the calloc()-zeroed default (black /
	// index 0), same convention decodeCodecCCell() uses for missing pixels.

	return true;
}

bool SPRFile::decodeCodecCCell(uint32 index, Graphics::Surface *surface) const {
	const uint32 headerOff = _cells[index].absOffset;
	const uint16 headerW = READ_LE_UINT16(_data.data() + headerOff);
	const uint16 headerH = READ_LE_UINT16(_data.data() + headerOff + 2);

	if (headerW == 0 || headerH == 0) {
		// Valid empty placeholder cell (see spr.h).
		surface->create(0, 0, Graphics::PixelFormat::createFormatCLUT8());
		return true;
	}

	if (headerH > kMaxDimension) {
		warning("Vangogh: SPR compressed cell %u has an implausible height %u, refusing to decode", index, headerH);
		return false;
	}

	// Safety bound: the next cell's own on-disk offset (or end of file for
	// the last cell). The codec is self-terminating via exactly `headerH`
	// end-of-row opcodes and needs no length field to decode correctly, but
	// this bound keeps a corrupt/truncated stream from reading past the
	// buffer -- see spr.h / milestone2/spr-codec.md section 3.
	const uint32 limit = (index + 1 < _cells.size()) ? _cells[index + 1].absOffset : (uint32)_data.size();

	Common::Array<Common::Array<byte>> rows;
	rows.reserve(headerH);

	uint32 p = headerOff + 4;
	bool truncated = false;
	for (uint16 row = 0; row < headerH && !truncated; ++row) {
		Common::Array<byte> line;
		for (;;) {
			if (p >= limit) {
				warning("Vangogh: SPR compressed cell %u row %u/%u: opcode stream ran past the next cell's offset (truncated file?)",
						index, row, headerH);
				truncated = true;
				break;
			}
			const byte op = _data[p++];
			if (op == kOpEndOfRow) {
				break;
			} else if (op < kOpEndOfRow) {
				// Transparent skip: advance the column cursor without
				// writing. The bytes are appended as index/color 0 so the
				// row buffer already IS the final pixel row -- see the
				// kTransparentColor doc comment in spr.h.
				for (uint32 i = 0; i < op; ++i)
					line.push_back(0);
			} else {
				const uint32 cnt = op & 0x7F; // 1..127; 0x80 is reserved for end-of-row.
				const uint32 avail = (p < limit) ? MIN((uint64)cnt, (uint64)(limit - p)) : 0;
				for (uint32 i = 0; i < avail; ++i)
					line.push_back(_data[p + i]);
				p += cnt;
				if (avail < cnt) {
					warning("Vangogh: SPR compressed cell %u row %u/%u: literal run of %u bytes overruns the next cell's offset, truncating",
							index, row, headerH, cnt);
					truncated = true;
					break;
				}
			}
		}
		rows.push_back(line);
	}

	if (rows.size() < headerH) {
		warning("Vangogh: SPR compressed cell %u decoded only %u/%u row(s) before truncation",
				index, (uint32)rows.size(), headerH);
	}

	// Row-width caveat: header.w is NOT a reliable predictor of the decoded
	// stride (see spr.h) -- derive it from the bitstream itself. Every row
	// should agree; if a malformed cell disagrees, pad to the widest row
	// rather than lose/truncate any decoded pixel data.
	uint32 decodedW = headerW;
	bool widthMismatch = false;
	for (uint32 i = 0; i < rows.size(); ++i) {
		if (i == 0)
			decodedW = rows[i].size();
		else if (rows[i].size() != decodedW)
			widthMismatch = true;
		if (rows[i].size() > decodedW)
			decodedW = rows[i].size();
	}
	if (widthMismatch)
		warning("Vangogh: SPR compressed cell %u has inconsistent per-row widths, padding to the widest row (%u px)", index, decodedW);
	if (decodedW > kMaxDimension) {
		warning("Vangogh: SPR compressed cell %u decoded to an implausible width %u, clamping to %u", index, decodedW, kMaxDimension);
		decodedW = kMaxDimension;
	}

	surface->create(decodedW, headerH, Graphics::PixelFormat::createFormatCLUT8());
	for (uint32 i = 0; i < rows.size(); ++i) {
		const uint32 rowBytes = MIN((uint32)rows[i].size(), decodedW);
		if (rowBytes > 0)
			memcpy(surface->getBasePtr(0, i), rows[i].data(), rowBytes);
		// Shorter rows, and any row beyond rows.size() (truncated cell),
		// stay at the calloc()-zeroed default: transparent (index 0).
	}

	return true;
}

} // End of namespace Vangogh
