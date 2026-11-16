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
#include "vangogh/page.h"
#include "vangogh/inventory.h"
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
#include "common/serializer.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"
#include "engines/util.h"
#include "engines/metaengine.h"
#include "graphics/cursorman.h"
#include "graphics/surface.h"
#include "image/tga.h"
#ifdef USE_PNG
#include "image/png.h"
#endif

namespace Vangogh {

VangoghEngine *g_engine;

VangoghEngine::VangoghEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _randomSource("Vangogh"), _world(nullptr), _page(new Page()),
	_pageOpen(false), _bootLoading(false), _pendingPageResume(false), _sound(new Sound()) {
	g_engine = this;
}

VangoghEngine::~VangoghEngine() {
	delete _page;
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
const uint32 kPageFrameMs = 40;	// 2D page tick (timer sub_4336b0 at 40 ms in mode 1)
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
		in.mouseDown = true;
		break;
	case Common::EVENT_LBUTTONUP:
		in.mouseDown = false;
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

const Graphics::Surface &VangoghEngine::frameSurface(const World &world) const {
	if (_ending.active())
		return _ending.surface();
	return _page->active() ? _page->surface() : world.surface();
}

void VangoghEngine::presentWorld(const World &world) {
	_screen->blitFrom(frameSurface(world));
	_screen->update();
}

uint32 VangoghEngine::framePeriod() const {
	// The movies and the credits tick like the 2D module (40 ms, timer_2d_cb).
	return (_page->active() || _ending.active()) ? kPageFrameMs : kFrameMs;
}

bool VangoghEngine::openPage(World &world, int pageIndex) {
	world.snapshotState();
	if (!_page->open(pageIndex, world.state(), _sound)) {
		warning("Vangogh: could not open page %d", pageIndex);
		return false;
	}
	debug("Vangogh: page %d (%s) opened", pageIndex, Page::pageName(pageIndex));
	setPageOpen(true);
	return true;
}

void VangoghEngine::gameStep(World &world, WorldInput &in, float dt) {
	if (_ending.active()) {
		world.setCursor(-1);
		_ending.step(in);
		if (_ending.finished())
			quitGame();	// credits_frame returns 1: timer stop + PostQuitMessage
		return;
	}

	if (_page->active()) {
		_page->tick(in);
		world.setCursor(_page->cursorKind());
		if (_page->takeMenuRequest())
			openMainMenuDialog();
		if (takePageResume()) {
			// A save made inside a page was loaded: reopen the page the state block names.
			_page->close();
			if (!openPage(world, world.state().getByte(kOfsPage)))
				setPageOpen(false);
			return;
		}
		if (_page->finished()) {
			const int target = _page->exitTarget();
			debug("Vangogh: page %d closed, target %d", _page->pageIndex(), target);
			const bool ending = _page->endingReached();
			_page->close();
			setPageOpen(false);
			if (ending) {
				// transition_commit(-1) at page 0x15 (flow.md 4.5): cinefin, cinefin2, credits, quit.
				_ending.start(false);
				return;
			}
			world.returnFromPage(target);
		}
		return;
	}

	world.frame(in, dt);
	if (world.takeFinRequest()) {
		_ending.start(true);
		return;
	}
	if (takePageResume()) {
		if (!openPage(world, world.state().getByte(kOfsPage)))
			setPageOpen(false);
		return;
	}
	int page;
	if (world.takePageRequest(page) && !openPage(world, page))
		setPageOpen(false);
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

	bootLoadSlot();

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

		gameStep(world, in, dt);
		presentWorld(world);
		world.updateCursor();

		if (world.takeMenuRequest())
			openMainMenuDialog();
		if (getDebugger())
			getDebugger()->onFrame();

		const uint32 spent = g_system->getMillis() - frameStart;
		if (spent < framePeriod())
			g_system->delayMillis(framePeriod() - spent);
	}
	// The exe quits from the credits without writing GGAME: the ending is not resumable.
	if (!_ending.active())
		saveResume();
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

static bool writeShot(const Graphics::Surface &src, const World &world, int mouseX, int mouseY, const Common::String &path) {
	// Copy the framebuffer and draw the current cursor sprite on it, since PNGs of the surface alone carry no pointer.
	Graphics::Surface shot;
	shot.copyFrom(src);

	world.drawCursor(shot, world.cursorKind(), mouseX, mouseY);

	const bool ok = writeSurface(shot, path);
	shot.free();
	return ok;
}
#endif

void VangoghEngine::botFrame(World &world, WorldInput &in, float dt) {
	gameStep(world, in, dt);
	clearEdges(in);
	presentWorld(world);
	world.updateCursor();
}

void VangoghEngine::botCommand(World &world, WorldInput &in, float dt, const Common::String &cmd, const Common::String &a1,
		const Common::String &a2, const Common::String &a3, int lineNo) {
	const int scene = world.scene();
	auto idle = [&](int n) {
		in.up = in.down = in.left = in.right = in.pgUp = in.pgDown = in.alt = false;
		for (int i = 0; i < n && !shouldQuit(); i++)
			botFrame(world, in, dt);
	};
	auto walkTo = [&](int tx, int tz, int radius, const char *what, int minRadius) {
		Common::Array<int16> path;
		botFrame(world, in, dt);
		if (!world.planWalk(tx, tz, radius, path, minRadius)) {
			warning("Vangogh: script:%d: walk %s: no route", lineNo, what);
			return false;
		}
		int frames = 0, stuck = 0;
		Pose3D last = world.pose();
		for (uint w = 0; w + 1 < path.size(); w += 2) {
			const bool final = w + 2 >= path.size();
			while (!shouldQuit()) {
				if (world.scene() != scene || world.clipActive() || _page->active() || world.glideFrames() > 0) {
					warning("Vangogh: script:%d: walk %s: the scene changed on the way", lineNo, what);
					return false;
				}
				if (world.steer(in, path[w], path[w + 1], final ? 60 : 130))
					break;
				botFrame(world, in, dt);
				if (++frames > 4000)
					break;
				if (frames % 40 == 0) {
					const Pose3D p = world.pose();
					stuck = (ABS(p.x - last.x) + ABS(p.z - last.z) < 60) ? stuck + 1 : 0;
					last = p;
					if (stuck >= 3) {
						warning("Vangogh: script:%d: walk %s: stuck at %d,%d", lineNo, what, p.x, p.z);
						idle(1);
						return false;
					}
				}
			}
			if (frames > 4000) {
				warning("Vangogh: script:%d: walk %s: gave up", lineNo, what);
				idle(1);
				return false;
			}
		}
		idle(6);
		const Pose3D p = world.pose();
		debug("SCRIPT walk %s: arrived at %d,%d after %d frames", what, p.x, p.z, frames);
		return true;
	};
	auto lookAt = [&](const char *name) {
		float c[3];
		int ok = 0;
		for (int i = 0; i < 400 && ok < 3 && !shouldQuit(); i++) {
			if (!world.nodeCentre(name, c)) {
				warning("Vangogh: script:%d: look: no node '%s'", lineNo, name);
				return false;
			}
			ok = world.turnTo(in, c) ? ok + 1 : 0;
			botFrame(world, in, dt);
		}
		idle(8);
		return ok >= 3;
	};
	auto hoverOn = [&](const char *name, bool quiet) {
		int x, y;
		if (!world.screenPointOf(name, x, y)) {
			if (!quiet)
				warning("Vangogh: script:%d: hover: '%s' is not visible", lineNo, name);
			return false;
		}
		in.mouseX = x;
		in.mouseY = y;
		idle(2);
		debug("SCRIPT hover %s: pointer %d,%d hover='%s' cursor=%d", name, x, y, world.hoverName(), world.cursorKind());
		return true;
	};

	if (cmd == "drive") {
		// drive x z frames: steer straight at the point with no route planning (the walk slides along walls).
		int f = 0;
		const int n = atoi(a3.c_str());
		botFrame(world, in, dt);
		while (!shouldQuit() && f < n && !world.steer(in, atoi(a1.c_str()), atoi(a2.c_str()), 40)) {
			botFrame(world, in, dt);
			f++;
		}
		idle(4);
		const Pose3D p = world.pose();
		debug("SCRIPT drive: at %d,%d after %d frames", p.x, p.z, f);
	} else if (cmd == "walkxz") {
		walkTo(atoi(a1.c_str()), atoi(a2.c_str()), a3.empty() ? 150 : atoi(a3.c_str()), "xz", 0);
	} else if (cmd == "walk" || cmd == "use") {
		float c[3];
		botFrame(world, in, dt);
		if (!world.nodeCentre(a1.c_str(), c)) {
			warning("Vangogh: script:%d: no node '%s'", lineNo, a1.c_str());
			return;
		}
		if (!walkTo((int)c[0], (int)c[2], a2.empty() ? 800 : atoi(a2.c_str()), a1.c_str(), atoi(a3.c_str())) || cmd == "walk")
			return;
		bool seen = lookAt(a1.c_str()) && hoverOn(a1.c_str(), true);
		// Something stands in the way (a chair, the stove): look from other sides, on a circle around the node.
		const int dmax = a2.empty() ? 800 : atoi(a2.c_str());
		const int dmin = a3.empty() ? dmax * 3 / 5 : atoi(a3.c_str());
		for (int k = 0; !seen && k < 16 && !shouldQuit(); k++) {
			const float ang = (float)k * 0.3927f;
			const int rad = (dmin + dmax) / 2;
			Common::Array<int16> probe;
			const int tx = (int)c[0] + (int)(sinf(ang) * rad), tz = (int)c[2] + (int)(cosf(ang) * rad);
			if (!world.planWalk(tx, tz, 120, probe, 0))
				continue;
			if (walkTo(tx, tz, 120, a1.c_str(), 0))
				seen = lookAt(a1.c_str()) && hoverOn(a1.c_str(), true);
		}
		if (!seen) {
			warning("Vangogh: script:%d: use %s: not visible from anywhere reachable", lineNo, a1.c_str());
		} else {
			// The target can move between the aim and the click frame (looping animations): re-aim until a click lands.
			for (int attempt = 0; attempt < 8 && !shouldQuit(); attempt++) {
				const uint before = world.clickCount();
				in.click = true;
				idle(8);
				if (world.clickCount() != before)
					break;
				debug("SCRIPT use %s: click %d missed, aiming again", a1.c_str(), attempt + 1);
				if (!hoverOn(a1.c_str(), true))
					break;
			}
		}
	} else if (cmd == "plan") {
		// plan <node> [dist]: is there a route to within dist of the node, and how long
		float c[3];
		Common::Array<int16> path;
		botFrame(world, in, dt);
		if (!world.nodeCentre(a1.c_str(), c))
			warning("Vangogh: script:%d: no node '%s'", lineNo, a1.c_str());
		else if (world.planWalk((int)c[0], (int)c[2], a2.empty() ? 800 : atoi(a2.c_str()), path, atoi(a3.c_str())))
			debug("SCRIPT plan %s: route of %u legs%s", a1.c_str(), path.size() / 2, path.empty() ? " (already in range)" : "");
		else
			debug("SCRIPT plan %s: NO ROUTE (box %d)", a1.c_str(), world.box());
	} else if (cmd == "look") {
		lookAt(a1.c_str());
	} else if (cmd == "hover") {
		hoverOn(a1.c_str(), false);
	} else if (cmd == "store") {
		in.mouseX = 320;
		in.mouseY = 470;
		// The bar only takes the drop once it has slid fully in.
		for (int i = 0; i < 60 && world.inventory().state() != Inventory::kShown; i++)
			idle(1);
		in.click = true;
		idle(4);
		in.mouseX = 320;
		in.mouseY = 240;
		idle(2);
	} else if (cmd == "skipclip") {
		const int max = a1.empty() ? 3000 : atoi(a1.c_str());
		int i = 0;
		for (; i < max && !shouldQuit() && (world.clipActive() || world.glideFrames() > 0); i++) {
			in.click = world.clipActive() && i % 4 == 3;
			botFrame(world, in, dt);
		}
		idle(3);
		debug("SCRIPT skipclip: %d frames", i);
	}
}

void VangoghEngine::runScriptStream(World &world, WorldInput &in, float dt, Common::SeekableReadStream *stream, const Common::String &name) {
	int lineNo = 0;
	while (!stream->eos() && !shouldQuit() && !_ending.finished()) {
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

		if (cmd == "walk" || cmd == "walkxz" || cmd == "drive" || cmd == "look" || cmd == "hover" || cmd == "use" || cmd == "plan" || cmd == "store" || cmd == "skipclip") {
			botCommand(world, in, dt, cmd, a1, a2, a3, lineNo);
			continue;
		}
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
		} else if (cmd == "down") {
			in.click = in.mouseDown = true;
		} else if (cmd == "up") {
			in.mouseDown = false;
		} else if (cmd == "page") {
			// Same route as clicking a launcher: the next frame hands the request to gameStep().
			world.requestPage(atoi(a1.c_str()));
			gameStep(world, in, dt);
		} else if (cmd == "include") {
			// include <file>: run another script file here (segments of a playthrough chain through ScummVM saves)
			Common::FSNode node(Common::Path(a1, '/'));
			Common::SeekableReadStream *sub = node.createReadStream();
			if (!sub) {
				warning("Vangogh: script:%d: cannot include %s", lineNo, a1.c_str());
			} else {
				debug("SCRIPT include %s", a1.c_str());
				runScriptStream(world, in, dt, sub, a1);
				delete sub;
			}
		} else if (cmd == "pwait") {
			// pwait <state[,state...]> <max> [skip]: page ticks (real time) until the 2D page reaches one of the states or
			// closes; "skip" holds the button in short pulses so skippable clips and voices end
			const int max = atoi(a2.c_str());
			const bool skip = a3 == "skip";
			int i = 0;
			bool hit = false;
			for (; i < max && !shouldQuit() && !_ending.finished(); i++) {
				if (!_page->active())
					break;
				Common::StringTokenizer states(a1, ",");
				while (!states.empty())
					hit |= states.nextToken() == _page->stateName();
				if (hit)
					break;
				const uint32 frameStart = g_system->getMillis();
				if (skip) {
					in.click = i % 4 == 0;
					in.mouseDown = i % 4 != 3;
				}
				gameStep(world, in, dt);
				clearEdges(in);
				presentWorld(world);
				world.updateCursor();
				if (g_system->getMillis() - frameStart < framePeriod())
					g_system->delayMillis(framePeriod() - (g_system->getMillis() - frameStart));
			}
			in.mouseDown = false;
			debug("SCRIPT pwait %s: %s after %d frames (page %d state %s)", a1.c_str(), hit ? "reached" : (_page->active() ? "TIMED OUT" : "page closed"), i,
				_page->active() ? _page->pageIndex() : -1, _page->active() ? _page->stateName() : "-");
		} else if (cmd == "give") {
			world.carryItem(atoi(a1.c_str()));
		} else if (cmd == "pagestate") {
			debug("SCRIPT pagestate %s page=%d state=%s bar=%d drag=%d cursor=%d", a1.c_str(), _page->pageIndex(),
				_page->stateName(), _page->barVisible(), _page->dragItem(), world.cursorKind());
		} else if (cmd == "spr") {
			// spr <name> <cell> <out.png>: log the sprite's cell sizes and draw one cell centred / at its origin
			UISprite sp;
			if (!sp.loadSpr(a1)) {
				warning("Vangogh: script:%d: cannot load sprite '%s'", lineNo, a1.c_str());
			} else {
				const uint cellNo = atoi(a2.c_str());
				debug("SCRIPT spr '%s' cells=%u mode=%d cell%u=%dx%d", a1.c_str(), sp.numCells(), (int)sp.mode(), cellNo, sp.width(cellNo), sp.height(cellNo));
#ifdef USE_PNG
				Graphics::Surface out;
				out.create(640, 480, g_system->getScreenFormat());
				memset(out.getPixels(), 0x40, out.pitch * out.h);
				sp.blit(out, cellNo, 320, 240);
				writeSurface(out, a3);
				out.free();
#endif
			}
		} else if (cmd == "tgp") {
			Graphics::Surface img;
			if (!loadTgpImage(a1, img)) {
				warning("Vangogh: script:%d: cannot load tgp '%s'", lineNo, a1.c_str());
			} else {
				debug("SCRIPT tgp '%s' %dx%d", a1.c_str(), img.w, img.h);
#ifdef USE_PNG
				if (!a2.empty()) {
					Graphics::Surface out;
					out.create(MIN<int>(img.w, 640), MIN<int>(img.h, 480), img.format);
					blitRect(out, img, 0, 0, out.w, out.h, 0, 0);
					writeSurface(out, a2);
					out.free();
				}
#endif
				img.free();
			}
		} else if (cmd == "step") {
			const int n = atoi(a1.c_str());
			for (int i = 0; i < n && !shouldQuit() && !_ending.finished(); i++) {
				const uint32 frameStart = g_system->getMillis();
				gameStep(world, in, dt);
				clearEdges(in);
				presentWorld(world);
				world.updateCursor();
				if (a2 == "rt" && g_system->getMillis() - frameStart < framePeriod())
					g_system->delayMillis(framePeriod() - (g_system->getMillis() - frameStart));
			}
		} else if (cmd == "pdrag") {
			// pdrag <item> <x> <y>: open the page's bar, scroll to the item, drag it to x,y and drop it
			auto frames = [&](int n) {
				for (int i = 0; i < n && !shouldQuit(); i++) {
					gameStep(world, in, dt);
					clearEdges(in);
					presentWorld(world);
					world.updateCursor();
				}
			};
			const int item = atoi(a1.c_str());
			if (!_page->active() || _page->barIndexOf(item) < 0) {
				warning("Vangogh: script:%d: pdrag: item %d is not in the page bar", lineNo, item);
			} else {
				if (!_page->barVisible()) {
					in.space = true;
					frames(1);
				}
				for (int i = 0; i < 60 && !_page->barIsShown(); i++)
					frames(1);
				for (int guard = 0; guard < 20; guard++) {
					const int idx = _page->barIndexOf(item), sc = _page->barScroll();
					if (idx >= sc && idx < sc + 6)
						break;
					in.mouseX = idx < sc ? 24 : 520;
					in.mouseY = 450;
					frames(2);
					in.click = true;
					frames(2);
				}
				const int k = _page->barIndexOf(item) - _page->barScroll();
				in.mouseX = 101 + 70 * k;
				in.mouseY = 450;
				frames(2);
				in.click = in.mouseDown = true;
				frames(2);
				in.mouseX = atoi(a2.c_str());
				in.mouseY = atoi(a3.c_str());
				frames(2);
				in.mouseDown = false;
				frames(3);
				debug("SCRIPT pdrag item %d (bar slot %d) -> %s,%s", item, k, a2.c_str(), a3.c_str());
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
		} else if (cmd == "aim") {
			// aim <node> [dist]: camera in front of the node, mouse on the screen centre
			if (!world.aimAt(a1.c_str(), a2.empty() ? 600 : atoi(a2.c_str())))
				warning("Vangogh: script:%d: no node '%s' to aim at", lineNo, a1.c_str());
			in.mouseX = 320;
			in.mouseY = 240;
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
		} else if (cmd == "save") {
			Common::U32String why;
			if (!canSaveGameStateCurrently(&why))
				warning("Vangogh: script:%d: cannot save now", lineNo);
			else
				debug("SCRIPT save slot %d: %s", atoi(a1.c_str()), saveGameState(atoi(a1.c_str()), a2.empty() ? "Script" : a2, false).getCode() == Common::kNoError ? "ok" : "FAILED");
		} else if (cmd == "load") {
			debug("SCRIPT load slot %d: %s", atoi(a1.c_str()), loadGameState(atoi(a1.c_str())).getCode() == Common::kNoError ? "ok" : "FAILED");
		} else if (cmd == "autosave") {
			autosaveAfterPlacement(atoi(a1.c_str()));
		} else if (cmd == "import") {
			Common::FSNode node(Common::Path(a1, '/'));
			Common::SeekableReadStream *in2 = node.createReadStream();
			debug("SCRIPT import %s -> slot %d: %s", a1.c_str(), atoi(a2.c_str()), in2 && importOriginalSave(in2, atoi(a2.c_str())) ? "ok" : "FAILED");
			delete in2;
		} else if (cmd == "print") {
			const Pose3D p = world.pose();
			debug("SCRIPT %s scene=%s box=%d pose=%d,%d,%d rot=%d,%d hover='%s' cursor=%d", a1.c_str(),
				world.sceneName(world.scene()).c_str(), world.box(), p.x, p.y, p.z, p.rotA, p.rotB, world.hoverName(), world.cursorKind());
		} else if (cmd == "until") {
			// until <cinefin|cinefin2|credits|release|done> <max> [rt] [click]: step until the ending reaches that stage;
			// "click" delivers a click every 25 frames while a 2D page is open (to get through page 21)
			int want = Ending::kStageMovie1;
			if (a1 == "cinefin2") want = Ending::kStageMovie2;
			else if (a1 == "credits") want = Ending::kStageCredits;
			else if (a1 == "release") want = Ending::kStageRelease;
			else if (a1 == "done") want = 99;
			const bool rt = a3 == "rt" || a4 == "rt";
			const bool autoClick = a3 == "click" || a4 == "click";
			const int max = atoi(a2.c_str());
			int i = 0;
			for (; i < max && !shouldQuit() && !_ending.finished(); i++) {
				if (_ending.active() && want != 99 && _ending.stage() == want)
					break;
				const uint32 frameStart = g_system->getMillis();
				if (autoClick && _page->active() && i % 25 == 24)
					in.click = true;
				gameStep(world, in, dt);
				clearEdges(in);
				presentWorld(world);
				world.updateCursor();
				if (rt && g_system->getMillis() - frameStart < framePeriod())
					g_system->delayMillis(framePeriod() - (g_system->getMillis() - frameStart));
			}
			debug("SCRIPT until %s: %s after %d frames (page open %d)", a1.c_str(),
				(_ending.active() && _ending.stage() == want) || _ending.finished() ? "reached" : "NOT reached", i, _page->active());
		} else if (cmd == "ending") {
			_ending.start(a1 == "fin");
		} else if (cmd == "shot") {
#ifdef USE_PNG
			writeShot(frameSurface(world), world, in.mouseX, in.mouseY, a1);
#else
			warning("Vangogh: script: this build has no PNG support");
#endif
		} else {
			warning("Vangogh: %s:%d: unknown command '%s'", name.c_str(), lineNo, cmd.c_str());
		}
	}

}

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
	//   aim <node> [dist]  camera dist (default 600) in front of the node, looking at it, mouse on the screen centre
	//   box <n>            SetBox(n): switch the active collision volume (0 = BOX.3DI, 1.. = the scene's variants)
	//   newgame            reset the game state (then goto a scene to apply it)
	//   flag <name> [v]    log (or set) a game state flag: named, A<n>, B<n>, inv<n> or 0x<offset>
	//   flags              log every set flag
	//   anim <sel> <op> [n]  3DA animations: op = start, stop, end, pose n, tick n, check ("anim list" lists them)
	//   objects            log the scene's objects with their runtime types
	//   reach              log the museum paintings' distance to the walkable floor
	//   movie <name> <frame> <out.png>  play a movie-table clip (CVY mask included) to frame N and save its surface
	//   save <slot> [desc] save the game to a ScummVM slot (refused like the menu would: not during clips/glides)
	//   load <slot>        load a ScummVM slot (the launcher's save_slot key does the same at boot)
	//   autosave <item>    autosaveAfterPlacement(item): the exe's GAME save on placing an item
	//   import <file> <slot>  convert an original GAME*.BIN / GGAME*.BIN (native path) into a slot
	//   down / up          left button pressed (with a click edge) / released: drags drop on release
	//   page <n>           open 2D page n over the current state
	//   give <item>        carry item n in 3D as if it had just been picked up
	//   pagestate [tag]    log the page's state machine state
	//   spr <name> <cell> <out.png>  log a sprite's cell count/size and draw one cell
	//   tgp <name> [out.png]         log a TGP's size and save its top-left 640x480
	//   print              log scene, pose, hovered object and cursor kind
	//   shot <file.png>    save the current frame with the cursor drawn on it
	//   walk <node> [dist] [min]  walk (arrow keys, planned route) to between min (default 0) and dist (default 800) of the node's centre
	//   walkxz <x> <z> [tol]  the same towards a floor point (default tolerance 150)
	//   look <node>        turn (arrows, PgUp/PgDn) until the node's centre is in front of the camera
	//   hover <node>       move the pointer onto a screen pixel that picks the node
	//   use <node> [dist] [min]  walk, look, hover, click and let the result play for a few frames
	//   store              drop the carried item in the inventory bar (pointer to the bottom, click)
	//   skipclip [max]     click through a running arrival/departure clip
	//   until <stage> <max> [rt] [click]  step until the ending stage (cinefin, cinefin2, credits, release, done) is reached
	//   ending [fin]       start the ending sequence (cinefin, cinefin2, credits, quit); "fin" = the museum fin trigger.
	//                      The script stops where the game would quit; "step n rt" paces the movies in real time
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
	bootLoadSlot();

	WorldInput in;
	const float dt = (float)kFrameMs * kTicksPerMs;
	runScriptStream(world, in, dt, stream, path);
	delete stream;
	_world = nullptr;
}


// ---------------------------------------------------------------------------
// Saves. The exe has GAME<user><item>.BIN (one per placed item) and GGAME<user>.BIN (the continue file, written on
// quit); ScummVM slots hold GameState::sync (the same two blocks) behind a small versioned header. The autosave slot
// plays both roles. Layout: "VGSV", u32 version, u32 resume flag (1 = 2D page, 0 = 3D world), state (0x46c bytes).

namespace {
const uint32 kSaveMagic = MKTAG('V', 'G', 'S', 'V');
const uint32 kSaveVersion = 1;
const int kOrigGameSize = GameState::kSaveSize;		// GAME<uu><ss>.BIN
const int kOrigGGameSize = 4 + GameState::kSaveSize;	// GGAME<u>.BIN: resume flag first
}

Common::Error VangoghEngine::writeSaveState(Common::WriteStream *stream, GameState &state, uint32 resumeFlag) {
	stream->writeUint32LE(kSaveMagic);
	stream->writeUint32LE(kSaveVersion);
	stream->writeUint32LE(resumeFlag);
	Common::Serializer ser(nullptr, stream);
	state.sync(ser);
	return stream->err() ? Common::kWritingFailed : Common::kNoError;
}

Common::Error VangoghEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	if (!_world || _world->scene() < 0)
		return Common::kWritingFailed;
	_world->snapshotState();
	return writeSaveState(stream, _world->state(), _pageOpen ? 1 : 0);
}

