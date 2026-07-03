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

#include "vangogh/console.h"
#include "vangogh/navgraph.h"
#include "vangogh/scene.h"
#include "vangogh/vangogh.h"

namespace Vangogh {

Console::Console() : GUI::Debugger() {
	registerCmd("test",     WRAP_METHOD(Console, Cmd_test));
	registerCmd("playhnm",  WRAP_METHOD(Console, Cmd_playHNM));
	registerCmd("showtgp",  WRAP_METHOD(Console, Cmd_showTGP));
	registerCmd("showspr",  WRAP_METHOD(Console, Cmd_showSPR));
	registerCmd("scene",    WRAP_METHOD(Console, Cmd_scene));
	registerCmd("hotspots", WRAP_METHOD(Console, Cmd_hotspots));
}

Console::~Console() {
}

bool Console::Cmd_test(int argc, const char **argv) {
	debugPrintf("Test\n");
	return true;
}

bool Console::Cmd_playHNM(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Usage: %s <basename>\n", argv[0]);
		debugPrintf("Plays data/movies/<basename>.hnm to completion (any key/click skips).\n");
		return true;
	}

	g_engine->playVideo(argv[1]);
	return true;
}

bool Console::Cmd_showTGP(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Usage: %s <basename>\n", argv[0]);
		debugPrintf("Decodes and shows data/gfx/<basename>.TGP for 5 seconds.\n");
		return true;
	}

	g_engine->showTGPImage(argv[1], 5000);
	return true;
}

bool Console::Cmd_showSPR(int argc, const char **argv) {
	if (argc != 2 && argc != 3) {
		debugPrintf("Usage: %s <basename> [cellIndex]\n", argv[0]);
		debugPrintf("Decodes and shows cell [cellIndex] (default 0) of data/sprites/<basename>.spr for 5 seconds.\n");
		return true;
	}

	const uint32 cellIndex = (argc == 3) ? (uint32)atoi(argv[2]) : 0;
	g_engine->showSPRCell(argv[1], cellIndex, 5000);
	return true;
}

bool Console::Cmd_scene(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Usage: %s <basename>\n", argv[0]);
		debugPrintf("Enters the scene loop (real-camera hotspot projection, one-shot arrival/departure backdrops,\n");
		debugPrintf("click-driven transitions) for data/scenes_3d/<basename>.bfg, until a key/quit/transition.\n");
		return true;
	}

	g_engine->enterScene(argv[1]);
	return true;
}

bool Console::Cmd_hotspots(int argc, const char **argv) {
	if (argc != 1) {
		debugPrintf("Usage: %s\n", argv[0]);
		debugPrintf("Lists the last-entered scene's (see `scene <basename>`) projected hotspot boxes -- screen\n");
		debugPrintf("rect, depth, and bound nav target -- plus any region-based (non-.3DI-box) nav edges.\n");
		return true;
	}

	const Common::String &name = g_engine->lastSceneName();
	if (name.empty()) {
		debugPrintf("No scene has been entered yet -- use 'scene <basename>' first.\n");
		return true;
	}

	const Common::Array<Scene::ProjectedHotspot> &hotspots = g_engine->lastSceneHotspots();
	debugPrintf("scene '%s': %u projected hotspot box(es)\n", name.c_str(), (uint32)hotspots.size());
	for (const auto &h : hotspots) {
		if (!h.visible) {
			debugPrintf("  box #%u: entirely behind camera at this pose, not visible/pickable\n", h.boxIndex);
		} else if (h.hasAction) {
			debugPrintf("  box #%u: screen rect (%d,%d)-(%d,%d) depth=%.0f %s\n",
				h.boxIndex, h.rect.left, h.rect.top, h.rect.right, h.rect.bottom, h.nearestDepth, h.actionLabel.c_str());
		} else {
			debugPrintf("  box #%u: screen rect (%d,%d)-(%d,%d) depth=%.0f (no bound nav target)\n",
				h.boxIndex, h.rect.left, h.rect.top, h.rect.right, h.rect.bottom, h.nearestDepth);
		}
	}

	bool anyRegion = false;
	for (const auto &edge : Vangogh::navEdgesForScene(name)) {
		if (edge.boxIndex < 0) {
			if (!anyRegion) {
				debugPrintf("region-based nav edges (not a .3DI box):\n");
				anyRegion = true;
			}
			debugPrintf("  region (%d,%d)-(%d,%d) %s\n",
				edge.region.left, edge.region.top, edge.region.right, edge.region.bottom, edge.label);
		}
	}

	return true;
}

} // End of namespace Vangogh
