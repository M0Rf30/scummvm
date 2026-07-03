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

#include "vangogh/navgraph.h"

#include "common/textconsole.h"
#include "common/util.h"

namespace Vangogh {

namespace {

struct SceneIdEntry {
	int id;
	const char *name;
};

// Ground truth per scene-load-findings.md sec.1 / scene-flow.md sec.2.1
// (3 independent switch tables in PEINTRE.exe agree).
const SceneIdEntry kSceneIds[] = {
	{ kSceneMusee,     "musee" },
	{ kSceneAuberge,   "auberge" },
	{ kSceneHopiext,   "hopiext" },
	{ kSceneMaisonet,  "maisonet" },
	{ kSceneMangeurs,  "mangeurs" },
	{ kSceneCafe,      "cafe" },
	{ kSceneChambreb,  "chambreb" },
	{ kSceneMaisonj,   "maisonj" },
	{ kSceneHopiint,   "hopiint" },
	{ kScenePont,      "pont" },
	{ kSceneTerrasse,  "terrasse" },
	{ kSceneJardin,    "jardin" },
	{ kSceneChamp,     "champ" },
	{ kSceneEglise,    "eglise" },
};

// chambreb/chambrev are the day/night variant pair for scene enum id 6
// (scene-load-findings.md sec.1, footnote); everything id-keyed in this
// module treats "chambrev" as an alias for "chambreb".
Common::String canonicalSceneName(const Common::String &name) {
	if (name.equalsIgnoreCase("chambrev"))
		return "chambreb";
	return name;
}

struct BackdropEntry {
	const char *scene;
	const char *arrival;
	const char *departure;
};

// scene-load-findings.md sec.2.2 / sec.4: fcn.00427290 (arrival, keyed on
// the TARGET scene) and fcn.00426680 (departure, keyed on the SOURCE
// scene), both 11-case switches over enum ids 3..13. Movie basenames are
// NOT always the scene name (maisonet -> "maisa"/"maisr", chambreb/v ->
// "chamba"/"chambr", hopiint -> "hopi"/"hopir"). musee/auberge/hopiext
// (ids 0/1/2) have no case in either dispatcher -- no entry below.
const BackdropEntry kBackdrops[] = {
	{ "maisonet",  "maisa",     "maisr" },
	{ "mangeurs",  "mangeurs",  "mangeurr" },
	{ "cafe",      "cafe",      "cafer" },
	{ "chambreb",  "chamba",    "chambr" },
	{ "maisonj",   "maisonj",   "maisonjr" },
	{ "hopiint",   "hopi",      "hopir" },
	{ "pont",      "pont",      "pontr" },
	{ "terrasse",  "terrasse",  "terr" },
	{ "jardin",    "jardin",    "jardinr" },
	{ "champ",     "champ",     "champr" },
	{ "eglise",    "eglise",    "eglr" },
};

// "Compass" exit-strip placeholders for scenes whose recovered local-walk
// edges outnumber their decoded .3DI boxes (or, for maisonj, where the
// real game bypasses box-picking for these specific edges entirely --
// scene-flow.md sec.3.2). These are NOT reversed literals: the real
// per-edge screen geometry is populated from runtime `.3DC`/`.3DA` heap
// data and isn't statically recoverable (scene-load-findings.md sec.7.5).
// Deliberately small edge/corner strips rather than full-screen halves, so
// "click empty space -> no hotspot" stays exercisable/testable too.
const Common::Rect kRegionLeft(0, 180, 140, 300);
const Common::Rect kRegionRight(500, 180, 640, 300);
const Common::Rect kRegionBottom(250, 400, 390, 480);
const Common::Rect kRegionTop(250, 0, 390, 80);

} // end of anonymous namespace

Common::String sceneNameForId(int sceneId) {
	for (uint i = 0; i < ARRAYSIZE(kSceneIds); i++) {
		if (kSceneIds[i].id == sceneId)
			return kSceneIds[i].name;
	}
	return Common::String();
}

int sceneIdForName(const Common::String &name) {
	const Common::String lookup = canonicalSceneName(name);
	for (uint i = 0; i < ARRAYSIZE(kSceneIds); i++) {
		if (lookup.equalsIgnoreCase(kSceneIds[i].name))
			return kSceneIds[i].id;
	}
	return -1;
}

SceneBackdropNames backdropNamesForScene(const Common::String &sceneName) {
	const Common::String lookup = canonicalSceneName(sceneName);
	for (uint i = 0; i < ARRAYSIZE(kBackdrops); i++) {
		if (lookup.equalsIgnoreCase(kBackdrops[i].scene))
			return { kBackdrops[i].arrival, kBackdrops[i].departure };
	}
	return { nullptr, nullptr };
}

Common::Array<NavEdge> navEdgesForScene(const Common::String &sceneNameIn) {
	const Common::String sceneName = canonicalSceneName(sceneNameIn);
	Common::Array<NavEdge> edges;

	if (sceneName.equalsIgnoreCase("musee")) {
		// 11 direct spokes, in the exact VA-ascending order the museum's
		// click dispatcher (fcn.0042be70) writes them in
		// (scene-load-findings.md sec.7.2). Only ONE real .3DI box was
		// decoded for musee (BOX.3DI; the BOX1-4.3DI state variants all
		// yield the SAME single box position, most likely a fixed
		// exit/floor collision volume rather than one-box-per-painting --
		// see box3di.cpp's per-scene notes), so it's bound to the first
		// spoke; the other 10 are an authored on-screen region grid,
		// since the real per-painting screen geometry is runtime data
		// this engine has no way to recover (scene-load-findings.md
		// sec.7.5). Note: box0's real projection at musee's literal
		// camera pose lands entirely off the visible 640x480 frame --
		// kept as honest data (it IS the real decoded/projected box)
		// even though it isn't clickable in practice at this fixed pose.
		edges.push_back({ 0, Common::Rect(), kSceneChamp, "-> champ" });
		edges.push_back({ -1, Common::Rect(10, 10, 126, 225), kSceneEglise, "-> eglise" });
		edges.push_back({ -1, Common::Rect(136, 10, 252, 225), kSceneTerrasse, "-> terrasse" });
		edges.push_back({ -1, Common::Rect(262, 10, 378, 225), kSceneMaisonet, "-> maisonet" });
		edges.push_back({ -1, Common::Rect(388, 10, 504, 225), kSceneChambreb, "-> chambreb/v" });
		edges.push_back({ -1, Common::Rect(514, 10, 630, 225), kSceneHopiint, "-> hopiint" });
		edges.push_back({ -1, Common::Rect(10, 245, 126, 460), kSceneCafe, "-> cafe" });
		edges.push_back({ -1, Common::Rect(136, 245, 252, 460), kSceneJardin, "-> jardin" });
		edges.push_back({ -1, Common::Rect(262, 245, 378, 460), kScenePont, "-> pont" });
		edges.push_back({ -1, Common::Rect(388, 245, 504, 460), kSceneMaisonj, "-> maisonj" });
		edges.push_back({ -1, Common::Rect(514, 245, 630, 460), kSceneMangeurs, "-> mangeurs" });
	} else if (sceneName.equalsIgnoreCase("auberge")) {
		edges.push_back({ 0, Common::Rect(), kSceneJardin, "-> jardin (walk)" });
	} else if (sceneName.equalsIgnoreCase("hopiext")) {
		edges.push_back({ -1, kRegionLeft, kSceneHopiint, "-> hopiint (walk)" });
		edges.push_back({ -1, kRegionRight, kScenePont, "-> pont (walk)" });
	} else if (sceneName.equalsIgnoreCase("maisonet")) {
		edges.push_back({ -1, kRegionLeft, kSceneMangeurs, "-> mangeurs (walk)" });
	} else if (sceneName.equalsIgnoreCase("mangeurs")) {
		edges.push_back({ 0, Common::Rect(), kSceneMaisonet, "-> maisonet (walk)" });
	} else if (sceneName.equalsIgnoreCase("cafe")) {
		edges.push_back({ 0, Common::Rect(), kSceneTerrasse, "-> terrasse (walk, \"porte01\"/\"porte02\")" });
		// cafe's 2nd box: the real room also has bespoke gant/mirroir/
		// lampe4/orloge examine-object clicks (scene-load-findings.md
		// sec.7.3/sec.4.3) that aren't scene transitions at all -- bound
		// here to kActionLocal as a single representative stub so the
		// committer's "-1 = local action" path is exercised by a real
		// click too, per this milestone's "stub as logged no-op" brief.
		edges.push_back({ 1, Common::Rect(), kActionLocal, "examine object (gant/mirroir/lampe4/orloge stub)" });
	} else if (sceneName.equalsIgnoreCase("chambreb")) {
		edges.push_back({ 0, Common::Rect(), kSceneMaisonj, "-> maisonj (walk)" });
	} else if (sceneName.equalsIgnoreCase("maisonj")) {
		// All 3 of maisonj's local edges use the game's raw 2D
		// coordinate-range hit-test shortcut, not the generic .3DI box
		// picker (scene-flow.md sec.3.2, sec.5 "what's not statically
		// resolvable"; fcn.0041c9d0) -- region-based by design, even
		// though maisonj does have real decoded .3DI boxes (8, still
		// available via hotspotBoxes()/the `hotspots` console command
		// for other, non-nav local objects/props).
		edges.push_back({ -1, kRegionLeft, kSceneChambreb, "-> chambreb/v (walk, coord-range)" });
		edges.push_back({ -1, kRegionRight, kScenePont, "-> pont (walk, coord-range)" });
		edges.push_back({ -1, kRegionBottom, kSceneTerrasse, "-> terrasse (walk, coord-range)" });
	} else if (sceneName.equalsIgnoreCase("hopiint")) {
		edges.push_back({ -1, kRegionLeft, kSceneHopiext, "-> hopiext (walk)" });
	} else if (sceneName.equalsIgnoreCase("pont")) {
		edges.push_back({ 0, Common::Rect(), kSceneHopiext, "-> hopiext (walk)" });
		edges.push_back({ 1, Common::Rect(), kSceneMaisonj, "-> maisonj (walk)" });
	} else if (sceneName.equalsIgnoreCase("terrasse")) {
		edges.push_back({ 0, Common::Rect(), kSceneCafe, "-> cafe (walk)" });
		edges.push_back({ -1, kRegionLeft, kSceneMaisonj, "-> maisonj (walk)" });
	} else if (sceneName.equalsIgnoreCase("jardin")) {
		// Matches the discovery order of the 3 real decoded boxes 1:1
		// with scene_hotspots.py's KNOWN_ACTIONS['jardin'] listed order.
		edges.push_back({ 0, Common::Rect(), kSceneAuberge, "-> auberge (walk, \"porte02\"-gated)" });
		edges.push_back({ 1, Common::Rect(), kSceneChamp, "-> champ (walk)" });
		edges.push_back({ 2, Common::Rect(), kSceneEglise, "-> eglise (walk)" });
	} else if (sceneName.equalsIgnoreCase("champ")) {
		edges.push_back({ 0, Common::Rect(), kSceneEglise, "-> eglise (walk)" });
		edges.push_back({ -1, kRegionLeft, kSceneJardin, "-> jardin (walk)" });
	} else if (sceneName.equalsIgnoreCase("eglise")) {
		edges.push_back({ -1, kRegionLeft, kSceneChamp, "-> champ (walk)" });
		edges.push_back({ -1, kRegionRight, kSceneJardin, "-> jardin (walk)" });
	}

	return edges;
}

NavAction resolveNavAction(int actionArg) {
	NavAction action;

	if (actionArg == kActionQuit) {
		action.kind = NavAction::kQuit;
	} else if (actionArg == kActionReturnToHub) {
		action.kind = NavAction::kHub;
		action.targetScene = sceneNameForId(kSceneMusee);
	} else if (actionArg == kActionLocal) {
		action.kind = NavAction::kLocal;
	} else {
		const Common::String name = sceneNameForId(actionArg);
		if (name.empty()) {
			warning("Vangogh: nav: unknown actionArg %d, treating as a local no-op", actionArg);
			action.kind = NavAction::kLocal;
		} else {
			action.kind = NavAction::kTransition;
			action.targetScene = name;
		}
	}

	return action;
}

} // End of namespace Vangogh
