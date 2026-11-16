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

#ifndef VANGOGH_PAGEGAMES_H
#define VANGOGH_PAGEGAMES_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"
#include "graphics/surface.h"

#include "vangogh/inventory.h"

namespace Vangogh {

class Page;
struct WorldInput;

/** A hit rectangle in the exe's table layout {x, y, w, h} (sub_412160). */
struct PgRect {
	int x, y, w, h;
	bool has(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

/**
 * A sprite of the exe's sprite layer (sub_40ae70) together with its 0x3c-byte runtime record: the animation cursor
 * of sub_40a5e0 / sub_40a290 and, for the sprites loaded with a background-restore mode (second argument 1 or 2), the
 * "drawn over the background" state. The restore modes keep the drawn cell as an overlay on top of the page background
 * instead of saving and restoring pixels; erase() removes it.
 */
struct PgSprite {
	UISprite spr;
	int mode;			///< sub_40ae70's second argument: 0 plain, 1 one saved background, 2 one saved background per cell
	bool running;		///< record +8: animation active
	int cell;			///< record +4: next cell of the animation
	int delay;			///< record +0xc: ticks the last cell is held (0 = loop forever)
	bool shown;			///< drawn over the background with its background saved
	int shownCell, shownX, shownY;

	PgSprite() : mode(0), running(false), cell(0), delay(0), shown(false), shownCell(0), shownX(0), shownY(0) {}
	bool loaded() const { return spr.loaded(); }
	uint numCells() const { return spr.numCells(); }
	int width(uint c) const { return spr.width(c); }
	int height(uint c) const { return spr.height(c); }
};

/**
 * The per-slot placement handler of a 2D page: the hook triple of the exe's page record (page+0x48 start, +0x4c abort
 * on Backspace, +0x50.. per-slot "run" called every 40 ms tick in state 0xd until it returns true). The exe's own
 * handlers share the page's back buffer; here that buffer is Page::_bg, and the primitives below mirror the exe
 * functions they are named after, so a handler reads like its decompilation.
 */
class PageGame {
public:
	virtual ~PageGame();

	/** State 0xe: hook at page+0x48 (a0 = item, a1 = slot). Called again when a placed capsule is clicked. */
	virtual void start(int item, int slot) = 0;
	/** Backspace pressed while the handler runs: hook at page+0x4c. */
	virtual void onAbort() {}
	/**
	 * State 0xd: one tick. @p tick is dat_52a458 (0..3, steps every 40 ms: animations advance on odd ticks).
	 * @p placed is dat_52a4c0, the exe's "the item stays in its capsule" flag. Returns true when the handler is done.
	 */
	virtual bool run(uint tick, bool &placed) = 0;

	/** The handler for the item of slot @p slot of page @p page. Never null. */
	static PageGame *create(Page &page, int pageIndex, int slot);

	/** Draws the sprites kept over the background and the dragged sprite. */
	void drawOverlays(Graphics::Surface &fb) const;
	/** True once the handler replaced the page background (the loupe icon of the old one is gone). */
	bool bgReloaded() const { return _bgReloaded; }

protected:
	explicit PageGame(Page &page);

	// ---- input (dat_52e2b4/b8 mouse, sub_42d740 left button held)
	int mouseX() const;
	int mouseY() const;
	bool mouseHeld() const;			///< sub_42d740
	void setCursor(int kind);		///< sub_42d3c0(0, kind)

	// ---- sound
	void stream(const char *name);	///< sub_42dac0: plays the voice stream "!name", replacing the running one
	void stopStream();				///< sub_42dc90
	bool streamDone() const;		///< dat_5e4918 (also true when no stream is running)
	uint16 loadSfx(const char *name);	///< sub_42e1c0 (freed with the handler)
	void playSfx(uint16 h, bool loop = false);	///< sub_42e2b0
	void stopSfx(uint16 h);			///< sub_42e300 / sub_42e210

	// ---- the two shared handlers
	bool voiceStep();				///< sub_4177f0 (item voice: ends with the stream or on a held button)
	bool movieStep();				///< sub_4177b0 (item movie: ends with the clip or, if skippable, on a held button)
	void startMovie(int index, int noSkip);	///< sub_417380(index, noSkip): noSkip = the exe's second argument (0 = skippable)
	bool movieDone() const;			///< the clip ended (sub_417520 would return 1)

	// ---- back buffer
	void loadBg(const char *name);	///< sub_416f20: replaces the background, wipes sprites and the last movie frame
	void drawCapsules(int openSlot);///< sub_4130f0: capsules of the page, the slot's capsule drawn open
	void snapshot();				///< sub_47683e: remembers the back buffer
	void restoreFromSnapshot(int x, int y, int w, int h);	///< copies a rect of the snapshot back

	// ---- sprites
	bool loadSprite(PgSprite &s, const char *name, int mode);	///< sub_40ae70
	void blitCell(PgSprite &s, int cell, int x, int y);			///< sub_40a7e0 / sub_40a8f0: drawn into the back buffer for good
	void saveDraw(PgSprite &s, int cell, int x, int y);			///< sub_40aa10 + sub_40a8f0: drawn so erase() can remove it
	void erase(PgSprite &s);									///< sub_40ad00 / sub_40ad70
	void animStart(PgSprite &s, int x, int y, int delay);		///< sub_40a5e0
	bool animStep(PgSprite &s, int x, int y);					///< sub_40a290: true while it drew a cell
	void animStop(PgSprite &s) { s.running = false; }			///< sub_40a170

	// ---- dragged sprite (the exe draws it at the pointer with the grab offset)
	void dragBegin(int mx, int my, int sx, int sy, PgSprite &s, int cell);	///< sub_412300
	void dragEnd();												///< sub_412480
	void dragPos(int &x, int &y) const;							///< sub_4124a0: pointer + grab offset

	Page &_p;

private:
	friend class Page;
	Common::Array<PgSprite *> _sprites;
	Common::Array<uint16> _sfx;
	Graphics::Surface _snap;
	bool _bgReloaded;
	bool _dragOn;
	PgSprite *_dragSpr;
	int _dragCell, _dragOffX, _dragOffY;
	uint16 _voice;
	uint32 _voiceStart;
	const WorldInput *_in;
};

// Bespoke handlers (pagegames_*.cpp), one per item; each returns the handler of that slot.
PageGame *createKettleGame(Page &page);		// page 2 slot 0, item 3 (A01_032)
PageGame *createA03_012Game(Page &page);	// page 4 slot 1, item 6
PageGame *createA03_023Game(Page &page);	// page 7 slot 1, item 12
PageGame *createA13_031Game(Page &page);	// page 8 slot 0, item 13
PageGame *createA13_032Game(Page &page);	// page 8 slot 1, item 14
PageGame *createA13_052Game(Page &page);	// page 10 slot 1, item 17
PageGame *createA03_042Game(Page &page);	// page 13 slot 0, item 21
PageGame *createA13_061Game(Page &page);	// page 15 slot 0, item 24
PageGame *createA03_05Game(Page &page);		// page 16 slot 0, item 26
PageGame *createA03_06Game(Page &page);		// page 18 slot 0, item 28
PageGame *createA14_01Game(Page &page);		// page 20 slot 0, item 31
PageGame *createA14_02Game(Page &page);		// page 24 slot 0, item 34

} // End of namespace Vangogh

#endif // VANGOGH_PAGEGAMES_H
