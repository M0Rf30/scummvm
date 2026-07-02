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

namespace Vangogh {

/**
 * A design-independent, pre-rendered-backdrop "scene": an HNM6 movie loop
 * (see HNMPlayer) plus the oriented-box hotspot volumes decoded from the
 * matching scenes_3d container's BOX.3DI record (see box3di.h). This is
 * milestone 4's playback core: it deliberately knows nothing about
 * navigation (which scene comes next -- SceneFlowRE's scene-flow reversing
 * feeds that in later), puzzle state, or the real camera/hotspot-picking
 * model (see HotspotBox::projectToScreen()'s TODO). There is no runtime
 * mesh geometry to render in this engine at all -- every scene is a
 * pre-rendered video backdrop plus these collision/hotspot volumes.
 */
class Scene {
public:
	explicit Scene(const Common::String &name);
	~Scene();

	Scene(const Scene &) = delete;
	Scene &operator=(const Scene &) = delete;

	/**
	 * Locates data/movies/<name>*.hnm for the backdrop and parses
	 * data/scenes_3d/<name>.bfg's BOX.3DI record into hotspotBoxes(). The
	 * two can fail independently (a warning() each, following every other
	 * loader in this engine); this only returns false when BOTH fail, i.e.
	 * there is nothing at all to show for this scene. Always logs
	 * "scene <name>: N hotspot boxes loaded" via debug(), matching the
	 * milestone brief, regardless of the backdrop's own outcome.
	 */
	bool load();

	/**
	 * Runs the backdrop loop until a keypress or quit/return-to-launcher
	 * request (matches the `scene <name>` console command's contract). A
	 * mouse click logs its coordinates and any hotspotBoxes() entry whose
	 * (placeholder, see box3di.h) 2D projection contains it, but does NOT
	 * leave the loop -- clicking is the scene's own interaction, not an
	 * exit gesture. Under automated/headless runs (ConfMan "boot_param"
	 * set -- see VangoghEngine::run()/showAccueil(), which have no real
	 * input device to ever deliver a keypress) the loop also leaves on its
	 * own after a small, fixed number of backdrop frames.
	 */
	void run();

	const Common::String &name() const { return _name; }
	const Common::Array<HotspotBox> &hotspotBoxes() const { return _hotspotBoxes; }
	bool hasBackdrop() const { return _hasBackdrop; }

private:
	void handleClick(const Common::Point &pt) const;

	Common::String _name;
	HNMPlayer _player;
	bool _hasBackdrop;
	Common::Array<HotspotBox> _hotspotBoxes;
};

} // End of namespace Vangogh

#endif // VANGOGH_SCENE_H
