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

const Common::Rect kScreenRect(0, 0, 640, 480);

} // end of anonymous namespace

Scene::Scene(const Common::String &name) : _name(name), _hasBackdrop(false), _camera(), _hasCameraLiteral(false), _leaveRequested(false) {
}

Scene::~Scene() {
}

bool Scene::load() {
	const SceneBackdropNames backdropNames = backdropNamesForScene(_name);
	if (backdropNames.arrival) {
		_arrivalMovie = backdropNames.arrival;
		_departureMovie = backdropNames.departure ? Common::String(backdropNames.departure) : Common::String();
		const Common::Path arrivalPath(Common::String::format("movies/%s.hnm", _arrivalMovie.c_str()));
		_hasBackdrop = Common::File::exists(arrivalPath);
		if (!_hasBackdrop)
			warning("Vangogh: scene '%s': arrival backdrop %s not found", _name.c_str(), arrivalPath.toString().c_str());
	} else {
		// musee/auberge/hopiext genuinely have no backdrop movie at all
		// (scene-load-findings.md sec.2.2/sec.4) -- not an error.
		debug("Vangogh: scene '%s': no backdrop movie for this scene", _name.c_str());
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

	_hasCameraLiteral = cameraPoseForScene(_name, _camera);
	if (!_hasCameraLiteral)
		warning("Vangogh: scene '%s': no recovered camera-pose literal, falling back to an identity camera "
			"(hotspot screen rects for this scene are unreliable)", _name.c_str());

	_edges = navEdgesForScene(_name);

	debug("Vangogh: scene %s: %u hotspot boxes loaded", _name.c_str(), (uint32)_hotspotBoxes.size());

	projectHotspots();

	// Only "nothing at all to show or do" fails load(): a scene can be a
	// legitimate, navigable node with an empty room (no backdrop, no
	// decoded boxes -- e.g. hopiext) as long as it still has outgoing nav
	// edges to click through.
	return _hasBackdrop || !_hotspotBoxes.empty() || !_edges.empty();
}

const NavEdge *Scene::findEdgeForBox(uint32 boxIndex) const {
	for (const auto &edge : _edges) {
		if (edge.boxIndex == (int)boxIndex)
			return &edge;
	}
	return nullptr;
}

void Scene::projectHotspots() {
	_projected.clear();

	uint32 visibleCount = 0;
	uint32 onScreenCount = 0;
	for (const auto &box : _hotspotBoxes) {
		ProjectedHotspot p;
		p.boxIndex = box.index;
		p.visible = box.projectToScreen(_camera, /* zoomLevel = */ 0, p.rect, p.nearestDepth);

		const NavEdge *edge = findEdgeForBox(box.index);
		if (edge) {
			p.hasAction = true;
			p.actionArg = edge->actionArg;
			p.actionLabel = edge->label;
		}

		if (p.visible) {
			visibleCount++;
			const bool onScreen = p.rect.intersects(kScreenRect);
			if (onScreen)
				onScreenCount++;
			debug("Vangogh: scene '%s': hotspot box #%u screen rect (%d,%d)-(%d,%d) depth=%.0f%s%s",
				_name.c_str(), p.boxIndex, p.rect.left, p.rect.top, p.rect.right, p.rect.bottom, p.nearestDepth,
				onScreen ? "" : " (off-screen at this pose)",
				p.hasAction ? Common::String::format(" %s", p.actionLabel.c_str()).c_str() : " (unbound)");
		} else {
			debug("Vangogh: scene '%s': hotspot box #%u entirely behind camera at this pose, not visible/pickable",
				_name.c_str(), p.boxIndex);
		}

		_projected.push_back(p);
	}

	debug("Vangogh: scene '%s': projected %u hotspots (%u in front of camera, %u actually on-screen, zoom level 0, camera %s)",
		_name.c_str(), (uint32)_hotspotBoxes.size(), visibleCount, onScreenCount, _hasCameraLiteral ? "literal" : "fallback");
}

void Scene::handleClick(const Common::Point &pt) {
	// Nearest-depth-wins among every VISIBLE projected box containing pt
	// (scene-flow.md sec.3.1's "honest point-in-triangle + nearest-depth"
	// test, simplified to point-in-projected-bbox since that's the
	// geometry this engine actually has).
	int hitBoxIndex = -1;
	double bestDepth = 0.0;
	const ProjectedHotspot *hit = nullptr;
	for (const auto &p : _projected) {
		if (p.visible && p.rect.contains(pt) && (hitBoxIndex < 0 || p.nearestDepth < bestDepth)) {
			hitBoxIndex = (int)p.boxIndex;
			bestDepth = p.nearestDepth;
			hit = &p;
		}
	}

	if (hit && hit->hasAction) {
		debug("Vangogh: scene '%s': click at (%d,%d) hit hotspot box #%d '%s'",
			_name.c_str(), pt.x, pt.y, hitBoxIndex, hit->actionLabel.c_str());
		commitAction(hit->actionArg);
		return;
	}

	// No (bound) box hit -- try maisonj-style raw 2D coordinate-range
	// regions (scene-flow.md sec.3.2), including scenes that have no
	// decoded boxes at all.
	for (const auto &edge : _edges) {
		if (edge.boxIndex < 0 && edge.region.contains(pt)) {
			debug("Vangogh: scene '%s': click at (%d,%d) hit region '%s'", _name.c_str(), pt.x, pt.y, edge.label);
			commitAction(edge.actionArg);
			return;
		}
	}

	if (hitBoxIndex >= 0)
		debug("Vangogh: scene '%s': click at (%d,%d) hit hotspot box #%d (no bound nav target)", _name.c_str(), pt.x, pt.y, hitBoxIndex);
	else
		debug("Vangogh: scene '%s': click at (%d,%d) hit no hotspot/region", _name.c_str(), pt.x, pt.y);
}

void Scene::commitAction(int actionArg) {
	// Mirrors PEINTRE.exe's shared committer, fcn.00432e10 (scene-flow.md
	// sec.4.1): resolve actionArg, then only a transition/hub/quit result
	// actually leaves the room.
	const NavAction action = resolveNavAction(actionArg);
	switch (action.kind) {
	case NavAction::kLocal:
		debug("Vangogh: scene '%s': local action (stub -- no puzzle/save/anim logic implemented), staying in scene", _name.c_str());
		_resolvedAction = action;
		break;
	case NavAction::kTransition:
		debug("Vangogh: scene '%s': transition -> '%s'", _name.c_str(), action.targetScene.c_str());
		_resolvedAction = action;
		_leaveRequested = true;
		break;
	case NavAction::kHub:
		debug("Vangogh: scene '%s': return-to-hub sentinel -> '%s'", _name.c_str(), action.targetScene.c_str());
		_resolvedAction = action;
		_leaveRequested = true;
		break;
	case NavAction::kQuit:
		debug("Vangogh: scene '%s': quit requested", _name.c_str());
		_resolvedAction = action;
		_leaveRequested = true;
		break;
	default:
		break;
	}
}

Common::Point Scene::bestSimulatedClickPoint() const {
	// Prefer a hotspot/region leading to 'jardin' when this scene has one
	// -- makes the automated boot_param demo deterministically exercise
	// BOTH of this milestone's own cross-check scenes (musee, jardin) in
	// a single run. Falls back to the first available, actually-onscreen,
	// bound hotspot/region otherwise.
	for (int preferJardin = 1; preferJardin >= 0; preferJardin--) {
		for (const auto &p : _projected) {
			if (p.visible && p.hasAction && p.rect.intersects(kScreenRect) &&
				(!preferJardin || p.actionArg == kSceneJardin))
				return Common::Point(p.rect.left + p.rect.width() / 2, p.rect.top + p.rect.height() / 2);
		}
		for (const auto &edge : _edges) {
			if (edge.boxIndex < 0 && (!preferJardin || edge.actionArg == kSceneJardin))
				return Common::Point(edge.region.left + edge.region.width() / 2, edge.region.top + edge.region.height() / 2);
		}
	}
	return Common::Point(-1, -1);
}

bool Scene::playOneShotClip(const Common::String &basename, const char *phase) {
	const Common::Path path(Common::String::format("movies/%s.hnm", basename.c_str()));

	HNMPlayer player;
	if (!player.load(path, /* loop = */ false)) {
		warning("Vangogh: scene '%s': could not open %s clip movies/%s.hnm", _name.c_str(), phase, basename.c_str());
		return false;
	}

	debug("Vangogh: scene '%s': playing %s clip '%s' (one-shot)", _name.c_str(), phase, basename.c_str());

	bool skipped = false;
	while (!g_engine->shouldQuit() && !player.endOfVideo() && !skipped) {
		if (player.decodeNextFrame()) {
			const Graphics::Surface *frame = player.currentSurface();
			if (frame)
				g_system->copyRectToScreen(frame->getPixels(), frame->pitch, 0, 0, player.width(), player.height());
		}

		g_system->updateScreen();
		g_system->delayMillis(10);

		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			switch (event.type) {
			case Common::EVENT_QUIT:
			case Common::EVENT_RETURN_TO_LAUNCHER:
			case Common::EVENT_KEYDOWN:
			case Common::EVENT_LBUTTONDOWN:
			case Common::EVENT_RBUTTONDOWN:
				skipped = true;
				break;
			default:
				break;
			}
		}
	}

	// Deliberately does NOT clear/redraw after the loop: leaving the last
	// decoded frame exactly as blitted IS "holding the last frame as the
	// static room view" (the caller's interactive loop keeps calling
	// updateScreen() without decoding anything further).
	debug("Vangogh: scene '%s': %s clip '%s' decoded %d/%u frame(s)%s", _name.c_str(), phase, basename.c_str(),
		player.getCurFrame() + 1, player.getFrameCount(), skipped ? " (skipped)" : "");
	return true;
}

