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

#include "vangogh/vangogh.h"
#include "vangogh/renderer3d.h"
#include "vangogh/detection.h"
#include "vangogh/console.h"
#include "vangogh/hnmplayer.h"
#include "vangogh/spr.h"
#include "vangogh/world.h"
#include "vangogh/tgp.h"
#include "common/archive.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/tokenizer.h"
#include "common/file.h"
#include "common/rect.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"
#include "engines/util.h"
#include "graphics/cursorman.h"
#include "graphics/surface.h"
#include "image/tga.h"
#ifdef USE_PNG
#include "image/png.h"
#endif

namespace Vangogh {

VangoghEngine *g_engine;

VangoghEngine::VangoghEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _randomSource("Vangogh"), _world(nullptr), _sound(new Sound()) {
	g_engine = this;
}

VangoghEngine::~VangoghEngine() {
	delete _sound;
	delete _screen;
}

uint32 VangoghEngine::getFeatures() const {
	return _gameDescription->flags;
}

Common::String VangoghEngine::getGameId() const {
	return _gameDescription->gameId;
}

Common::Error VangoghEngine::run() {
	// The "data" tree is two levels deep for what we need right now (e.g.
	// data/local/intro.ini); depth 2 caches both levels. Matching is
	// case-insensitive, which matters here: the disk copy mixes
	// "schermo1.tga"/"schermo2.tga" with "Schermo3.tga".
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "data", 0, 2);

	// Headless debug aid: vangogh_render3d=<scene>:<out.png> renders the scene's
	// 3D model with its default pose to a PNG and exits, with no graphics init.
	if (ConfMan.hasKey("vangogh_render3d")) {
		const Common::String spec = ConfMan.get("vangogh_render3d");
		const size_t sep = spec.findFirstOf(':');
		if (sep == Common::String::npos) {
			warning("Vangogh: vangogh_render3d expects <scene>:<out.png>");
			return Common::kUnknownError;
		}
		const bool ok = renderSceneToPNG(spec.substr(0, sep), spec.substr(sep + 1), nullptr);
		quitGame();	// don't fall back to the launcher
		return ok ? Common::kNoError : Common::kUnknownError;
	}

	// Set the engine's debugger console
	setDebugger(new Console());

	// The original splash screens are pre-rendered at 640x480; verified
	// directly against data/local/schermo{1,2,3}.tga. A null format asks
	// for the backend's preferred true color mode, since the default
	// CLUT8 mode can't display these directly.
	initGraphics(640, 480, nullptr);
	_screen = new Graphics::Screen();

	// Headless test harness: no splashes, no intro, just the scripted world.
	if (ConfMan.hasKey("vangogh_script")) {
		runScript(ConfMan.get("vangogh_script"));
		quitGame();	// don't fall back to the launcher
		return Common::kNoError;
	}

	showIntro();

	// Real intro cutscene (data/movies/intro.hnm, scene-flow.md sec.1.5:
	// "playMovie(hwndMain, 'intro')") -- playVideo() already no-ops
	// gracefully (with a warning) if the file is missing.
	if (!shouldQuit())
		playVideo("intro");

	// New games start in the museum (world.md sec.8.1); the ScummVM launcher
	// replaces the original profile dialog.
	if (!shouldQuit())
		runWorld();

	return Common::kNoError;
}

void VangoghEngine::showIntro() {
	Common::File iniFile;
	if (!iniFile.open(Common::Path("local/intro.ini"))) {
		warning("Vangogh: could not open data/local/intro.ini");
		return;
	}

	while (!iniFile.eos() && !shouldQuit()) {
		Common::String line = iniFile.readLine();
		line.trim();
		if (line.empty())
			continue;

		const size_t sep = line.findFirstOf(' ');
		if (sep == Common::String::npos) {
			warning("Vangogh: ignoring malformed intro.ini line '%s'", line.c_str());
			continue;
		}

		const Common::String name = line.substr(0, sep);
		const uint32 durationMs = (uint32)atoi(line.c_str() + sep + 1);

		showSplashImage(name, durationMs);
	}
}

