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

#ifndef VANGOGH_INVENTORY_H
#define VANGOGH_INVENTORY_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"
#include "graphics/surface.h"

namespace Vangogh {

class GameState;
class SPRFile;

/** One decoded sprite cell as 0xAARRGGBB (alpha is 0 or 0xff: the exe's colour key). */
struct UICell {
	int w, h;
	int yOff;			///< strip sprites: the screen row of the first scanline (the cell header's "width" field)
	Common::Array<uint32> px;
	UICell() : w(0), h(0), yOff(0) {}
};

/**
 * A 2D UI graphic of the exe's sprite layer (sub_40ae70): an SPR (all cells decoded once at load) or a TGA tile
 * (one cell; the exe's colour key 0x03E0 is transparent). Blits clip to the destination and key out transparent pixels.
 */
class UISprite {
public:
	/**
	 * Placement rule of the exe's sprite layer (sub_40aa10/sub_40a7e0), chosen by the SPR header flags: raw cells are
	 * drawn with their top-left corner at (x, y); Codec-C cells are centred on (x, y); Codec-C cells with header bit 30
	 * are full-width strips that ignore x and start at the screen row stored in the cell header.
	 */
	enum Mode { kTopLeft, kCentre, kStrip };

	UISprite();
	~UISprite();
	UISprite(const UISprite &) = delete;
	UISprite &operator=(const UISprite &) = delete;

	/** data/sprites/<name>.spr; Codec-C cells use palette index 0 as transparent, raw cells are opaque. Cells decode on first use. */
	bool loadSpr(const Common::String &name);
	/** data/graphs_2d/<name>.tga as a single cell. */
	bool loadTga(const Common::String &name);
	void clear();

	bool loaded() const { return _numCells != 0; }
	uint numCells() const { return _numCells; }
	int width(uint i = 0) const { return loaded() ? cell(i).w : 0; }
	int height(uint i = 0) const { return loaded() ? cell(i).h : 0; }
	Mode mode() const { return _mode; }
	/** Cell @p i as width*height 0xAARRGGBB pixels (alpha 0 = transparent). */
	const uint32 *argb(uint i = 0) const { return cell(i).px.begin(); }

	/** Draws cell @p i according to the sprite's placement mode (see Mode). */
	void blit(Graphics::Surface &dst, uint i, int x, int y) const;

private:
	const UICell &cell(uint i) const;

	SPRFile *_spr;
	uint _numCells;
	Mode _mode;
	mutable UICell _cache;		///< the most recently decoded cell (page animations walk one sprite's cells in order)
	mutable int _cacheIdx;
	Common::Array<uint16> _stripY;
};

/** data/gfx/<name>.TGP: 'a' images (640x480) and chunked 'b' images (the page zoom views, up to 1500 px) as RGB565. */
bool loadTgpImage(const Common::String &name, Graphics::Surface &out);

/** Copies @p src into the RGB565 or 32bpp surface @p dst at (x, y), clipped. @p src is RGB565. */
void blitSurface(Graphics::Surface &dst, const Graphics::Surface &src, int x, int y);

/** Draws the rectangle (sx, sy, w, h) of @p src (RGB565) at (dx, dy) of @p dst, clipped. */
void blitRect(Graphics::Surface &dst, const Graphics::Surface &src, int sx, int sy, int w, int h, int dx, int dy);

/**
 * The 3D world's inventory bar (hud_draw sub_40e270, world.md 6.5): a 60 px strip that slides up from the bottom
 * (8 px per frame), shows up to 6 of the owned items (op_<n>.tga tiles centred on fixed x positions) with scroll
 * arrows, and the sunflower counter tile. A picked-up item becomes the cursor (carried) and is stored by clicking
 * inside the bar. The item flags live in the game state block (dat_699540 = GameState::item).
 */
class Inventory {
public:
	enum State { kHidden = 0, kShown = 1, kSlidingIn = 2, kSlidingOut = 3 };

	Inventory();

	/** Loads the tiles; false (with a warning) if invent.tga is missing. */
	bool init();
	void reset();

	State state() const { return _state; }
	bool visible() const { return _state != kHidden; }
	int carried() const { return _carried; }
	/** Bar top edge (480 = hidden). */
	int barY() const { return _barY; }
	/** True when (x, y) is inside the area where a click belongs to the bar (the exe: y > barY - 0x1e). */
	bool inBar(int x, int y) const;

	/** Space: slides the bar in, or out when it is shown and nothing is carried. */
	void toggle();
	/** The pickup protocol: the item becomes the cursor and the bar slides in. */
	void pickUp(int item);

	/**
	 * One frame (sub_40e270's logic half). Returns true when the click was consumed by the bar. When the carried item
	 * is dropped into the bar, @p stored receives its id (else -1) and the bar starts sliding out.
	 */
	bool update(GameState &state, int mouseX, int mouseY, bool click, int &stored);

	/** Draws the bar, its items and the carried item into @p dst (RGB565 or 32bpp). */
	void draw(Graphics::Surface &dst, const GameState &state, int mouseX, int mouseY) const;

	/** Number of items in the bar (owned and not placed). */
	int count(const GameState &state) const;

private:
	int itemAt(const GameState &state, int visibleSlot) const;

	UISprite _bar;
	UISprite _items[35];
	UISprite _digits[16];
	UISprite _arrowL, _arrowR;
	State _state;
	int _barY;
	int _scroll;
	int _carried;
	bool _ready;
};

} // End of namespace Vangogh

#endif // VANGOGH_INVENTORY_H