bool VangoghEngine::restoreState(const GameState &state, uint32 resumeFlag) {
	if (!_world)
		return false;
	const int scene = state.getByte(kOfsScene);
	const int prev = state.getByte(kOfsPrevScene);
	if (scene >= kNumScenes || prev >= kNumScenes)
		return false;

	// The state first: scene init reads it (hidden nodes, collision volume, chambre variant, ...).
	_world->state() = state;
	GameState &st = _world->state();

	// load_game fix-up 5: the sunflower counter restarts at 0 when the next wing begins.
	const byte sun = st.getByte(kOfsSunflowers);
	if ((sun == 3 && !st.isSet(kOfsCurAct2)) || (sun == 0xf && !st.isSet(kOfsCurAct3)))
		st.setByte(kOfsSunflowers, 0);

	if (!_world->enterScene(scene, prev))
		return false;

	Pose3D p;
	p.x = (int16)st.get(kOfsCamX);
	p.y = (int16)st.get(kOfsCamY);
	p.z = (int16)st.get(kOfsCamZ);
	p.rotA = (int16)st.get(kOfsCamRotA);
	p.rotB = (int16)st.get(kOfsCamRotB);
	_world->setPose(p);

	_pageOpen = resumeFlag != 0;
	_pendingPageResume = _pageOpen;
	return true;
}