void VangoghEngine::showSplashImage(const Common::String &name, uint32 durationMs) {
	const Common::Path tgaPath(Common::String::format("local/%s.tga", name.c_str()));

	Common::File tgaFile;
	if (!tgaFile.open(tgaPath)) {
		warning("Vangogh: could not open splash image %s", tgaPath.toString().c_str());
		return;
	}

	Image::TGADecoder decoder;
	if (!decoder.loadStream(tgaFile)) {
		warning("Vangogh: failed to decode splash image %s", tgaPath.toString().c_str());
		return;
	}

	debug("Vangogh: showing splash '%s' for %u ms", name.c_str(), durationMs);

	// The source TGAs set the 1-bit "attribute" field of the image
	// descriptor without ever using it as real alpha (it's 0 for every
	// pixel). Reinterpreting the decoded surface as fully opaque avoids
	// ManagedSurface::blitFrom treating the whole image as transparent.
	Graphics::Surface opaque = *decoder.getSurface();
	opaque.format = Graphics::PixelFormat(opaque.format.bytesPerPixel,
		opaque.format.rBits(), opaque.format.gBits(), opaque.format.bBits(), 0,
		opaque.format.rShift, opaque.format.gShift, opaque.format.bShift, 0);

	const Common::Point dest((_screen->w - opaque.w) / 2, (_screen->h - opaque.h) / 2);
	_screen->blitFrom(opaque, dest);
	_screen->update();

	waitMillis(durationMs);
}

