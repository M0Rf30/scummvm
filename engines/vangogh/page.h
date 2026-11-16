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

#ifndef VANGOGH_PAGE_H
#define VANGOGH_PAGE_H

#include "common/scummsys.h"
#include "common/str.h"
#include "graphics/surface.h"

#include "vangogh/clip.h"
#include "vangogh/inventory.h"
#include "vangogh/gamestate.h"

namespace Vangogh {

class Sound;
struct WorldInput;

/**
 * Mode 1: the 2D painting page (flow.md 5.3, wndproc_2d sub_414080). A page shows one painting's scene, a left column
 * of up to three capsules and the inventory bar. Items from the bar are dragged into their capsule; a valid drop plays
 * the item's movie, completes the page when every item is in, and finally offers the sunflower, which is dragged to
 * the counter at the bottom right. Everything is composed into a 640x480 surface at the engine's 40 ms page tick.
 *
 * The 25 pages share this engine; pages whose own handlers run a bespoke mini game (the 5-step kettle of page 2, the
 * briket of page 4, ...) use the same drag / movie / complete path, see the notes in page.cpp.
 */
class Page {
public:
	/** Exit targets of transition_commit. */
	enum { kExitResume = -1, kExitMuseum = -3 };
	/** Number of pages (puzzle descriptors). */
	enum { kNumPages = 25 };

	Page();
	~Page();

	/** The page launched by clicking object @p name of world @p scene, or -1. */
	static int launcherPage(int scene, const char *name);
	/** Descriptor name of page @p page for logs ("A01_02"), or an empty string. */
	static const char *pageName(int page);

	/** Starts page @p page. @p sound may be null. Returns false if its assets are missing. */
	bool open(int page, GameState &state, Sound *sound);
	/** Frees the page assets (the exit target stays readable). */
	void close();

	bool active() const { return _active; }
	/** True once the player left the page (Backspace, a chrome icon or the ending). */
	bool finished() const { return _active && _exit != 0; }
	/** kExitResume or kExitMuseum once finished(). */
	int exitTarget() const { return _exit; }
	int pageIndex() const { return _page; }
	/** The ending page (21) was left: the game is won. */
	bool endingReached() const { return _ending; }

	/** One 40 ms tick: input, animation, composition. */
	void tick(const WorldInput &in);
	const Graphics::Surface &surface() const { return _fb; }

	/** Cursor kind to show (curseurs.spr kinds: 0 arrow, 1..8 scroll edges, 9 busy, 11 finger, 12 grab, 13 drag), or -1 to hide it. */
	int cursorKind() const { return _cursor; }
	/** True once after Esc was pressed. */
	bool takeMenuRequest();

	/** Test hooks for the headless harness. */
	int slotOfItem(int item) const;
	const char *stateName() const;
	bool barVisible() const { return _barState != kBarHidden; }
	int dragItem() const { return _dragItem; }

private:
	enum State {
		kIdle,
		kDrag,			///< dragging a bar item (an item of another page is dragged the same way and always rejected)
		kReject,		///< item flying back to the bar
		kPlaceOpen,		///< CapsOP animation
		kPlayback,		///< the item's movie or voice
		kPlaceClose,	///< CapsAC animation
		kClickOpen,		///< CapsAO animation after clicking a placed item
		kComplete,		///< PA<n>a animation
		kSunReady,		///< sunflower on offer
		kSunDrag,
		kPot,			///< POT animation at the counter
		kAfter,			///< PA<n>c animation
		kZoom,			///< scroller over the big "b" image
		kPaint,			///< painting animation
		kP0Loupe,		///< page 0: bg b with the loupe, wait for a click or time out
		kP0Hold,		///< page 0: bg d, 40 ticks
		kEndWait,		///< page 21: click rect before the last movie
		kEndHold,
		kDone
	};
	enum BarState { kBarHidden, kBarIn, kBarShown, kBarOut };
	enum Loupe { kLoupeAppearing, kLoupeVisible, kLoupeHidden };
	enum Next { kNextNone, kNextClose, kNextP0Loupe, kNextP0Sun, kNextP0Hold, kNextEndWait, kNextEndHold };

	/** A sprite animation: cell advances every other tick. */
	struct Anim {
		const UISprite *spr;
		int cell;
		int x, y;
		bool active;
		Anim() : spr(nullptr), cell(0), x(0), y(0), active(false) {}
	};

	void buildBar();
	void removeFromBar(int index);
	void insertInBar(int item, int index);
	int barSlotAt(int x, int y) const;

	void loadBackground(const Common::String &name);
	void startAnim(Anim &a, const UISprite *spr, int x, int y);
	bool stepAnim(Anim &a);

	void play(const char *name);
	void playItemSound(int item);
	void startMovie(int index);
	void autosave();

	void placeItem(int slot);
	void startPlayback();
	void endPlayback();
	void afterPlayback();
	void finishPlacement();
	void beginSunflower();
	void dropSunflower(bool inCounter);
	void finishCounter();
	void commitAndExit(int target);

	void tickIdle(const WorldInput &in);
	void tickDrag(const WorldInput &in);
	void tickSunflower(const WorldInput &in);
	void tickScroller(const WorldInput &in);
	void beginDrag(int barIndex, bool held);
	void enterZoom();
	void enterPaint();
	void leaveView();

	void compose();
	void drawCapsules();
	void drawBar();
	void setState(State s);

	GameState *_st;
	Sound *_sound;
	bool _active;
	int _page;
	int _exit;
	bool _ending;
	bool _menuRequest;
	State _state;
	State _returnState;			///< state to go back to after a view or a rejected drag
	Next _next;					///< what follows the running movie
	uint32 _tick;
	uint32 _stateTicks;

	Graphics::Surface _fb;
	Graphics::Surface _bg;		///< current background (640x480)
	Graphics::Surface _big;		///< the "b" zoom image while scrolling
	Common::String _bgName;
	int _scrollX, _scrollY;
	int _kx, _ky;				///< arrow-key scroll speed

	UISprite _invent, _opi, _op, _count, _loupeOut, _retour, _retourM, _pot, _tourn;
	UISprite _capsE[3], _capsOP[3], _capsAO[3], _capsAC[3];
	UISprite _paA, _paB, _paC, _paint;	///< PA<n>a growing, b empty, c after; the painting animation
	UISprite _p0;				///< page 0's sunflower scene (A14_031a)

	MovieClip _clip;
	int _movieIndex;
	uint16 _voice;

	Anim _anim;					///< transient animation (capsule, POT, painting)
	int _animSlot;				///< capsule slot of _anim, or -1
	Anim _layer;				///< strip drawn over the background (PA animations)
	Anim _loupeAnim;
	int _p0Cell;				///< page 0 sunflower scene cell, -1 = hidden

	BarState _barState;
	int _barY;
	int _barList[GameState::kNumItems];
	int _barCount;
	int _barScroll;
	int _dragItem;
	int _dragSlot;
	int _dragIndex;				///< bar index the dragged item came from
	uint32 _dragStart;
	bool _dragHeld;				///< picked up with the button down: dropped on release
	int _flyFrom[2], _flyTo[2];
	int _flyTick;
	int _flyItem;
	int _flyIndex;

	Loupe _loupe;
	int _hoverSlot;
	int _retourCell;
	int _lastPlaced;
	int _cursor;
	int _mx, _my;
};

} // End of namespace Vangogh

#endif // VANGOGH_PAGE_H
