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

#include "vangogh/scene.h"

#include "vangogh/bfg.h"
#include "vangogh/vangogh.h"

#include "common/archive.h"
#include "common/config-manager.h"
#include "common/endian.h"
#include "common/events.h"
#include "common/file.h"
#include "common/path.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/cursorman.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"

namespace Vangogh {

namespace {

// Not every scene's HNM basename matches its scenes_3d container's basename
// exactly (e.g. the room "chambreb" vs. a differently-abbreviated movie
// codename) -- untangling that mapping for real is exactly what
// SceneFlowRE's scene-flow investigation is for. Until that lands, resolve
// the common case (the movie IS the scene name, optionally with a suffix,
// e.g. "jardin.hnm"/"jardinr.hnm" for scene "jardin") without a hardcoded
// table: try the exact name first, then fall back to a prefix search.
Common::Path locateBackdropPath(const Common::String &name) {
	const Common::Path exactPath(Common::String::format("movies/%s.hnm", name.c_str()));
	if (Common::File::exists(exactPath))
		return exactPath;

	Common::ArchiveMemberList matches;
	SearchMan.listMatchingMembers(matches, Common::Path(Common::String::format("movies/%s*.hnm", name.c_str())));
	if (matches.empty())
		return Common::Path();

	// Deterministic pick among multiple matches: shortest file name first
	// (the "plain" variant, e.g. "jardin.hnm" over "jardinr.hnm"), then
	// lexicographic.
	Common::Path best;
	Common::String bestName;
	for (const auto &member : matches) {
		const Common::String candidate = member->getFileName();
		if (bestName.empty() || candidate.size() < bestName.size() ||
			(candidate.size() == bestName.size() && candidate < bestName)) {
			bestName = candidate;
			best = member->getPathInArchive();
		}
	}
	return best;
}

const uint kCursorSize = 15;

// No real cursor art has been recovered from the game data (out of scope
// for hotspot/navigation reversing so far); this is a minimal generated
// crosshair purely so the scene loop has *some* visible pointer, not a
// reversed asset -- TODO: replace if/when a real cursor resource turns up.
void setPlaceholderCursor() {
	const Graphics::PixelFormat format = g_system->getScreenFormat();
	if (format.bytesPerPixel != 2 && format.bytesPerPixel != 4) {
		warning("Vangogh: unsupported screen format (%d bytes/pixel), skipping placeholder cursor", format.bytesPerPixel);
		return;
	}

	const uint32 keyColor = format.RGBToColor(255, 0, 255);
	const uint32 crossColor = format.RGBToColor(255, 255, 255);
	const uint mid = kCursorSize / 2;

	Common::Array<byte> pixels(kCursorSize * kCursorSize * format.bytesPerPixel);
	for (uint y = 0; y < kCursorSize; y++) {
		for (uint x = 0; x < kCursorSize; x++) {
			const uint32 color = (x == mid || y == mid) ? crossColor : keyColor;
			byte *dst = pixels.data() + (y * kCursorSize + x) * format.bytesPerPixel;
			if (format.bytesPerPixel == 2)
				WRITE_LE_UINT16(dst, (uint16)color);
			else
				WRITE_LE_UINT32(dst, color);
		}
	}

	CursorMan.pushCursor(pixels.data(), kCursorSize, kCursorSize, mid, mid, keyColor, false, &format);
	CursorMan.showMouse(true);
}

} // end of anonymous namespace

Scene::Scene(const Common::String &name) : _name(name), _hasBackdrop(false) {
}

Scene::~Scene() {
}

bool Scene::load() {
	const Common::Path backdropPath = locateBackdropPath(_name);
	if (backdropPath.empty()) {
		warning("Vangogh: scene '%s': no backdrop movie found matching movies/%s*.hnm", _name.c_str(), _name.c_str());
	} else {
		// Looping: a scene backdrop is ambient, not a one-shot cutscene --
		// see HNMPlayer::load()/video/hnm_decoder.h.
		_hasBackdrop = _player.load(backdropPath, /*loop=*/true);
	}

	const Common::Path bfgPath(Common::String::format("scenes_3d/%s.bfg", _name.c_str()));
	Common::File bfgStream;
	if (!bfgStream.open(bfgPath)) {
		warning("Vangogh: scene '%s': could not open %s", _name.c_str(), bfgPath.toString().c_str());
	} else {
		BFGFile bfg;
		if (!bfg.load(bfgStream)) {
			warning("Vangogh: scene '%s': failed to parse %s", _name.c_str(), bfgPath.toString().c_str());
		} else {
			Common::Array<byte> boxData;
			if (bfg.getRecord("BOX.3DI", boxData))
				_hotspotBoxes = decodeHotspotBoxes(boxData);
		}
	}

	debug("Vangogh: scene %s: %u hotspot boxes loaded", _name.c_str(), (uint32)_hotspotBoxes.size());

	return _hasBackdrop || !_hotspotBoxes.empty();
}

void Scene::handleClick(const Common::Point &pt) const {
	int hitIndex = -1;
	for (const auto &box : _hotspotBoxes) {
		if (box.containsScreenPoint(pt)) {
			hitIndex = (int)box.index;
			break;
		}
	}

	if (hitIndex >= 0)
		debug("Vangogh: scene '%s': click at (%d,%d) hit hotspot box #%d", _name.c_str(), pt.x, pt.y, hitIndex);
	else
		debug("Vangogh: scene '%s': click at (%d,%d) hit no hotspot box", _name.c_str(), pt.x, pt.y);
}

void Scene::run() {
	// Automated/headless runs (see VangoghEngine::showAccueil()) have no
	// real input device to ever produce a keypress, so under boot_param
	// the loop also leaves on its own after a small, fixed number of
	// ticks -- enough to prove backdrop decoding is actually happening,
	// for scenes that have one -- rather than waiting forever. Ticks, not
	// decoded frames: a scene with no backdrop (_hasBackdrop false, see
	// load()) never decodes a frame at all and must still bound the loop.
	// Interactive play never sets boot_param.
	const bool autoAdvance = ConfMan.hasKey("boot_param");
	const uint32 kAutoAdvanceTicks = 100;

	setPlaceholderCursor();

	uint32 framesDecoded = 0;
	uint32 ticks = 0;
	bool leave = false;
	while (!g_engine->shouldQuit() && !leave) {
		if (_hasBackdrop && _player.decodeNextFrame()) {
			const Graphics::Surface *frame = _player.currentSurface();
			if (frame) {
				g_system->copyRectToScreen(frame->getPixels(), frame->pitch, 0, 0, _player.width(), _player.height());
				framesDecoded++;
			}
		}

		g_system->updateScreen();
		g_system->delayMillis(10);

		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			switch (event.type) {
			case Common::EVENT_QUIT:
			case Common::EVENT_RETURN_TO_LAUNCHER:
			case Common::EVENT_KEYDOWN:
				leave = true;
				break;
			case Common::EVENT_LBUTTONDOWN:
				handleClick(event.mouse);
				break;
			default:
				break;
			}
		}

		if (autoAdvance && ++ticks >= kAutoAdvanceTicks)
			leave = true;
	}

	CursorMan.popCursor();
	CursorMan.showMouse(false);

	debug("Vangogh: scene '%s': leaving after %u backdrop frame(s) decoded", _name.c_str(), framesDecoded);
}

} // End of namespace Vangogh