Common::Error VangoghEngine::loadGameStream(Common::SeekableReadStream *stream) {
	if (!_world)
		return Common::kReadingFailed;
	if (stream->readUint32LE() != kSaveMagic)
		return Common::kReadingFailed;
	if (stream->readUint32LE() != kSaveVersion)
		return Common::kReadingFailed;
	const uint32 resumeFlag = stream->readUint32LE();

	GameState state;
	Common::Serializer ser(stream, nullptr);
	state.sync(ser);
	if (stream->err() || stream->eos())
		return Common::kReadingFailed;

	return restoreState(state, resumeFlag) ? Common::kNoError : Common::kReadingFailed;
}

bool VangoghEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	return _world && _world->scene() >= 0 && !_ending.active() && !_world->clipActive() && _world->glideFrames() == 0;
}

bool VangoghEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return _world && !_ending.active() && !_world->clipActive() && _world->glideFrames() == 0;
}

void VangoghEngine::autosaveAfterPlacement(int itemId) {
	if (!_world || _world->scene() < 0)
		return;
	if (itemId >= 0)
		debug("Vangogh: autosave after placing item %d", itemId);
	else
		debug("Vangogh: autosave at a puzzle checkpoint");
	if (saveGameState(getAutosaveSlot(), "Autosave", true).getCode() != Common::kNoError)
		warning("Vangogh: autosave failed");
}

