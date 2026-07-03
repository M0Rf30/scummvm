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

#ifndef VANGOGH_NAVGRAPH_H
#define VANGOGH_NAVGRAPH_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

namespace Vangogh {

/**
 * The 14-scene id enum -- ground truth per 3 independent switch tables in
 * PEINTRE.exe (scene-load-findings.md sec.1, scene-flow.md sec.2.1).
 * `chambreb`/`chambrev` share id 6 (a day/night or before/after variant
 * pair, originally selected by a global this engine doesn't track --
 * sceneIdForName()/sceneNameForId() always resolve id 6 to "chambreb").
 */
enum SceneId {
	kSceneMusee = 0,
	kSceneAuberge = 1,
	kSceneHopiext = 2,
	kSceneMaisonet = 3,
	kSceneMangeurs = 4,
	kSceneCafe = 5,
	kSceneChambreb = 6,
	kSceneMaisonj = 7,
	kSceneHopiint = 8,
	kScenePont = 9,
	kSceneTerrasse = 10,
	kSceneJardin = 11,
	kSceneChamp = 12,
	kSceneEglise = 13
};

/**
 * actionArg sentinels, exactly matching the shared transition committer's
 * contract (`fcn.00432e10`, scene-flow.md sec.4.1). Any non-negative value
 * is a SceneId (a transition target); these three are the special cases.
 */
enum {
	kActionLocal = -1,        ///< Stay in the scene (pickup/examine/anim/puzzle -- bespoke, out of this milestone's scope).
	kActionQuit = -2,         ///< Quit the whole game.
	kActionReturnToHub = -3   ///< Generic "return to musee" sentinel, not tied to a specific hotspot.
};

/** @return the scene's canonical lowercase name, or an empty string if @p sceneId is out of range. */
Common::String sceneNameForId(int sceneId);

/** @return the SceneId for @p name (case-insensitive; "chambrev" resolves to kSceneChambreb), or -1 if unknown. */
int sceneIdForName(const Common::String &name);

/**
 * A scene's plain-named one-shot ARRIVAL clip and (if any) `r`-suffixed
 * one-shot DEPARTURE clip -- scene-load-findings.md sec.2.2/sec.4: two
 * disjoint 11-case dispatchers keyed on enum ids 3..13; `musee`/`auberge`/
 * `hopiext` have no case in either (no backdrop movie at all for those 3).
 * Names are movie basenames, NOT always the scene name itself (e.g. scene
 * "maisonet" backs onto movies "maisa"/"maisr", NOT "maisonet"/"maisonetr").
 */
struct SceneBackdropNames {
	const char *arrival;   ///< nullptr if this scene has no backdrop movie at all.
	const char *departure; ///< nullptr if this scene has no departure clip (may still have an arrival).
};

/** @return @p sceneName's backdrop movie basenames (see SceneBackdropNames); both fields null if unknown/none. */
SceneBackdropNames backdropNamesForScene(const Common::String &sceneName);

/**
 * One outgoing navigation edge from a scene, resolved either by a specific
 * decoded `.3DI` hotspot box index, or by a raw on-screen region --
 * exactly the "hotspotBoxIndex-or-region" shape the milestone brief asks
 * for. `maisonj`'s 3 local edges are region-based because the real game
 * itself uses a raw 2D coordinate-range hit-test for them instead of the
 * generic box picker (scene-flow.md sec.3.2, `fcn.0041c9d0`); scenes with
 * fewer decoded boxes than recovered edges (or none at all, e.g.
 * `hopiint`/`eglise`/`maisonet`) fall back to authored on-screen regions
 * for the remainder.
 *
 * IMPORTANT HONESTY NOTE: which *specific* box/region triggers which
 * *specific* target is, for the real game, runtime data populated from
 * `.3DC`/`.3DA` heap state at scene-load time and is NOT statically
 * recoverable (scene-load-findings.md sec.7.5, scene-flow.md sec.5/sec.7
 * item 7). Bindings below are a best-effort, clearly-approximated
 * assignment (by discovery order / an authored screen layout) that
 * reproduces the real CONNECTIVITY (scene-flow.md sec.5's 23-edge graph)
 * losslessly, but not necessarily the real per-pixel binding -- see the
 * per-scene comments in navgraph.cpp.
 */
struct NavEdge {
	int boxIndex;         ///< >=0: index into the scene's decodeHotspotBoxes() result. -1: use `region` instead.
	Common::Rect region;   ///< Raw screen-space hit region; only meaningful when boxIndex < 0.
	int actionArg;         ///< fcn.00432e10 contract: a SceneId, or kActionLocal/kActionQuit/kActionReturnToHub.
	const char *label;     ///< Human-readable description for logging/console output, e.g. "-> jardin (walk)".
};

/** @return all recovered/approximated outgoing edges for @p sceneName (empty if the scene has none/is unknown). */
Common::Array<NavEdge> navEdgesForScene(const Common::String &sceneName);

/**
 * The resolved outcome of one committed hotspot action -- mirrors what
 * `fcn.00432e10` (scene-flow.md sec.4.1) actually DOES with an actionArg,
 * as opposed to NavEdge's static actionArg field.
 */
struct NavAction {
	enum Kind {
		kNone,       ///< Nothing resolved yet (no hotspot/region clicked), or the scene was left without any action (e.g. Escape).
		kLocal,      ///< Stay in the scene; a local action fired (examine/pickup/anim stub).
		kTransition, ///< Leave to targetScene.
		kHub,        ///< Leave to targetScene, which is always musee (the generic return-to-hub sentinel).
		kQuit        ///< Quit the whole game.
	};

	Kind kind = kNone;
	Common::String targetScene; ///< valid iff kind == kTransition or kind == kHub.
};

/** Implements fcn.00432e10's actionArg dispatch (scene-flow.md sec.4.1) as a pure function. */
NavAction resolveNavAction(int actionArg);

} // End of namespace Vangogh

#endif // VANGOGH_NAVGRAPH_H
