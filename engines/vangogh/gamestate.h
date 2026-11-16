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


#ifndef VANGOGH_GAMESTATE_H
#define VANGOGH_GAMESTATE_H

#include "common/scummsys.h"
#include "common/str.h"

namespace Common {
class Serializer;
}

namespace Vangogh {

/**
 * Offsets (in bytes) into the exe's 0x36c-byte game state block, which starts
 * at dat_4f9550 (flow.md sec.3.4, world.md sec.5/12). Fields written as
 * `0x4f97f4 - kStateBase` are named after the exe's global so they can be
 * checked against the notes; every one of them is a dword unless a comment
 * says otherwise.
 */
enum StateOffset {
	kStateBase = 0x4f9550,

	// Global part.
	kOfsScreenSize = 0x000,
	kOfsStarTaken = 0x4f9554 - kStateBase,		///< the museum star was picked up
	kOfsRobotOff = 0x4f9558 - kStateBase,		///< museum first visit done (robot parts disabled)
	kOfsPage0Done = 0x4f955c - kStateBase,
	kOfsActEntered1 = 0x4f9560 - kStateBase,	///< wing A (m01_*) entered
	kOfsActEntered2 = 0x4f9564 - kStateBase,	///< wing B (m03_*) entered
	kOfsActEntered3 = 0x4f9568 - kStateBase,	///< wing C (m04_*) entered
	kOfsEnding = 0x4f956c - kStateBase,			///< game finished
	kOfsEndingPlayed = 0x4f9570 - kStateBase,
	kOfsCamRotA = 0x024,						///< saved camera: pitch, yaw, roll, x, y, z (sign-extended int16)
	kOfsCamRotB = 0x028,
	kOfsCamRotC = 0x02c,
	kOfsCamX = 0x030,
	kOfsCamY = 0x034,
	kOfsCamZ = 0x038,
	kOfsScene = 0x03c,							///< byte: current world id
	kOfsPrevScene = 0x03d,						///< byte: previous world id
	kOfsPage = 0x03e,							///< byte: last 2D page
	kOfsInventory = 0x040,						///< dword[35], non-zero = owned
	kOfsPuzzle = 0x4f961c - kStateBase,			///< A[25]: puzzle solved flags
	kOfsDeparted = 0x4f9680 - kStateBase,		///< B[25]: departure clip played
	kOfsSunflowers = 0x4f96e4 - kStateBase,		///< byte: NbTournesols
	kOfsStarDeposited = 0x4f96e8 - kStateBase,	///< 0 = museum places the camera at the boot pose
	kOfsCurAct1 = 0x4f96ec - kStateBase,		///< one-hot current wing
	kOfsCurAct2 = 0x4f96f0 - kStateBase,
	kOfsCurAct3 = 0x4f96f4 - kStateBase,
	kOfsVaseEnabled = 0x4f96f8 - kStateBase,
	kOfsRobotMet = 0x4f9700 - kStateBase,		///< museum robot greeted the player

