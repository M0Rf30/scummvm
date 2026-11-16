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

#include "common/file.h"
#include "common/memstream.h"
#include "common/textconsole.h"
#include "graphics/palette.h"
#include "image/tga.h"

#include "vangogh/gamestate.h"
#include "vangogh/inventory.h"
#include "vangogh/lzss.h"
#include "vangogh/spr.h"

namespace Vangogh {

// ---------------------------------------------------------------------------
// Sprites and images.

UISprite::UISprite() : _spr(nullptr), _numCells(0), _mode(kTopLeft), _cacheIdx(-1) {
}

UISprite::~UISprite() {
	clear();
}

void UISprite::clear() {
	delete _spr;
	_spr = nullptr;
	_numCells = 0;
	_cacheIdx = -1;
}

bool UISprite::loadSpr(const Common::String &name) {
	clear();
	Common::File f;
	if (!f.open(Common::Path(Common::String::format("sprites/%s.spr", name.c_str())))) {
		warning("Vangogh: missing sprite %s", name.c_str());
		return false;
	}
	SPRFile *spr = new SPRFile();
	if (!spr->load(f) || spr->numCells() == 0) {
		delete spr;
		return false;
	}
	// Header flags (sub_40b500 keeps them per sprite): bit 31 raw cells, else bit 30 full-width strips.
	f.seek(0);
	const uint32 head = f.readUint32LE();
	_mode = (head & 0x80000000u) ? kTopLeft : ((head & 0x40000000u) ? kStrip : kCentre);
	_spr = spr;
	_numCells = spr->numCells();
	// Strips store their first screen row where raw cells keep the width: read it once per cell.
	_cache.px.clear();
	_stripY.resize(_numCells);
	const uint32 tableBase = head & 0x3fffffff;
	for (uint i = 0; i < _numCells; i++) {
		f.seek(tableBase + 4 * i);
		const uint32 off = f.readUint32LE();
		f.seek(tableBase + off);
		_stripY[i] = f.readUint16LE();
	}
	return true;
}

const UICell &UISprite::cell(uint i) const {
	i = MIN<uint>(i, _numCells - 1);
	if (!_spr || (int)i == _cacheIdx)
		return _cache;
	_cacheIdx = i;
	_cache.w = _cache.h = 0;
	_cache.yOff = _stripY[i];
	Graphics::Surface *s = _spr->decodeCell(i);
	if (!s)
		return _cache;
	_cache.w = s->w;
	_cache.h = s->h;
	_cache.px.resize((uint)s->w * s->h);
	const bool keyed = _spr->isCompressed();
	const Graphics::Palette &pal = _spr->getPalette();
	for (int y = 0; y < s->h; y++) {
		for (int x = 0; x < s->w; x++) {
			uint32 v;
			if (s->format.bytesPerPixel == 1) {
				const byte idx = *((const byte *)s->getBasePtr(x, y));
				byte r = 0, g = 0, b = 0;
				if (idx < pal.size())
					pal.get(idx, r, g, b);
				v = (keyed && idx == SPRFile::kTransparentColor) ? 0 : (0xff000000u | (r << 16) | (g << 8) | b);
			} else {
				byte r, g, b;
				s->format.colorToRGB(*((const uint16 *)s->getBasePtr(x, y)), r, g, b);
				v = 0xff000000u | (r << 16) | (g << 8) | b;
			}
			_cache.px[y * s->w + x] = v;
		}
	}
	s->free();
	delete s;
	return _cache;
}

bool UISprite::loadTga(const Common::String &name) {
	clear();
	Common::File f;
	if (!f.open(Common::Path(Common::String::format("graphs_2d/%s.tga", name.c_str())))) {
		warning("Vangogh: missing image %s.tga", name.c_str());
		return false;
	}
	Image::TGADecoder dec;
	if (!dec.loadStream(f) || !dec.getSurface())
		return false;
	const Graphics::Surface *s = dec.getSurface();
	_mode = kTopLeft;
	_cache.w = s->w;
	_cache.h = s->h;
	_cache.yOff = 0;
	_cache.px.resize((uint)s->w * s->h);
	for (int y = 0; y < s->h; y++) {
		for (int x = 0; x < s->w; x++) {
			byte r, g, b, a = 0xff;
			uint32 raw = s->format.bytesPerPixel == 2 ? *((const uint16 *)s->getBasePtr(x, y)) : *((const uint32 *)s->getBasePtr(x, y));
			s->format.colorToARGB(raw, a, r, g, b);
			// The exe's colour key is RGB555 0x03E0 (pure green), which every 16 bpp conversion maps to g >= 0xf8, r = b = 0.
			const bool key = r == 0 && b == 0 && g >= 0xf8;
			_cache.px[y * s->w + x] = key ? 0 : (0xff000000u | (r << 16) | (g << 8) | b);
		}
	}
	_cacheIdx = 0;
	_numCells = 1;
	return true;
}

static inline void putPixel(Graphics::Surface &dst, int x, int y, uint32 argb) {
	const byte r = (argb >> 16) & 0xff, g = (argb >> 8) & 0xff, b = argb & 0xff;
	const uint32 c = dst.format.RGBToColor(r, g, b);
	if (dst.format.bytesPerPixel == 2)
		*((uint16 *)dst.getBasePtr(x, y)) = (uint16)c;
	else
		*((uint32 *)dst.getBasePtr(x, y)) = c;
}

void UISprite::blit(Graphics::Surface &dst, uint i, int x, int y) const {
	if (!loaded())
		return;
	const UICell &c = cell(i);
	if (_mode == kCentre) {
		x -= c.w >> 1;
		y -= c.h >> 1;
	} else if (_mode == kStrip) {
		x = 0;
		y = c.yOff;
	}
	for (int row = 0; row < c.h; row++) {
		const int dy = y + row;
		if (dy < 0 || dy >= dst.h)
			continue;
		for (int col = 0; col < c.w; col++) {
			const int dx = x + col;
			if (dx < 0 || dx >= dst.w)
				continue;
			const uint32 v = c.px[row * c.w + col];
			if (v >> 24)
				putPixel(dst, dx, dy, v);
		}
	}
}

bool loadTgpImage(const Common::String &name, Graphics::Surface &out) {
	Common::File f;
	if (!f.open(Common::Path(Common::String::format("gfx/%s.TGP", name.c_str())))) {
		warning("Vangogh: missing image %s.TGP", name.c_str());
		return false;
	}
	const uint32 w = f.readUint32LE();
	const uint32 h = f.readUint32LE();
	const uint32 third = f.readUint32LE();
	if (w == 0 || h == 0 || w > 4096 || h > 4096)
		return false;
	out.free();
	out.create(w, h, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));

