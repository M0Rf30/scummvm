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


#include "vangogh/gamestate.h"

#include "common/serializer.h"
#include "common/util.h"

namespace Vangogh {

namespace {

/** The non-zero dwords of the block in the exe's static .data (PE image at 0x4f9550..0x4f98bb). */
struct InitialDword {
	uint16 offset;
	uint32 value;
};

const InitialDword kInitialDwords[] = {
	{ 0x1ac, 0xff },
	{ 0x1b8, 0x10001 },
	{ 0x1c4, 0xff }, { 0x1c8, 1 },	// 0x1c8 = mangeurs cuckoo pending
	{ 0x1d0, 0xffffff }, { 0x1d4, 0xff },
	{ 0x1e0, 0xffffff }, { 0x1e4, 0xff }, { 0x1e8, 1 },
	{ 0x1f0, 0xffffff }, { 0x1f4, 0xff },
	{ 0x200, 0xffffff }, { 0x204, 0xff },
	{ 0x210, 0xffffff }, { 0x214, 0xff },
	{ 0x220, 0xffffff },
	{ 0x230, 0xff },
	{ 0x23c, 1 },
	{ 0x240, 0xff },
	{ 0x24c, 1 },
	{ 0x250, 0xff },
	{ 0x25c, 1 },
	{ 0x288, 0xff },
	{ 0x294, 0x10001 },
	{ 0x298, 1 },	// kEgKiteAway
	{ 0x320, 1 }
};

#define FLAG(n, o) { n, (uint16)(o) }
const StateFlagName kFlagNames[] = {
	FLAG("starTaken", kOfsStarTaken), FLAG("robotOff", kOfsRobotOff), FLAG("page0Done", kOfsPage0Done),
	FLAG("actEntered1", kOfsActEntered1), FLAG("actEntered2", kOfsActEntered2), FLAG("actEntered3", kOfsActEntered3),
	FLAG("ending", kOfsEnding), FLAG("endingPlayed", kOfsEndingPlayed),
	FLAG("scene", kOfsScene), FLAG("prevScene", kOfsPrevScene), FLAG("page", kOfsPage),
	FLAG("sunflowers", kOfsSunflowers), FLAG("starDeposited", kOfsStarDeposited),
	FLAG("curAct1", kOfsCurAct1), FLAG("curAct2", kOfsCurAct2), FLAG("curAct3", kOfsCurAct3),
	FLAG("vaseEnabled", kOfsVaseEnabled), FLAG("robotMet", kOfsRobotMet),
	FLAG("mangeurs.potatoTaken", kMgPotatoTaken), FLAG("mangeurs.stoveOpen", kMgStoveOpen),
	FLAG("mangeurs.cuckooPending", kMgCuckooPending), FLAG("mangeurs.fireLit", kMgFireLit),
	FLAG("mangeurs.kettleTaken", kMgKettleTaken), FLAG("mangeurs.logPlaced", kMgLogPlaced),
	FLAG("mangeurs.kindlingPlaced", kMgKindlingPlaced), FLAG("mangeurs.windowClosed", kMgWindowClosed),
	FLAG("maisonet.featherTaken", kMeFeatherTaken), FLAG("maisonet.earthTaken", kMeEarthTaken),
	FLAG("maisonet.henFled", kMeHenFled), FLAG("maisonet.digDone", kMeDigDone), FLAG("maisonet.birdFlew", kMeBirdFlew),
	FLAG("cafe.billiardDone", kCfBilliardDone), FLAG("cafe.mapTaken", kCfMapTaken),
	FLAG("cafe.briketTaken", kCfBriketTaken), FLAG("cafe.pitcherTaken", kCfPitcherTaken),
	FLAG("cafe.doorOpen", kCfDoorOpen), FLAG("cafe.teapotTaken", kCfTeapotTaken), FLAG("cafe.clockTaken", kCfClockTaken),
	FLAG("jardin.fuzzTaken", kJaFuzzTaken), FLAG("jardin.ratoTaken", kJaRatoTaken),
	FLAG("jardin.gateOpen", kJaGateOpen), FLAG("jardin.kiteFlown", kJaKiteFlown),
	FLAG("eglise.kiteAway", kEgKiteAway), FLAG("eglise.kiteDone", kEgKiteDone), FLAG("eglise.sheafTaken", kEgSheafTaken),
	FLAG("auberge.stethoscopeTaken", kAuStethoscopeTaken), FLAG("auberge.bagOpen", kAuBagOpen),
	FLAG("auberge.lowerDoorOpen", kAuLowerDoorOpen), FLAG("auberge.upperDoorOpen", kAuUpperDoorOpen),
	FLAG("champ.gunTaken", kChGunTaken), FLAG("champ.scytheDone", kChScytheDone),
	FLAG("hopiext.gridOpen", kHxGridOpen),
	FLAG("hopiint.wardrobeOpen", kHiWardrobeOpen), FLAG("hopiint.crossTaken", kHiCrossTaken),
	FLAG("hopiint.lampTaken", kHiLampTaken), FLAG("hopiint.bonnetTaken", kHiBonnetTaken),
	FLAG("hopiint.candleTaken", kHiCandleTaken),
	FLAG("chambre.variant", kCbVariant), FLAG("chambre.object09Taken", kCbObject09Taken),
	FLAG("chambre.object07Taken", kCbObject07Taken), FLAG("chambre.frameTaken", kCbFrameTaken),
	FLAG("chambre.object02Taken", kCbObject02Taken), FLAG("chambre.umbrellaTaken", kCbUmbrellaTaken),
	FLAG("chambre.cameoTaken", kCbCameoTaken), FLAG("chambre.mirrorShardTaken", kCbMirrorShardTaken),
	FLAG("chambre.shoesTaken", kCbShoesTaken), FLAG("chambre.wardrobeOpen", kCbWardrobeOpen),
	FLAG("chambre.vDoorOpen", kCbVDoorOpen), FLAG("chambre.drawerOpen", kCbDrawerOpen),
	FLAG("chambre.chairMoved", kCbChairMoved), FLAG("chambre.paperOpen", kCbPaperOpen),
	FLAG("chambre.mirrorBroken", kCbMirrorBroken), FLAG("chambre.shoesEnabled", kCbShoesEnabled),
	FLAG("terrasse.dieTaken", kTeDieTaken), FLAG("terrasse.glassesTaken", kTeGlassesTaken),
	FLAG("terrasse.letterTaken", kTeLetterTaken), FLAG("terrasse.storeUp", kTeStoreUp),
	FLAG("terrasse.introPlayed", kTeIntroPlayed),
	FLAG("pont.trapOpen", kPoTrapOpen), FLAG("pont.bridgeDown", kPoBridgeDown),
	FLAG("pont.handleMounted", kPoHandleMounted), FLAG("pont.appleFallen", kPoAppleFallen),
	FLAG("pont.appleTaken", kPoAppleTaken), FLAG("pont.inkTaken", kPoInkTaken),
	FLAG("maisonj.yellowDone", kMjYellowDone), FLAG("maisonj.snailTaken", kMjSnailTaken),
	FLAG("maisonj.snailReleased", kMjSnailReleased), FLAG("maisonj.gateOpen", kMjGateOpen),
	FLAG("maisonj.doorOpen", kMjDoorOpen), FLAG("maisonj.rainSeen", kMjRainSeen)
};
#undef FLAG

} // end of anonymous namespace

GameState::GameState() {
	newGame();
}

void GameState::newGame() {
	memset(_block, 0, sizeof(_block));
	memset(pageDone, 0, sizeof(pageDone));
	memset(itemPlaced, 0, sizeof(itemPlaced));
	memset(groupCounter, 0, sizeof(groupCounter));
	for (uint i = 0; i < ARRAYSIZE(kInitialDwords); i++)
		set(kInitialDwords[i].offset, kInitialDwords[i].value);
}

void GameState::setByte(uint offset, byte value) {
	const uint shift = (offset & 3) * 8;
	uint32 &w = _block[offset >> 2];
	w = (w & ~(0xffu << shift)) | ((uint32)value << shift);
}

bool GameState::sceneSolved(int scene) const {
	// sub_426480: world id -> the A[] puzzle flags that must all be set.
	static const int8 kPuzzles[14][5] = {
		{ -1 },						// 0 musee
		{ -1 },						// 1 auberge: never "solved"
		{ -1 },						// 2 hopiext
		{ 1, -1 },					// 3 maisonet
		{ 2, 3, -1 },				// 4 mangeurs
		{ 4, 5, 6, -1 },			// 5 cafe
		{ 7, 8, 9, 10, 11 },		// 6 chambre
		{ 12, -1 },					// 7 maisonj
		{ 13, 14, 15, -1 },			// 8 hopiint
		{ 18, -1 },					// 9 pont
		{ 16, 17, -1 },				// 10 terrasse
		{ -1 },						// 11 jardin: never "solved"
		{ 22, 24, -1 },				// 12 champ
		{ 23, -1 }					// 13 eglise
	};
	if (scene < 0 || scene >= 14 || scene < 3 || scene == 11)
		return false;
	for (int i = 0; i < 5 && kPuzzles[scene][i] >= 0; i++) {
		if (!puzzleSolved(kPuzzles[scene][i]))
			return false;
	}
	return true;
}

void GameState::sync(Common::Serializer &s) {
	for (uint i = 0; i < ARRAYSIZE(_block); i++)
		s.syncAsUint32LE(_block[i]);
	for (int i = 0; i < kNumPuzzles; i++)
		s.syncAsUint32LE(pageDone[i]);
	for (int i = 0; i < kNumItems; i++)
		s.syncAsUint32LE(itemPlaced[i]);
	for (int i = 0; i < kNumGroups; i++)
		s.syncAsSint32LE(groupCounter[i]);
}

void GameState::describe(Common::String &out) const {
	out += Common::String::format("scene=%d prev=%d page=%d sunflowers=%d\n", getByte(kOfsScene), getByte(kOfsPrevScene),
		getByte(kOfsPage), getByte(kOfsSunflowers));
	out += "A (solved):";
	for (int i = 0; i < kNumPuzzles; i++) {
		if (puzzleSolved(i))
			out += Common::String::format(" A%d", i);
	}
	out += "\nB (departed):";
	for (int i = 0; i < kNumPuzzles; i++) {
		if (departed(i))
			out += Common::String::format(" B%d", i);
	}
	out += "\nset flags:";
	for (uint i = 0; i < ARRAYSIZE(kFlagNames); i++) {
		const uint32 v = get(kFlagNames[i].offset);
		if (v && kFlagNames[i].offset != kOfsSunflowers && kFlagNames[i].offset != kOfsScene && kFlagNames[i].offset != kOfsPrevScene
				&& kFlagNames[i].offset != kOfsPage)
			out += Common::String::format(" %s=%u", kFlagNames[i].name, v);
	}
	out += "\n";
}

uint GameState::numNamedFlags() {
	return ARRAYSIZE(kFlagNames);
}

const StateFlagName &GameState::namedFlag(uint i) {
	return kFlagNames[i];
}

bool GameState::findFlag(const Common::String &name, uint &offset) const {
	for (uint i = 0; i < ARRAYSIZE(kFlagNames); i++) {
		if (name.equalsIgnoreCase(kFlagNames[i].name)) {
			offset = kFlagNames[i].offset;
			return true;
		}
	}
	if (name.size() > 1 && (name[0] == 'A' || name[0] == 'a' || name[0] == 'B' || name[0] == 'b')) {
		char *end = nullptr;
		const long n = strtol(name.c_str() + 1, &end, 10);
		if (*end == '\0' && n >= 0 && n < kNumPuzzles) {
			offset = ((name[0] | 0x20) == 'a' ? kOfsPuzzle : kOfsDeparted) + n * 4;
			return true;
		}
	}
	if (name.size() > 3 && name.hasPrefixIgnoreCase("inv")) {
		char *end = nullptr;
		const long n = strtol(name.c_str() + 3, &end, 10);
		if (*end == '\0' && n >= 0 && n < kNumItems) {
			offset = kOfsInventory + n * 4;
			return true;
		}
	}
	if (name.hasPrefix("0x")) {
		char *end = nullptr;
		const long n = strtol(name.c_str() + 2, &end, 16);
		if (*end == '\0' && n >= 0 && n < kBlockSize) {
			// Accept both block offsets and exe addresses (0x4f97f4).
			offset = (uint)(n >= (long)kStateBase ? n - kStateBase : n);
			return offset < (uint)kBlockSize;
		}
	}
	return false;
}

} // End of namespace Vangogh