	// mangeurs
	kMgPotatoTaken = 0x4f970c - kStateBase,
	kMgStoveOpen = 0x4f9710 - kStateBase,
	kMgCuckooPending = 0x4f9718 - kStateBase,
	kMgFireLit = 0x4f9728 - kStateBase,
	kMgKettleTaken = 0x4f972c - kStateBase,
	kMgLogPlaced = 0x4f973c - kStateBase,
	kMgKindlingPlaced = 0x4f974c - kStateBase,
	kMgWindowClosed = 0x4f9768 - kStateBase,
	// maisonet
	kMeFeatherTaken = 0x4f9774 - kStateBase,
	kMeEarthTaken = 0x4f9778 - kStateBase,
	kMeHenFled = 0x4f9784 - kStateBase,
	kMeDigDone = 0x4f9794 - kStateBase,
	kMeBirdFlew = 0x4f97a4 - kStateBase,
	// cafe
	kCfBilliardDone = 0x4f97b0 - kStateBase,	///< billiard solved and the shadow picked
	kCfMapTaken = 0x4f97b4 - kStateBase,
	kCfBriketTaken = 0x4f97b8 - kStateBase,
	kCfPitcherTaken = 0x4f97bc - kStateBase,
	kCfDoorOpen = 0x4f97c0 - kStateBase,		///< bar door unlocked (key used)
	kCfTeapotTaken = 0x4f97c4 - kStateBase,
	kCfClockTaken = 0x4f97c8 - kStateBase,
	// jardin
	kJaFuzzTaken = 0x4f97cc - kStateBase,
	kJaRatoTaken = 0x4f97d0 - kStateBase,
	kJaGateOpen = 0x4f97d4 - kStateBase,
	kJaKiteFlown = 0x4f97dc - kStateBase,
	// eglise
	kEgKiteAway = 0x4f97e8 - kStateBase,		///< 1 until the kite lands (church hides `cerf` while set)
	kEgKiteDone = 0x4f97ec - kStateBase,
	kEgSheafTaken = 0x4f97f0 - kStateBase,
	// auberge
	kAuStethoscopeTaken = 0x4f97f4 - kStateBase,
	kAuBagOpen = 0x4f97f8 - kStateBase,
	kAuLowerDoorOpen = 0x4f97fc - kStateBase,
	kAuUpperDoorOpen = 0x4f9800 - kStateBase,
	// champ
	kChGunTaken = 0x4f9804 - kStateBase,
	kChScytheDone = 0x4f9808 - kStateBase,
	// hopiext / hopiint
	kHxGridOpen = 0x4f980c - kStateBase,
	kHiWardrobeOpen = 0x4f9810 - kStateBase,
	kHiCrossTaken = 0x4f9814 - kStateBase,
	kHiLampTaken = 0x4f9818 - kStateBase,
	kHiBonnetTaken = 0x4f981c - kStateBase,
	kHiCandleTaken = 0x4f9820 - kStateBase,
	// chambre
	kCbVariant = 0x4f9824 - kStateBase,			///< 0 = chambrev, non-zero = chambreb
	kCbObject09Taken = 0x4f9828 - kStateBase,
	kCbObject07Taken = 0x4f982c - kStateBase,
	kCbFrameTaken = 0x4f9830 - kStateBase,
	kCbObject02Taken = 0x4f9834 - kStateBase,
	kCbUmbrellaTaken = 0x4f9838 - kStateBase,
	kCbCameoTaken = 0x4f983c - kStateBase,
	kCbMirrorShardTaken = 0x4f9840 - kStateBase,
	kCbShoesTaken = 0x4f9844 - kStateBase,
	kCbWardrobeOpen = 0x4f9848 - kStateBase,
	kCbVDoorOpen = 0x4f984c - kStateBase,		///< chambrev bedroom door opened
	kCbDrawerOpen = 0x4f9850 - kStateBase,
	kCbChairMoved = 0x4f9854 - kStateBase,
	kCbPaperOpen = 0x4f9858 - kStateBase,
	kCbMirrorBroken = 0x4f985c - kStateBase,
	kCbShoesEnabled = 0x4f9860 - kStateBase,
	// terrasse
	kTeDieTaken = 0x4f9878 - kStateBase,
	kTeGlassesTaken = 0x4f987c - kStateBase,
	kTeLetterTaken = 0x4f9880 - kStateBase,
	kTeStoreUp = 0x4f9884 - kStateBase,
	kTeIntroPlayed = 0x4f9888 - kStateBase,
	// pont
	kPoTrapOpen = 0x4f988c - kStateBase,
	kPoBridgeDown = 0x4f9890 - kStateBase,
	kPoHandleMounted = 0x4f9894 - kStateBase,
	kPoAppleFallen = 0x4f9898 - kStateBase,
	kPoAppleTaken = 0x4f989c - kStateBase,
	kPoInkTaken = 0x4f98a0 - kStateBase,
	// maisonj
	kMjYellowDone = 0x4f98a4 - kStateBase,
	kMjSnailTaken = 0x4f98a8 - kStateBase,
	kMjSnailReleased = 0x4f98ac - kStateBase,
	kMjGateOpen = 0x4f98b0 - kStateBase,
	kMjDoorOpen = 0x4f98b4 - kStateBase,
	kMjRainSeen = 0x4f98b8 - kStateBase
};

/** A named state dword, for the console and the test harness. */
struct StateFlagName {
	const char *name;
	uint16 offset;
};

/**
 * The exe's saved game state: the 0x36c-byte block at 0x4f9550 and the
 * 0x100-byte 2D-puzzle extra block (page_done[25], item_placed[35],
 * group_counter[4]), flow.md sec.3.4/3.5. The block is kept as the exe's own
 * dword array so that sync() produces the original byte layout (GAME%04d.BIN
 * is the two blocks back to back, 0x46c bytes).
 */
class GameState {
public:
	enum {
		kBlockSize = 0x36c,
		kExtraSize = 0x100,
		kSaveSize = kBlockSize + kExtraSize,
		kNumItems = 35,
		kNumPuzzles = 25,
		kNumGroups = 4
	};

	GameState();

	/** Fresh game: the exe's static .data image of the block (zero except a few initial values). */
	void newGame();

	uint32 get(uint offset) const { return _block[offset >> 2]; }
	void set(uint offset, uint32 value) { _block[offset >> 2] = value; }
	bool isSet(uint offset) const { return get(offset) != 0; }

	/** Byte-wide fields (scene ids, NbTournesols). */
	byte getByte(uint offset) const { return (byte)(_block[offset >> 2] >> ((offset & 3) * 8)); }
	void setByte(uint offset, byte value);

	uint32 item(int i) const { return get(kOfsInventory + i * 4); }
	void setItem(int i, uint32 v) { set(kOfsInventory + i * 4, v); }
	bool puzzleSolved(int id) const { return get(kOfsPuzzle + id * 4) != 0; }
	void setPuzzleSolved(int id, bool v) { set(kOfsPuzzle + id * 4, v ? 1 : 0); }
	bool departed(int page) const { return get(kOfsDeparted + page * 4) != 0; }

	/** sub_426480: all puzzle flags of world @p scene are set (auberge, hopiext and jardin never are). */
	bool sceneSolved(int scene) const;

	/** Reads or writes both blocks in the exe's byte order (little endian). */
	void sync(Common::Serializer &s);

	/** Looks a flag up by console name ("maisonj.gateOpen", "A12", "inv5", or a hex block offset). */
	bool findFlag(const Common::String &name, uint &offset) const;
	/** Appends the named flags that are set (and A[]/B[]/NbTournesols) as text lines. */
	void describe(Common::String &out) const;
	static uint numNamedFlags();
	static const StateFlagName &namedFlag(uint i);

	// 0x100 extra block.
	uint32 pageDone[kNumPuzzles];
	uint32 itemPlaced[kNumItems];
	int32 groupCounter[kNumGroups];

private:
	uint32 _block[kBlockSize / 4];
};

} // End of namespace Vangogh

#endif // VANGOGH_GAMESTATE_H