	if (third == 36) {
		// 'a' image: 44-byte header ("LZWC"), one LZSS stream of w*h RGB565 words.
		f.seek(44);
		decompressLZSS(f, (byte *)out.getPixels(), w * h * 2);
		return true;
	}

	// 'b' image (sub_40ed90): third = chunk count, then per chunk {u32 compressed size, u32 decoded size, LZSS data}.
	byte *dst = (byte *)out.getPixels();
	uint32 filled = 0;
	for (uint32 k = 0; k < third && !f.eos(); k++) {
		const uint32 compSize = f.readUint32LE();
		const uint32 rawSize = f.readUint32LE();
		if (compSize == 0 || compSize > (uint32)f.size() || filled + rawSize > w * h * 2)
			return false;
		byte *buf = (byte *)malloc(compSize);
		if (!buf)
			return false;
		f.read(buf, compSize);
		Common::MemoryReadStream chunk(buf, compSize);
		decompressLZSS(chunk, dst + filled, rawSize);
		free(buf);
		filled += rawSize;
	}
	return filled == w * h * 2;
}

void blitRect(Graphics::Surface &dst, const Graphics::Surface &src, int sx, int sy, int w, int h, int dx, int dy) {
	if (sx < 0) { dx -= sx; w += sx; sx = 0; }
	if (sy < 0) { dy -= sy; h += sy; sy = 0; }
	if (dx < 0) { sx -= dx; w += dx; dx = 0; }
	if (dy < 0) { sy -= dy; h += dy; dy = 0; }
	w = MIN(w, MIN(src.w - sx, dst.w - dx));
	h = MIN(h, MIN(src.h - sy, dst.h - dy));
	if (w <= 0 || h <= 0)
		return;
	for (int row = 0; row < h; row++) {
		const uint16 *s = (const uint16 *)src.getBasePtr(sx, sy + row);
		if (dst.format.bytesPerPixel == 2) {
			memcpy(dst.getBasePtr(dx, dy + row), s, (size_t)w * 2);
		} else {
			uint32 *d = (uint32 *)dst.getBasePtr(dx, dy + row);
			for (int col = 0; col < w; col++) {
				const uint16 p = s[col];
				d[col] = dst.format.RGBToColor(((p >> 11) & 31) << 3 | ((p >> 11) & 31) >> 2,
				                               ((p >> 5) & 63) << 2 | ((p >> 5) & 63) >> 4,
				                               (p & 31) << 3 | (p & 31) >> 2);
			}
		}
	}
}

