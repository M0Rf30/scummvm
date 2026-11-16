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
#include "vangogh/renderer3d.h"
#include "vangogh/world.h"
#include "vangogh/vangogh.h"

namespace Vangogh {

Console::Console() : GUI::Debugger() {
	registerCmd("test",     WRAP_METHOD(Console, Cmd_test));
	registerCmd("playhnm",  WRAP_METHOD(Console, Cmd_playHNM));
	registerCmd("showtgp",  WRAP_METHOD(Console, Cmd_showTGP));
	registerCmd("showspr",  WRAP_METHOD(Console, Cmd_showSPR));
	registerCmd("playsnd",  WRAP_METHOD(Console, Cmd_playSnd));
	registerCmd("goto",     WRAP_METHOD(Console, Cmd_goto));
	registerCmd("pose",     WRAP_METHOD(Console, Cmd_pose));
	registerCmd("render3d", WRAP_METHOD(Console, Cmd_render3D));
	registerCmd("flags",    WRAP_METHOD(Console, Cmd_flags));
	registerCmd("setflag",  WRAP_METHOD(Console, Cmd_setFlag));
	registerCmd("anim",     WRAP_METHOD(Console, Cmd_anim));
	registerCmd("objects",  WRAP_METHOD(Console, Cmd_objects));
	registerCmd("reach",    WRAP_METHOD(Console, Cmd_reach));
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

bool Console::Cmd_playSnd(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		debugPrintf("Usage: %s <name> [loop]\n", argv[0]);
		debugPrintf("Plays data/sound/<name>.wav; '!name' = APC voice, '_name' = streamed WAV.\n");
		return true;
	}
	Sound *snd = g_engine->_sound;
	const uint16 h = snd->load(argv[1]);
	if (!h) {
		debugPrintf("Cannot load '%s'\n", argv[1]);
		return true;
	}
	snd->play(h, argc == 3 && Common::String(argv[2]) == "loop");
	debugPrintf("handle %u, %u ms, playing=%d\n", h, snd->durationMs(h), snd->isPlaying(h));
	return true;
}

bool Console::Cmd_goto(int argc, const char **argv) {
	World *world = g_engine->world();
	if (argc != 2 || !world) {
		debugPrintf("Usage: %s <scene>\n", argv[0]);
		debugPrintf("Enters a scene as if coming from the museum (needs the running 3D world). Scenes:\n");
		for (int i = 0; i < kNumScenes; i++)
			debugPrintf("  %s\n", world ? world->sceneName(i).c_str() : "(world not running)");
		return true;
	}

	const int id = World::sceneIdByName(argv[1]);
	if (id < 0) {
		debugPrintf("Unknown scene '%s'\n", argv[1]);
		return true;
	}
	if (!world->enterScene(id, 0))
		debugPrintf("Could not enter '%s' (see log)\n", argv[1]);
	return true;
}

bool Console::Cmd_pose(int argc, const char **argv) {
	World *world = g_engine->world();
	if (!world || (argc != 1 && argc != 6)) {
		debugPrintf("Usage: %s [x y z rotA rotB]\n", argv[0]);
		debugPrintf("Prints the camera pose in the running 3D world, or teleports it. rotA = pitch, rotB = yaw (4096 = 360 deg).\n");
		return true;
	}

	if (argc == 6) {
		Pose3D p;
		p.x = atoi(argv[1]);
		p.y = atoi(argv[2]);
		p.z = atoi(argv[3]);
		p.rotA = atoi(argv[4]);
		p.rotB = atoi(argv[5]);
		world->setPose(p);
	}

	const Pose3D p = world->pose();
	debugPrintf("%s: pose %d %d %d %d %d, hover '%s'\n", world->sceneName(world->scene()).c_str(), p.x, p.y, p.z, p.rotA, p.rotB, world->hoverName());
	return true;
}

bool Console::Cmd_flags(int argc, const char **argv) {
	World *world = g_engine->world();
	if (!world) {
		debugPrintf("The 3D world is not running\n");
		return true;
	}
	Common::String out;
	world->state().describe(out);
	debugPrintf("%s", out.c_str());
	return true;
}

bool Console::Cmd_setFlag(int argc, const char **argv) {
	World *world = g_engine->world();
	if (!world || argc != 3) {
		debugPrintf("Usage: %s <name> <value>\n", argv[0]);
		debugPrintf("Names: A<n> (puzzle solved), B<n>, inv<n>, 0x<offset>, or one of:\n");
		for (uint i = 0; i < GameState::numNamedFlags(); i++)
			debugPrintf("  %s\n", GameState::namedFlag(i).name);
		return true;
	}
	uint offset;
	if (!world->state().findFlag(argv[1], offset)) {
		debugPrintf("Unknown flag '%s'\n", argv[1]);
		return true;
	}
	world->state().set(offset, (uint32)strtoul(argv[2], nullptr, 0));
	debugPrintf("%s (0x%03x) = %u; re-enter the scene (goto) to apply it\n", argv[1], offset, world->state().get(offset));
	return true;
}

bool Console::Cmd_anim(int argc, const char **argv) {
	World *world = g_engine->world();
	if (!world || argc > 4) {
		debugPrintf("Usage: %s [<name|index> <start|stop|end|pose t|tick n|check>]\n", argv[0]);
		return true;
	}
	Common::String out;
	const bool ok = world->animCommand(argc >= 2 ? argv[1] : "", argc >= 3 ? argv[2] : "list", argc >= 4 ? atoi(argv[3]) : 0, out);
	debugPrintf("%s%s", out.c_str(), ok ? "" : "failed\n");
	return true;
}

bool Console::Cmd_objects(int argc, const char **argv) {
	World *world = g_engine->world();
	if (!world) {
		debugPrintf("The 3D world is not running\n");
		return true;
	}
	Common::String out;
	world->describeObjects(out);
	debugPrintf("%s", out.c_str());
	return true;
}

bool Console::Cmd_reach(int argc, const char **argv) {
	World *world = g_engine->world();
	if (!world) {
		debugPrintf("The 3D world is not running\n");
		return true;
	}
	Common::String out;
	world->describeReach(out);
	debugPrintf("%s", out.c_str());
	return true;
}

bool Console::Cmd_render3D(int argc, const char **argv) {
	if (argc != 3 && argc != 8) {
		debugPrintf("Usage: %s <scene> <out.png> [x y z rotA rotB]\n", argv[0]);
		debugPrintf("Software-renders data/scenes_3d/<scene>.bfg to a 640x480 PNG. rotA = pitch, rotB = yaw (4096 = 360 deg).\n");
		debugPrintf("Without a pose the scene's default arrival pose is used.\n");
		return true;
	}

	Pose3D pose = Pose3D();
	if (argc == 8) {
		pose.x = atoi(argv[3]);
		pose.y = atoi(argv[4]);
		pose.z = atoi(argv[5]);
		pose.rotA = atoi(argv[6]);
		pose.rotB = atoi(argv[7]);
	}

	if (renderSceneToPNG(argv[1], argv[2], argc == 8 ? &pose : nullptr))
		debugPrintf("Wrote %s\n", argv[2]);
	else
		debugPrintf("Rendering '%s' failed (see log)\n", argv[1]);
	return true;
}

} // End of namespace Vangogh
