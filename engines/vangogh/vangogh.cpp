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
#include "vangogh/detection.h"
#include "vangogh/console.h"
#include "vangogh/hnmplayer.h"
#include "vangogh/scene.h"
#include "vangogh/spr.h"
#include "vangogh/tgp.h"
#include "common/archive.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/rect.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"
#include "engines/util.h"
#include "graphics/surface.h"
#include "image/bmp.h"
#include "image/tga.h"

namespace Vangogh {

VangoghEngine *g_engine;

VangoghEngine::VangoghEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _randomSource("Vangogh") {
	g_engine = this;
}

VangoghEngine::~VangoghEngine() {
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

	// Set the engine's debugger console
	setDebugger(new Console());

	// The original splash screens are pre-rendered at 640x480; verified
	// directly against data/local/schermo{1,2,3}.tga. A null format asks
	// for the backend's preferred true color mode, since the default
	// CLUT8 mode can't display these directly.
	initGraphics(640, 480, nullptr);
	_screen = new Graphics::Screen();

	showIntro();

	// Real intro cutscene (data/movies/intro.hnm, scene-flow.md sec.1.5:
	// "playMovie(hwndMain, 'intro')") -- playVideo() already no-ops
	// gracefully (with a warning) if the file is missing.
	if (!shouldQuit())
		playVideo("intro");

	// Approximated main menu (accueil.bmp + clickable BUTTON regions --
	// see showAccueil()). A Start click (or Enter, or boot_param) begins
	// a new game; Quit (or Escape, or closing the window) does not.
	bool startGame = true;
	if (!shouldQuit())
		startGame = showAccueil();

	// New-game entry point is always musee (scene 0, scene-flow.md
	// sec.1.5/sec.0.6); runGame() drives every subsequent hotspot-click
	// transition across the recovered navigation graph (navgraph.h).
	if (!shouldQuit() && startGame)
		runGame();

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

NavAction VangoghEngine::enterScene(const Common::String &name, bool simulateClick) {
	Scene scene(name);
	if (!scene.load()) {
		warning("Vangogh: enterScene('%s'): nothing to show (no backdrop, no hotspot boxes, no nav edges)", name.c_str());
		_lastSceneName = name;
		_lastHotspots.clear();
		return NavAction();
	}

	scene.run(simulateClick);

	_lastSceneName = name;
	_lastHotspots = scene.projectedHotspots();
	return scene.resolvedAction();
}

void VangoghEngine::runGame() {
	Common::String current = sceneNameForId(kSceneMusee);

	// Headless/automated runs (--boot-param=1) have no real input device;
	// simulate exactly ONE hotspot click (in whichever scene visit needs
	// it first) to demonstrate a real transition end-to-end, then let the
	// chain end naturally (the next scene's own auto-advance timeout,
	// Scene::run()'s existing boot_param handling) instead of cascading
	// through the whole 23-edge graph unattended.
	int simulateHopsRemaining = ConfMan.hasKey("boot_param") ? 1 : 0;

	while (!shouldQuit()) {
		const NavAction action = enterScene(current, simulateHopsRemaining > 0);

		if (action.kind == NavAction::kQuit) {
			quitGame();
			break;
		}
		if (action.kind == NavAction::kNone || action.kind == NavAction::kLocal)
			break; // nothing left to do: window closed, Escape pressed, or (shouldn't happen -- Scene::run() only
			       // returns on a leaving action or no action) a stray local-only result.

		if (simulateHopsRemaining > 0)
			simulateHopsRemaining--;

		current = (action.kind == NavAction::kHub) ? sceneNameForId(kSceneMusee) : action.targetScene;
	}

	debug("Vangogh: navigation loop ended (last scene '%s')", current.c_str());
}

bool VangoghEngine::showAccueil() {
	const Common::Path bmpPath("local/accueil.bmp");

	Common::File bmpFile;
	if (!bmpFile.open(bmpPath)) {
		warning("Vangogh: could not open menu background %s -- starting the game directly", bmpPath.toString().c_str());
		return true;
	}

	Image::BitmapDecoder decoder;
	if (!decoder.loadStream(bmpFile)) {
		warning("Vangogh: failed to decode menu background %s -- starting the game directly", bmpPath.toString().c_str());
		return true;
	}

	// Reversed Win32 BUTTON rects, assuming a 640x480 surface with
	// baseX=baseY=0 (scene-flow.md sec.1.3). Only "Go"/start and "Quit"
	// are wired to an action; the player-name EDIT control and the
	// save-slot rows are non-goals here (no save/load) -- slots are kept
	// as logged no-ops rather than silently invisible/dead.
	const Common::Rect kStartButton(407, 396, 443, 419);
	const Common::Rect kQuitButton(468, 396, 540, 423);
	const Common::Rect kPlayerSlots[5] = {
		Common::Rect(90, 217, 252, 244),
		Common::Rect(90, 251, 252, 278),
		Common::Rect(90, 285, 252, 312),
		Common::Rect(90, 319, 252, 346),
		Common::Rect(90, 353, 252, 380),
	};

	debug("Vangogh: showing accueil.bmp menu (click/Enter Start to begin, Quit/Escape to exit)");

	const Graphics::Surface *surface = decoder.getSurface();
	const Common::Point dest((_screen->w - surface->w) / 2, (_screen->h - surface->h) / 2);
	_screen->blitFrom(*surface, dest);
	// Subtle visual affordance for the two wired buttons -- accueil.bmp's
	// own BOUTONS.BMP-drawn button faces aren't reproduced here (out of
	// this milestone's scope), just enough of an outline that the
	// approximation isn't a fully invisible click target.
	_screen->frameRect(kStartButton, _screen->format.RGBToColor(255, 255, 255));
	_screen->frameRect(kQuitButton, _screen->format.RGBToColor(255, 255, 255));
	_screen->update();

	// Automated/headless runs (e.g. `--boot-param=1`, used for CI smoke
	// tests under SDL_VIDEODRIVER=dummy where no real input device
	// exists) skip the wait and advance immediately, exactly as if
	// "Start" had been clicked.
	if (ConfMan.hasKey("boot_param")) {
		debug("Vangogh: boot_param set, advancing past accueil immediately");
		return true;
	}

	while (!shouldQuit()) {
		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			switch (event.type) {
			case Common::EVENT_QUIT:
			case Common::EVENT_RETURN_TO_LAUNCHER:
				return false;
			case Common::EVENT_LBUTTONDOWN:
				if (kStartButton.contains(event.mouse)) {
					debug("Vangogh: accueil: Start clicked");
					return true;
				}
				if (kQuitButton.contains(event.mouse)) {
					debug("Vangogh: accueil: Quit clicked");
					return false;
				}
				for (uint i = 0; i < ARRAYSIZE(kPlayerSlots); i++) {
					if (kPlayerSlots[i].contains(event.mouse)) {
						debug("Vangogh: accueil: player slot %u clicked (save/load not implemented, ignoring)", i);
						break;
					}
				}
				break;
			case Common::EVENT_KEYDOWN:
				// Mirrors the real accueil window's Enter->"Go"/
				// Escape->"Quit" shortcuts (scene-flow.md sec.1.4).
				if (event.kbd.keycode == Common::KEYCODE_RETURN) {
					debug("Vangogh: accueil: Enter pressed (Start)");
					return true;
				}
				if (event.kbd.keycode == Common::KEYCODE_ESCAPE) {
					debug("Vangogh: accueil: Escape pressed (Quit)");
					return false;
				}
				break;
			default:
				break;
			}
		}
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
	return false; // shouldQuit() became true some other way (e.g. return-to-launcher).
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