void blitSurface(Graphics::Surface &dst, const Graphics::Surface &src, int x, int y) {
	blitRect(dst, src, 0, 0, src.w, src.h, x, y);
}

// ---------------------------------------------------------------------------
// The 3D inventory bar.

namespace {

const int kScreenH = 480;
const int kSlideStep = 8;			// dat_4f34dc moves 8 px per frame
const int kVisibleSlots = 6;
// Item centres (dat_4f34e0, six u16): the tile is centred on x and on y = 30 within the bar.
const int kSlotCentre[kVisibleSlots] = { 100, 170, 250, 310, 380, 450 };
const int kArrowLeftX = 12, kArrowRightX = 500, kArrowY = 15, kArrowSize = 32;	// click rects (0xc/500, bar + 0xf, 0x20)
const int kDigitX = 0x22c;			// sunflower counter tile

}

Inventory::Inventory() : _state(kHidden), _barY(kScreenH), _scroll(0), _carried(-1), _ready(false) {
}

bool Inventory::init() {
	if (_ready)
		return true;
	if (!_bar.loadTga("invent"))
		return false;
	for (int i = 0; i < GameState::kNumItems; i++)
		_items[i].loadTga(Common::String::format("op_%d", i));
	for (int i = 0; i < 16; i++)
		_digits[i].loadTga(Common::String::format("ct%d", i));
	_arrowL.loadTga("def_0");
	_arrowR.loadTga("def_1");
	_ready = true;
	reset();
	return true;
}

void Inventory::reset() {
	_state = kHidden;
	_barY = kScreenH;
	_scroll = 0;
	_carried = -1;
}

bool Inventory::inBar(int x, int y) const {
	return _state != kHidden && x >= 0 && x < 640 && y > _barY - 0x1e;
}

int Inventory::count(const GameState &state) const {
	int n = 0;
	for (int i = 0; i < GameState::kNumItems; i++) {
		if (state.item(i) == 1)
			n++;
	}
	return n;
}

int Inventory::itemAt(const GameState &state, int visibleSlot) const {
	// sub_40e120: skip _scroll owned items, then take the next ones.
	int skipped = 0, k = 0;
	for (int i = 0; i < GameState::kNumItems; i++) {
		if (state.item(i) != 1)
			continue;
		if (skipped < _scroll) {
			skipped++;
			continue;
		}
		if (k == visibleSlot)
			return i;
		k++;
	}
	return -1;
}