void VangoghEngine::saveResume() {
	if (!_world || _world->scene() < 0 || _world->clipActive() || _world->glideFrames() > 0)
		return;
	if (saveGameState(getAutosaveSlot(), "Autosave", true).getCode() != Common::kNoError)
		warning("Vangogh: could not save the resume state");
}

void VangoghEngine::bootLoadSlot() {
	if (!ConfMan.hasKey("save_slot"))
		return;
	const int slot = ConfMan.getInt("save_slot");
	if (slot < 0)
		return;
	_bootLoading = true;
	if (loadGameState(slot).getCode() != Common::kNoError)
		warning("Vangogh: could not load save slot %d", slot);
	_bootLoading = false;
}

bool VangoghEngine::importOriginalSave(Common::SeekableReadStream *stream, int slot) {
	const int64 size = stream->size();
	uint32 flag = 0;
	if (size == kOrigGGameSize)
		flag = stream->readUint32LE();
	else if (size != kOrigGameSize)
		return false;

	GameState state;
	Common::Serializer ser(stream, nullptr);
	state.sync(ser);
	if (stream->err() || state.getByte(kOfsScene) >= kNumScenes || flag > 1)
		return false;

	Common::OutSaveFile *out = _saveFileMan->openForSaving(getSaveStateName(slot));
	if (!out)
		return false;
	const bool ok = writeSaveState(out, state, flag).getCode() == Common::kNoError;
	if (ok) {
		getMetaEngine()->appendExtendedSave(out, getTotalPlayTime(), "Imported", false);
		out->finalize();
	}
	delete out;
	return ok;
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