void Scene::run(bool simulateClick) {
	// Automated/headless runs (see VangoghEngine::showAccueil()) have no
	// real input device to ever produce a keypress, so under boot_param
	// the interactive loop also leaves on its own after a small, fixed
	// number of ticks. Interactive play never sets boot_param.
	const bool autoAdvance = ConfMan.hasKey("boot_param");
	const uint32 kAutoAdvanceTicks = 100;

	_leaveRequested = false;
	_resolvedAction = NavAction();

	setPlaceholderCursor();

	// Phase 1: arrival clip, one-shot (never looped -- scene-load-
	// findings.md sec.2.2/sec.5: both plain and `r` clips play exactly
	// once, the low-level frame driver unconditionally closes the file at
	// end-of-stream).
	if (_hasBackdrop)
		playOneShotClip(_arrivalMovie, "arrival");

	// Phase 2: interactive hotspot loop. The arrival clip's last frame is
	// still on screen (see playOneShotClip()) -- exactly the "still
	// backdrop between clips" the milestone brief asks for.
	uint32 ticks = 0;
	while (!g_engine->shouldQuit() && !_leaveRequested) {
		g_system->updateScreen();
		g_system->delayMillis(10);

		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			switch (event.type) {
			case Common::EVENT_QUIT:
			case Common::EVENT_RETURN_TO_LAUNCHER:
				_leaveRequested = true;
				break;
			case Common::EVENT_KEYDOWN:
				if (event.kbd.keycode == Common::KEYCODE_BACKSPACE) {
					// Generic "return to hub" sentinel -- NOT a per-room
					// hotspot (scene-load-findings.md sec.7.4).
					commitAction(kActionReturnToHub);
				} else if (event.kbd.keycode == Common::KEYCODE_ESCAPE) {
					_leaveRequested = true; // leave with no action resolved.
				}
				break;
			case Common::EVENT_LBUTTONDOWN:
				handleClick(event.mouse);
				break;
			default:
				break;
			}
		}

		if (simulateClick && !_leaveRequested) {
			simulateClick = false; // only ever try once.
			const Common::Point pt = bestSimulatedClickPoint();
			if (pt.x >= 0) {
				debug("Vangogh: scene '%s': boot_param set, simulating a click at (%d,%d) to demonstrate a transition",
					_name.c_str(), pt.x, pt.y);
				handleClick(pt);
			} else {
				debug("Vangogh: scene '%s': boot_param set, no clickable hotspot/region available to simulate", _name.c_str());
			}
		}

		if (autoAdvance && ++ticks >= kAutoAdvanceTicks)
			break; // no real input device and nothing left to simulate -- leave cleanly.
	}

	// Phase 3: departure clip, one-shot -- only for an actual room-leaving
	// resolution (transition/hub), never for a raw engine quit/window
	// close or a local (-1) action (which never sets _leaveRequested).
	const bool engineQuitting = g_engine->shouldQuit();
	if (_leaveRequested && !engineQuitting && _resolvedAction.kind != NavAction::kQuit && !_departureMovie.empty())
		playOneShotClip(_departureMovie, "departure");

	CursorMan.popCursor();
	CursorMan.showMouse(false);

	Common::String leaveDesc;
	switch (_resolvedAction.kind) {
	case NavAction::kTransition:
		leaveDesc = Common::String::format("transition -> %s", _resolvedAction.targetScene.c_str());
		break;
	case NavAction::kHub:
		leaveDesc = Common::String::format("return-to-hub -> %s", _resolvedAction.targetScene.c_str());
		break;
	case NavAction::kQuit:
		leaveDesc = "quit";
		break;
	default:
		leaveDesc = "no action resolved (auto-advance timeout, window closed, or Escape pressed)";
		break;
	}
	debug("Vangogh: scene '%s': leaving (%s)", _name.c_str(), leaveDesc.c_str());
}

} // End of namespace Vangogh