void Inventory::toggle() {
	if (_carried >= 0)
		return;
	if (_state == kHidden || _state == kSlidingOut)
		_state = kSlidingIn;
	else if (_state == kShown || _state == kSlidingIn)
		_state = kSlidingOut;
}

void Inventory::pickUp(int item) {
	_carried = item;
	if (_state != kShown)
		_state = kSlidingIn;
}

bool Inventory::update(GameState &state, int mx, int my, bool click, int &stored) {
	stored = -1;
	bool consumed = false;
	const int barH = _bar.loaded() ? _bar.height() : 60;

	switch (_state) {
	case kSlidingIn:
		_barY -= kSlideStep;
		if (_barY < kScreenH - barH) {
			_barY = kScreenH - barH;
			_state = kShown;
		}
		break;
	case kSlidingOut:
		_barY += kSlideStep;
		if (_barY >= kScreenH) {
			_barY = kScreenH;
			_state = kHidden;
		}
		break;
	default:
		break;
	}

	if (_state != kShown || !click)
		return false;

	if (_carried >= 0) {
		// Dropped anywhere in the bar zone: stored, the bar closes (sub_40e270).
		if (my >= _barY - 0x1e) {
			state.setItem(_carried, 1);
			if (_carried == 0)
				state.set(kOfsStarDeposited, 1);
			stored = _carried;
			_carried = -1;
			const int n = count(state);
			if (n >= 7)
				_scroll = n - 7;	// the exe adds 0xf9 (-7) to the owned count
			_state = kSlidingOut;
			consumed = true;
		}
		return consumed;
	}

	const int n = count(state);
	if (mx >= kArrowLeftX && mx < kArrowLeftX + kArrowSize && my >= _barY + kArrowY && my < _barY + kArrowY + kArrowSize) {
		if (_scroll > 0)
			_scroll--;
		consumed = true;
	} else if (mx >= kArrowRightX && mx < kArrowRightX + kArrowSize && my >= _barY + kArrowY && my < _barY + kArrowY + kArrowSize) {
		_scroll++;
		if (n < _scroll + kVisibleSlots)
			_scroll--;
		consumed = true;
	}
	return consumed;
}

void Inventory::draw(Graphics::Surface &dst, const GameState &state, int mx, int my) const {
	if (_state != kHidden && _bar.loaded()) {
		_bar.blit(dst, 0, 0, _barY);
		const int sun = MIN<int>(state.getByte(kOfsSunflowers), 15);
		const UISprite &d = _digits[sun];
		if (d.loaded())
			d.blit(dst, 0, kDigitX, _barY + 31 - d.height() / 2);
		for (int k = 0; k < kVisibleSlots; k++) {
			const int item = itemAt(state, k);
			if (item < 0)
				break;
			const UISprite &s = _items[item];
			if (s.loaded())
				s.blit(dst, 0, kSlotCentre[k] - s.width() / 2, _barY + 30 - s.height() / 2);
		}
		if (_carried < 0 && _state == kShown) {
			// The arrow tiles flash while the mouse is on them.
			if (mx >= kArrowLeftX && mx < kArrowLeftX + kArrowSize && my >= _barY + kArrowY && my < _barY + kArrowY + kArrowSize)
				_arrowL.blit(dst, 0, 10, _barY + 13);
			else if (mx >= kArrowRightX && mx < kArrowRightX + kArrowSize && my >= _barY + kArrowY && my < _barY + kArrowY + kArrowSize)
				_arrowR.blit(dst, 0, 0x1f7, _barY + 13);
		}
	}
	if (_carried >= 0 && _carried < GameState::kNumItems) {
		// The carried item is the cursor: its tile centred on the pointer.
		const UISprite &s = _items[_carried];
		if (s.loaded())
			s.blit(dst, 0, mx - s.width() / 2, my - s.height() / 2);
	}
}

} // End of namespace Vangogh