void VangoghEngine::playVideo(const Common::String &name) {
	const Common::Path hnmPath(Common::String::format("movies/%s.hnm", name.c_str()));

	HNMPlayer player;
	if (!player.load(hnmPath, /*loop=*/false))
		return;

	const uint16 width = player.width();
	const uint16 height = player.height();

	bool skipped = false;
	while (!shouldQuit() && !player.endOfVideo() && !skipped) {
		if (player.decodeNextFrame()) {
			const Graphics::Surface *frame = player.currentSurface();
			if (frame)
				g_system->copyRectToScreen(frame->getPixels(), frame->pitch, 0, 0, width, height);
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

	debug("Vangogh: movie '%s' decoded %d/%u frames%s", name.c_str(),
		player.getCurFrame() + 1, player.getFrameCount(),
		skipped ? " (skipped)" : "");
}

void VangoghEngine::showTGPImage(const Common::String &name, uint32 durationMs) {
	const Common::Path tgpPath(Common::String::format("gfx/%s.TGP", name.c_str()));

	Common::File tgpFile;
	if (!tgpFile.open(tgpPath)) {
		warning("Vangogh: could not open TGP image %s", tgpPath.toString().c_str());
		return;
	}

	TGPDecoder decoder;
	if (!decoder.loadStream(tgpFile)) {
		warning("Vangogh: failed to decode TGP image %s", tgpPath.toString().c_str());
		return;
	}

	const Graphics::Surface *surface = decoder.getSurface();
	debug("Vangogh: showing TGP '%s' (%dx%d) for %u ms", name.c_str(), surface->w, surface->h, durationMs);

	const Common::Point dest((_screen->w - surface->w) / 2, (_screen->h - surface->h) / 2);
	_screen->blitFrom(*surface, dest);
	_screen->update();

	waitMillis(durationMs);
}

void VangoghEngine::showSPRCell(const Common::String &name, uint32 cellIndex, uint32 durationMs) {
	const Common::Path sprPath(Common::String::format("sprites/%s.spr", name.c_str()));

	Common::File sprFile;
	if (!sprFile.open(sprPath)) {
		warning("Vangogh: could not open SPR sprite %s", sprPath.toString().c_str());
		return;
	}

	SPRFile spr;
	if (!spr.load(sprFile)) {
		warning("Vangogh: failed to parse SPR sprite %s", sprPath.toString().c_str());
		return;
	}

	if (cellIndex >= spr.numCells()) {
		warning("Vangogh: SPR '%s' has %u cell(s), index %u is out of range", name.c_str(), spr.numCells(), cellIndex);
		return;
	}

	if (spr.isCompressed() && !spr.hasPalette()) {
		warning("Vangogh: SPR '%s' is Codec-C compressed but carries no palette, cannot decode", name.c_str());
		return;
	}

	Graphics::Surface *cell = spr.decodeCell(cellIndex);
	if (!cell)
		return; // decodeCell() already logged a warning().

	if (cell->w == 0 || cell->h == 0) {
		debug("Vangogh: SPR '%s' cell %u is an empty placeholder (0x0), nothing to show", name.c_str(), cellIndex);
		cell->free();
		delete cell;
		return;
	}

	debug("Vangogh: showing SPR '%s' cell %u (%dx%d, %s%s) for %u ms", name.c_str(), cellIndex, cell->w, cell->h,
		spr.hasPalette() ? "CLUT8" : "RGB565", spr.isCompressed() ? ", transparent" : ", opaque", durationMs);

	const Common::Point dest((_screen->w - cell->w) / 2, (_screen->h - cell->h) / 2);
	if (spr.isCompressed()) {
		// Codec-C cells: blit with the color key its transparent-skip
		// opcode decodes to, so skipped pixels leave the screen untouched.
		_screen->transBlitFrom(*cell, dest, SPRFile::kTransparentColor, false, 0xff, &spr.getPalette());
	} else if (spr.hasPalette()) {
		_screen->blitFrom(*cell, dest, &spr.getPalette());
	} else {
		_screen->blitFrom(*cell, dest);
	}
	_screen->update();

	cell->free();
	delete cell;

	waitMillis(durationMs);
}

namespace {

// Original frame period (timer sub_4336b0, 66 ms) and ticks per second of the exe's clock.
const uint32 kFrameMs = 66;
const float kTicksPerMs = 30.0f / 1000.0f;

/** Folds one ScummVM event into the per-frame input (held keys persist, edges are cleared by the caller). */
void mapWorldEvent(const Common::Event &event, WorldInput &in) {
	switch (event.type) {
	case Common::EVENT_MOUSEMOVE:
		in.mouseX = CLIP<int>(event.mouse.x, 0, 639);
		in.mouseY = CLIP<int>(event.mouse.y, 0, 479);
		break;
	case Common::EVENT_LBUTTONDOWN:
		in.mouseX = CLIP<int>(event.mouse.x, 0, 639);
		in.mouseY = CLIP<int>(event.mouse.y, 0, 479);
		in.click = true;
		break;
	case Common::EVENT_KEYDOWN:
	case Common::EVENT_KEYUP: {
		const bool down = event.type == Common::EVENT_KEYDOWN;
		const bool edge = down && !event.kbdRepeat;
		switch (event.kbd.keycode) {
		case Common::KEYCODE_UP: in.up = down; break;
		case Common::KEYCODE_DOWN: in.down = down; break;
		case Common::KEYCODE_LEFT: in.left = down; break;
		case Common::KEYCODE_RIGHT: in.right = down; break;
		case Common::KEYCODE_PAGEUP: in.pgUp = down; break;
		case Common::KEYCODE_PAGEDOWN: in.pgDown = down; break;
		case Common::KEYCODE_LALT:
		case Common::KEYCODE_RALT: in.alt = down; break;
		case Common::KEYCODE_F1: in.f1 |= edge; break;
		case Common::KEYCODE_BACKSPACE: in.backspace |= edge; break;
		case Common::KEYCODE_SPACE: in.space |= edge; break;
		case Common::KEYCODE_ESCAPE: in.esc |= edge; break;
		case Common::KEYCODE_RETURN:
		case Common::KEYCODE_KP_ENTER: in.click |= edge; break;	// Enter is the keyboard click
		default: break;
		}
		break;
	}
	default:
		break;
	}
}

inline void clearEdges(WorldInput &in) {
	in.f1 = in.backspace = in.space = in.esc = in.click = false;
}

} // end of anonymous namespace

void VangoghEngine::presentWorld(const World &world) {
	_screen->blitFrom(world.surface());
	_screen->update();
}

void VangoghEngine::runWorld() {
	World world;
	if (!world.init())
		warning("Vangogh: mouse cursors unavailable, using the system cursor");
	if (!world.enterScene(kSceneMusee, 0)) {
		warning("Vangogh: could not load the museum scene");
		return;
	}
	_world = &world;
	CursorMan.showMouse(true);

	WorldInput in;
	uint32 last = g_system->getMillis();
	while (!shouldQuit()) {
		const uint32 frameStart = g_system->getMillis();

		clearEdges(in);
		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event))
			mapWorldEvent(event, in);

		// dt in original ticks; the exe's tick counter (dat_5e4040) is clamped to 1..10.
		const float dt = CLIP<float>((float)(frameStart - last) * kTicksPerMs, 1.0f, 10.0f);
		last = frameStart;

		world.frame(in, dt);
		presentWorld(world);
		world.updateCursor();

		if (world.takeMenuRequest())
			openMainMenuDialog();
		if (getDebugger())
			getDebugger()->onFrame();

		const uint32 spent = g_system->getMillis() - frameStart;
		if (spent < kFrameMs)
			g_system->delayMillis(kFrameMs - spent);
	}
	_world = nullptr;
}

