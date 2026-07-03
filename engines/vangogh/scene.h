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

#ifndef VANGOGH_SCENE_H
#define VANGOGH_SCENE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "vangogh/box3di.h"
#include "vangogh/hnmplayer.h"
#include "vangogh/navgraph.h"

namespace Vangogh {

/**
 * One playable room: a per-scene FIXED camera (box3di.h's CameraPose)
 * projecting the `.3DI` hotspot boxes decoded from the matching
 * scenes_3d/<name>.bfg's BOX.3DI record into 640x480 screen space, a pair
 * of one-shot HNM backdrop clips (plain-named ARRIVAL, `r`-suffixed
 * DEPARTURE -- see navgraph.h's backdropNamesForScene(), never looped,
 * never auto-chained), and this scene's outgoing navigation edges
 * (navgraph.h's navEdgesForScene()). run() plays the arrival clip once,
 * holds its last frame as the static room view, waits for a hotspot
 * click/keyboard sentinel, commits the resulting action through
 * resolveNavAction() (mirroring PEINTRE.exe's fcn.00432e10), and -- if
 * the action leaves the room -- plays the departure clip once before
 * returning.
 */
class Scene {
public:
	explicit Scene(const Common::String &name);
	~Scene();

	Scene(const Scene &) = delete;
	Scene &operator=(const Scene &) = delete;

	/**
	 * One decoded hotspot box's cached zoom-0 screen projection (computed
	 * once here, in load(), against this scene's fixed camera pose) plus
	 * whatever nav edge, if any, navEdgesForScene() bound to its index --
	 * this is exactly the "projected boxes+targets" the `hotspots`
	 * console command and the boot-flow log print.
	 */
	struct ProjectedHotspot {
		uint32 boxIndex = 0;
		bool visible = false;    ///< false if every corner is behind the camera at this pose (see HotspotBox::projectToScreen()).
		Common::Rect rect;
		double nearestDepth = 0.0;
		bool hasAction = false;  ///< true if a NavEdge is bound to this box index.
		int actionArg = 0;       ///< valid iff hasAction.
		Common::String actionLabel; ///< valid iff hasAction, e.g. "-> jardin (walk)".
	};

	/**
	 * Locates data/scenes_3d/<name>.bfg's BOX.3DI hotspot boxes, this
	 * scene's camera pose (box3di.h's cameraPoseForScene()), backdrop
	 * movie basenames and outgoing nav edges (navgraph.h), and projects
	 * every decoded box through the camera once (see projectedHotspots()).
	 * The BFG lookup and the arrival backdrop's existence check can fail
	 * independently (a warning() each); this only returns false when
	 * there is NOTHING at all to show or do for this scene (no backdrop,
	 * no hotspot boxes, AND no nav edges -- e.g. an unknown scene name).
	 * Always logs "scene <name>: N hotspot boxes loaded" and "projected N
	 * hotspots" via debug(), matching the milestone brief.
	 */
	bool load();

	/**
	 * Plays the arrival clip to completion (one-shot, skippable via any
	 * key/click, quit-responsive -- never looped), then loops holding its
	 * last decoded frame as the static room view while waiting for a
	 * hotspot click, the Backspace "return to hub" sentinel, or Escape
	 * ("leave with no action"). A resolved click is committed through
	 * resolveNavAction() (navgraph.h); once that commit actually leaves
	 * the room (a transition or hub result -- not a local/-1 action),
	 * the departure clip (if this scene has one) plays one-shot before
	 * returning. Under automated/headless runs (ConfMan "boot_param" --
	 * no real input device ever delivers a keypress/click) the loop also
	 * leaves on its own after a small, fixed number of ticks; if
	 * @p simulateClick is also set, it first synthesizes exactly one
	 * click (preferring a hotspot/region that leads to "jardin", so the
	 * milestone's own musee/jardin cross-check scenes are both exercised
	 * in one boot_param run; else the first available on-screen
	 * hotspot/region) to demonstrate a real transition end-to-end.
	 */
	void run(bool simulateClick = false);

	const Common::String &name() const { return _name; }
	const Common::Array<HotspotBox> &hotspotBoxes() const { return _hotspotBoxes; }
	const Common::Array<ProjectedHotspot> &projectedHotspots() const { return _projected; }
	const Common::Array<NavEdge> &navEdges() const { return _edges; }
	bool hasBackdrop() const { return _hasBackdrop; }
	bool hasCameraLiteral() const { return _hasCameraLiteral; }

	/** What run() resolved (transition/hub/quit/local/none) -- see navgraph.h's NavAction. */
	const NavAction &resolvedAction() const { return _resolvedAction; }

private:
	void projectHotspots();
	const NavEdge *findEdgeForBox(uint32 boxIndex) const;
	void handleClick(const Common::Point &pt);
	void commitAction(int actionArg);
	Common::Point bestSimulatedClickPoint() const;
	bool playOneShotClip(const Common::String &basename, const char *phase);

	Common::String _name;
	bool _hasBackdrop;
	Common::String _arrivalMovie;
	Common::String _departureMovie;
	Common::Array<HotspotBox> _hotspotBoxes;
	CameraPose _camera;
	bool _hasCameraLiteral;
	Common::Array<ProjectedHotspot> _projected;
	Common::Array<NavEdge> _edges;
	NavAction _resolvedAction;
	bool _leaveRequested;
};

} // End of namespace Vangogh

#endif // VANGOGH_SCENE_H
