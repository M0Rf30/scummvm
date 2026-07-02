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
#include "vangogh/vangogh.h"

namespace Vangogh {

Console::Console() : GUI::Debugger() {
	registerCmd("test",     WRAP_METHOD(Console, Cmd_test));
	registerCmd("playhnm",  WRAP_METHOD(Console, Cmd_playHNM));
	registerCmd("showtgp",  WRAP_METHOD(Console, Cmd_showTGP));
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

} // End of namespace Vangogh