#ifdef USE_PNG
static bool writeSurface(const Graphics::Surface &surf, const Common::String &path) {
	Common::DumpFile out;
	const bool ok = out.open(Common::Path(path, '/')) && Image::writePNG(out, surf);
	if (!ok)
		warning("Vangogh: script: could not write %s", path.c_str());
	return ok;
}

static bool writeShot(const World &world, int mouseX, int mouseY, const Common::String &path) {
	// Copy the framebuffer and draw the current cursor sprite on it, since PNGs of the surface alone carry no pointer.
	const Graphics::Surface &src = world.surface();
	Graphics::Surface shot;
	shot.copyFrom(src);

	const byte *pix, *pal;
	int w, h, hx, hy;
	if (world.cursorImage(world.cursorKind(), pix, w, h, hx, hy, pal)) {
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				const byte c = pix[y * w + x];
				const int px = mouseX - hx + x, py = mouseY - hy + y;
				if (!c || px < 0 || py < 0 || px >= shot.w || py >= shot.h)
					continue;
				const uint32 col = shot.format.RGBToColor(pal[c * 3], pal[c * 3 + 1], pal[c * 3 + 2]);
				if (shot.format.bytesPerPixel == 2)
					*(uint16 *)shot.getBasePtr(px, py) = (uint16)col;
				else
					*(uint32 *)shot.getBasePtr(px, py) = col;
			}
		}
	}

	const bool ok = writeSurface(shot, path);
	shot.free();
	return ok;
}
#endif

