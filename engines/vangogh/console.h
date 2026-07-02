
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

	/** `scene <basename>` - enters the scene loop (backdrop + click logging) for data/scenes_3d/<basename>.bfg until a key/quit. */
	bool Cmd_scene(int argc, const char **argv);
public:
	Console();
	~Console() override;
};

} // End of namespace Vangogh

#endif // VANGOGH_CONSOLE_H
