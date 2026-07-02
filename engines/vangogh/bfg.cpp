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

#include "vangogh/bfg.h"

#include "common/endian.h"
#include "common/stream.h"
#include "common/textconsole.h"

namespace Vangogh {

namespace {

// Fixed offset where the concatenated record data blobs begin, regardless
// of recordCount -- a hardcoded capacity reservation in the authoring tool,
// not a computed value. See bfg.h / spr-bfg-cvy.md sec.2.
const uint32 kDataBase = 0xE18;
// 28-byte name + u32 dataOffset + u32 dataSize.
const uint32 kRecordStride = 36;
const uint32 kNameFieldSize = 28;
const uint32 kTableStart = 4; // Right after the u32 record count.

} // end of anonymous namespace

void decompressBFGRecord(const byte *src, uint32 srcSize, Common::Array<byte> &dst) {
	dst.clear();

	if (srcSize < 4) {
		warning("Vangogh: BFG record blob too small (%u byte(s)) to hold its sub-header", srcSize);
		return;
	}

	if (src[0] == 1) {
		// Stored raw: payload is everything after the 4-byte sub-header.
		// Unobserved in the shipped corpus, but implemented for
		// completeness -- see bfg.h.
		dst.resize(srcSize - 4);
		for (uint32 i = 0; i < srcSize - 4; i++)
			dst[i] = src[4 + i];
		return;
	}

	uint32 pos = 4;
	uint16 ctrl = 0;
	int bitsLeft = 0;
	while (pos < srcSize) {
		if (bitsLeft == 0) {
			if (pos + 2 > srcSize) {
				warning("Vangogh: BFG Codec B stream truncated reading a control word at %u/%u", pos, srcSize);
				break;
			}
			ctrl = READ_LE_UINT16(src + pos);
			pos += 2;
			bitsLeft = 16;
		}

		if ((ctrl & 1) == 0) {
			if (pos + 1 > srcSize) {
				warning("Vangogh: BFG Codec B stream truncated reading a literal at %u/%u", pos, srcSize);
				break;
			}
			dst.push_back(src[pos]);
			pos += 1;
		} else {
			if (pos + 2 > srcSize) {
				warning("Vangogh: BFG Codec B stream truncated reading a match at %u/%u", pos, srcSize);
				break;
			}
			const byte a = src[pos];
			const byte b = src[pos + 1];
			pos += 2;
			const uint32 dist = ((uint32)(a & 0xF0) << 4) | b;
			const uint32 length = (a & 0x0F) + 1;
			if (dist > dst.size()) {
				warning("Vangogh: BFG Codec B backreference underflow (dist=%u, decoded so far=%u)", dist, (uint32)dst.size());
				break;
			}
			// Byte-by-byte, not memcpy/memmove: dist < length is used
			// deliberately for runs, so later bytes may reference bytes
			// this same copy just appended.
			const uint32 start = dst.size() - dist;
			for (uint32 i = 0; i < length; i++)
				dst.push_back(dst[start + i]);
		}

		ctrl >>= 1;
		bitsLeft--;
	}
}

bool BFGFile::load(Common::SeekableReadStream &stream) {
	destroy();

	const int64 streamSize = stream.size();
	if (streamSize < (int64)kTableStart) {
		warning("Vangogh: BFG stream too small (%d byte(s)) to hold a record count", (int)streamSize);
		return false;
	}

	const uint32 fileSize = (uint32)streamSize;
	_data.resize(fileSize);
	if (stream.read(_data.data(), fileSize) != fileSize) {
		warning("Vangogh: failed to read BFG stream in full (%u byte(s) expected)", fileSize);
		destroy();
		return false;
	}

	const uint32 count = READ_LE_UINT32(_data.data());
	_records.reserve(count);
	for (uint32 i = 0; i < count; i++) {
		const uint32 recOff = kTableStart + kRecordStride * i;
		if (recOff + kRecordStride > fileSize) {
			warning("Vangogh: BFG record table entry %u runs past end of file, stopping at %u/%u record(s)", i, i, count);
			break;
		}

		const byte *nameBytes = _data.data() + recOff;
		uint32 nameLen = 0;
		while (nameLen < kNameFieldSize && nameBytes[nameLen] != 0)
			nameLen++;

		RecordEntry entry;
		entry.name = Common::String((const char *)nameBytes, nameLen);
		entry.dataOffset = READ_LE_UINT32(_data.data() + recOff + kNameFieldSize);
		entry.dataSize = READ_LE_UINT32(_data.data() + recOff + kNameFieldSize + 4);
		_records.push_back(entry);
	}

	return true;
}

void BFGFile::destroy() {
	_data.clear();
	_records.clear();
}

bool BFGFile::hasRecord(const Common::String &name) const {
	for (const auto &rec : _records) {
		if (rec.name.equalsIgnoreCase(name))
			return true;
	}
	return false;
}

bool BFGFile::getRecord(const Common::String &name, Common::Array<byte> &out) const {
	for (const auto &rec : _records) {
		if (!rec.name.equalsIgnoreCase(name))
			continue;

		const uint32 absStart = kDataBase + rec.dataOffset;
		const uint32 absEnd = absStart + rec.dataSize;
		if (absStart > absEnd || absEnd > _data.size()) {
			warning("Vangogh: BFG record '%s' data range [%u,%u) does not fit in a %u-byte file",
				name.c_str(), absStart, absEnd, (uint32)_data.size());
			return false;
		}

		decompressBFGRecord(_data.data() + absStart, rec.dataSize, out);
		return true;
	}

	warning("Vangogh: BFG record '%s' not found", name.c_str());
	return false;
}

} // End of namespace Vangogh