void VangoghEngine::runScript(const Common::String &path) {
	// Script commands, one per line ('#' starts a comment):
	//   hold <keys>        comma list of up,down,left,right,pgup,pgdn,alt held from now on (none = release all)
	//   press <key>        one-frame edge for f1, backspace, space, esc or enter
	//   mouse <x> <y>      move the mouse
	//   click              left click, delivered on the next frame
	//   step <n> [rt]      run n 66 ms frames with the current input (rt = paced in real time, needed for clips and voices)
	//   snd <name> [loop]  load+play a sound (Sound::load prefixes: plain, '!' APC, '_' WAV stream), wait, log
	//   goto <scene>       enter a scene as if coming from the museum
	//   pose x y z a b     teleport the camera
	//   box <n>            SetBox(n): switch the active collision volume (0 = BOX.3DI, 1.. = the scene's variants)
	//   newgame            reset the game state (then goto a scene to apply it)
	//   flag <name> [v]    log (or set) a game state flag: named, A<n>, B<n>, inv<n> or 0x<offset>
	//   flags              log every set flag
	//   anim <sel> <op> [n]  3DA animations: op = start, stop, end, pose n, tick n, check ("anim list" lists them)
	//   objects            log the scene's objects with their runtime types
	//   reach              log the museum paintings' distance to the walkable floor
	//   movie <name> <frame> <out.png>  play a movie-table clip (CVY mask included) to frame N and save its surface
	//   print              log scene, pose, hovered object and cursor kind
	//   shot <file.png>    save the current frame with the cursor drawn on it
	Common::FSNode scriptNode(Common::Path(path, '/'));
	Common::SeekableReadStream *stream = scriptNode.createReadStream();
	if (!stream) {
		warning("Vangogh: script: cannot open %s", path.c_str());
		return;
	}

	World world;
	if (!world.init())
		warning("Vangogh: mouse cursors unavailable");
	if (!world.enterScene(kSceneMusee, 0)) {
		delete stream;
		return;
	}
	_world = &world;

	WorldInput in;
	const float dt = (float)kFrameMs * kTicksPerMs;
	int lineNo = 0;
	while (!stream->eos() && !shouldQuit()) {
		Common::String line = stream->readLine();
		lineNo++;
		const size_t hash = line.findFirstOf('#');
		if (hash != Common::String::npos)
			line = line.substr(0, hash);
		line.trim();
		if (line.empty())
			continue;

		Common::StringTokenizer tok(line, " \t");
		const Common::String cmd = tok.nextToken();
		const Common::String a1 = tok.nextToken();
		const Common::String a2 = tok.nextToken();
		const Common::String a3 = tok.nextToken();
		const Common::String a4 = tok.nextToken();
		const Common::String a5 = tok.nextToken();

		if (cmd == "hold") {
			in.up = in.down = in.left = in.right = in.pgUp = in.pgDown = in.alt = false;
			Common::StringTokenizer keys(a1, ",");
			while (!keys.empty()) {
				const Common::String k = keys.nextToken();
				if (k == "up") in.up = true;
				else if (k == "down") in.down = true;
				else if (k == "left") in.left = true;
				else if (k == "right") in.right = true;
				else if (k == "pgup") in.pgUp = true;
				else if (k == "pgdn") in.pgDown = true;
				else if (k == "alt") in.alt = true;
				else warning("Vangogh: script:%d: unknown key '%s'", lineNo, k.c_str());
			}
		} else if (cmd == "press") {
			if (a1 == "f1") in.f1 = true;
			else if (a1 == "backspace") in.backspace = true;
			else if (a1 == "space") in.space = true;
			else if (a1 == "esc") in.esc = true;
			else if (a1 == "enter") in.click = true;
			else warning("Vangogh: script:%d: unknown key '%s'", lineNo, a1.c_str());
		} else if (cmd == "mouse") {
			in.mouseX = CLIP(atoi(a1.c_str()), 0, 639);
			in.mouseY = CLIP(atoi(a2.c_str()), 0, 479);
		} else if (cmd == "click") {
			in.click = true;
		} else if (cmd == "step") {
			const int n = atoi(a1.c_str());
			for (int i = 0; i < n && !shouldQuit(); i++) {
				const uint32 frameStart = g_system->getMillis();
				world.frame(in, dt);
				clearEdges(in);
				presentWorld(world);
				world.updateCursor();
				if (a2 == "rt" && g_system->getMillis() - frameStart < kFrameMs)
					g_system->delayMillis(kFrameMs - (g_system->getMillis() - frameStart));
			}
		} else if (cmd == "snd") {
			// snd <name> [loop]: load+play via Sound, wait (max 3 s real time), log state
			const uint16 h = _sound->load(a1);
			if (!h) {
				warning("Vangogh: script:%d: cannot load sound '%s'", lineNo, a1.c_str());
			} else {
				_sound->play(h, a2 == "loop");
				debug("SCRIPT snd '%s' handle=%u duration=%u ms playing=%d", a1.c_str(), h, _sound->durationMs(h), _sound->isPlaying(h));
				const uint32 end = g_system->getMillis() + MIN<uint32>(_sound->durationMs(h) + 300, 3000);
				while (g_system->getMillis() < end && _sound->isPlaying(h) && !shouldQuit())
					g_system->delayMillis(20);
				debug("SCRIPT snd '%s' after wait playing=%d", a1.c_str(), _sound->isPlaying(h));
				_sound->stop(h);
			}
		} else if (cmd == "goto") {
			const int id = World::sceneIdByName(a1);
			if (id < 0 || !world.enterScene(id, 0))
				warning("Vangogh: script:%d: cannot enter scene '%s'", lineNo, a1.c_str());
		} else if (cmd == "pose") {
			Pose3D p;
			p.x = atoi(a1.c_str());
			p.y = atoi(a2.c_str());
			p.z = atoi(a3.c_str());
			p.rotA = atoi(a4.c_str());
			p.rotB = atoi(a5.c_str());
			world.setPose(p);
		} else if (cmd == "box") {
			if (!world.setBox(atoi(a1.c_str())))
				warning("Vangogh: script:%d: scene has no collision volume %s", lineNo, a1.c_str());
		} else if (cmd == "newgame") {
			world.newGame();
		} else if (cmd == "flag" || cmd == "flags") {
			Common::String out;
			uint offset;
			if (cmd == "flags") {
				world.state().describe(out);
			} else if (!world.state().findFlag(a1, offset)) {
				warning("Vangogh: script:%d: unknown flag '%s'", lineNo, a1.c_str());
			} else {
				if (!a2.empty())
					world.state().set(offset, (uint32)strtoul(a2.c_str(), nullptr, 0));
				out = Common::String::format("%s (0x%03x) = %u\n", a1.c_str(), offset, world.state().get(offset));
			}
			debug("SCRIPT %s\n%s", cmd.c_str(), out.c_str());
		} else if (cmd == "anim" || cmd == "objects" || cmd == "reach") {
			Common::String out;
			bool ok = true;
			if (cmd == "anim")
				ok = world.animCommand(a1 == "list" ? Common::String() : a1, a2.empty() ? "list" : a2, atoi(a3.c_str()), out);
			else if (cmd == "objects")
				world.describeObjects(out);
			else
				world.describeReach(out);
			if (!ok)
				warning("Vangogh: script:%d: %s failed", lineNo, line.c_str());
			debug("SCRIPT %s %s\n%s", cmd.c_str(), a1.c_str(), out.c_str());
		} else if (cmd == "movie") {
			// movie <name> <frame> <out.png>: plays the clip in real time until frame N has been composed
			MovieClip clip;
			if (!clip.open(a1)) {
				warning("Vangogh: script:%d: cannot open movie '%s'", lineNo, a1.c_str());
			} else {
				const int target = atoi(a2.c_str());
				const uint32 limit = g_system->getMillis() + 60000;
				while (!clip.done() && clip.frame() < target && !shouldQuit() && g_system->getMillis() < limit) {
					clip.step();
					g_system->delayMillis(5);
				}
				debug("SCRIPT movie '%s' frame=%d done=%d mask=%d", a1.c_str(), clip.frame(), clip.done(), clip.hasMask());
#ifdef USE_PNG
				writeSurface(clip.surface(), a3);
#endif
				clip.close();
			}
		} else if (cmd == "print") {
			const Pose3D p = world.pose();
			debug("SCRIPT %s scene=%s box=%d pose=%d,%d,%d rot=%d,%d hover='%s' cursor=%d", a1.c_str(),
				world.sceneName(world.scene()).c_str(), world.box(), p.x, p.y, p.z, p.rotA, p.rotB, world.hoverName(), world.cursorKind());
		} else if (cmd == "shot") {
#ifdef USE_PNG
			writeShot(world, in.mouseX, in.mouseY, a1);
#else
			warning("Vangogh: script: this build has no PNG support");
#endif
		} else {
			warning("Vangogh: script:%d: unknown command '%s'", lineNo, cmd.c_str());
		}
	}

	delete stream;
	_world = nullptr;
}

void VangoghEngine::waitMillis(uint32 ms) {
	const uint32 startMs = g_system->getMillis();
	while (!shouldQuit() && g_system->getMillis() - startMs < ms) {
		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			if (event.type == Common::EVENT_QUIT || event.type == Common::EVENT_RETURN_TO_LAUNCHER)
				return;
		}
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
}

} // End of namespace Vangogh
