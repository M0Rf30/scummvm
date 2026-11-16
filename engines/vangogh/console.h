
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

#ifndef VANGOGH_CONSOLE_H
#define VANGOGH_CONSOLE_H

#include "gui/debugger.h"

namespace Vangogh {

class Console : public GUI::Debugger {
private:
	bool Cmd_test(int argc, const char **argv);

	/** `playhnm <basename>` - plays data/movies/<basename>.hnm. */
	bool Cmd_playHNM(int argc, const char **argv);

	/** `showtgp <basename>` - decodes and blits data/gfx/<basename>.TGP for 5s. */
	bool Cmd_showTGP(int argc, const char **argv);

	/** `showspr <basename> [cellIndex]` - decodes and blits cell [cellIndex] (default 0) of data/sprites/<basename>.spr for 5s. */
	bool Cmd_showSPR(int argc, const char **argv);

	/** `playsnd <name> [loop]` - plays data/sound/<name>.wav ('!' prefix = APC voice, '_' = streamed WAV). */
	bool Cmd_playSnd(int argc, const char **argv);

	/** `goto <scene>` - enters a scene of the running 3D world as if arriving from the museum. */
	bool Cmd_goto(int argc, const char **argv);

	/** `pose [x y z rotA rotB]` - prints (or sets) the camera pose of the running 3D world. */
	bool Cmd_pose(int argc, const char **argv);

	/** `render3d <scene> <out.png> [x y z rotA rotB]` - software-renders scenes_3d/<scene>.bfg to a PNG (default pose if none given). */
	bool Cmd_render3D(int argc, const char **argv);

	/** `flags` - lists the game state flags that are set. */
	bool Cmd_flags(int argc, const char **argv);

	/** `setflag <name> <value>` - sets a game state flag (named, A<n>, B<n>, inv<n> or a hex offset). */
	bool Cmd_setFlag(int argc, const char **argv);

	/** `anim [<name|index> <start|stop|end|pose t|tick n|check>]` - lists or drives the scene's 3DA animations. */
	bool Cmd_anim(int argc, const char **argv);

	/** `objects` - lists the scene's pickable objects with their runtime type. */
	bool Cmd_objects(int argc, const char **argv);

	/** `reach` - museum gating: distance of each painting to the walkable floor of the active collision volume. */
	bool Cmd_reach(int argc, const char **argv);

	/** `import <GAME*.BIN|GGAME*.BIN> <slot>` - converts an original save file (native path) into a ScummVM slot. */
	bool Cmd_import(int argc, const char **argv);

	/** `page <0-24>` - opens a 2D page over the running world as if its launcher had been clicked. */
	bool Cmd_page(int argc, const char **argv);

	/** `give <item>` - carries item <0-34> in the 3D world as if it had just been picked up. */
	bool Cmd_give(int argc, const char **argv);
public:
	Console();
	~Console() override;
};

} // End of namespace Vangogh

#endif // VANGOGH_CONSOLE_H
