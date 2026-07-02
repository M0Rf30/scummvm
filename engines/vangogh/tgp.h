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

#ifndef VANGOGH_TGP_H
#define VANGOGH_TGP_H

#include "common/scummsys.h"
#include "graphics/palette.h"
#include "image/image_decoder.h"

namespace Common {
class SeekableReadStream;
}

namespace Graphics {
struct Surface;
}

namespace Vangogh {

/**
 * Decoder for Cryo's .TGP still-image format, as used for pre-rendered room
 * backgrounds and UI screens in "Mission Sunlight" / "Missione van Gogh"
 * (1998, PEINTRE.exe).
 *
 * Only the "a" sub-format is implemented: a fixed 44-byte header (magic
 * "LZWC" at offset 12, headerSize field of 36 at offset 8) followed by one
 * Cryo-LZSS-compressed (see lzss.h) RGB565 pixel stream, always 640x480 in
 * this title's data. The other on-disk sub-format ("b", chunked, used for
 * oversized ~1500px 3D-scene textures) is not handled here -- see
 * tgp-format.md in the reversing notes for its layout.
 */
class TGPDecoder : public Image::ImageDecoder {
public:
	TGPDecoder();
	~TGPDecoder() override;

	// Image::ImageDecoder API
	void destroy() override;
	bool loadStream(Common::SeekableReadStream &stream) override;
	const Graphics::Surface *getSurface() const override { return _surface; }
	const Graphics::Palette &getPalette() const override { return _palette; }

private:
	Graphics::Surface *_surface;
	Graphics::Palette _palette; // Always empty: RGB565 pixels carry no palette.
};

} // End of namespace Vangogh

#endif // VANGOGH_TGP_H
