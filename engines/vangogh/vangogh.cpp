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
#include "common/system.h"
#include "common/textconsole.h"
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

	// "jardin.hnm" is the ambient garden-view intro movie played right
	// after the splash screens, before the (not yet implemented) main
	// menu. playVideo() already no-ops gracefully (with a warning) if the
	// file is missing, so no separate existence check is needed here.
	if (!shouldQuit())
		playVideo("jardin");

	// Placeholder for the real main menu: show the original "accueil"
	// (welcome) screen and wait for input.
	if (!shouldQuit())
		showAccueil();

	// Vertical-slice demo: after the (placeholder) main menu, drop
	// directly into the first real playable scene rather than returning
	// to an empty screen. "jardin" plays two independent roles here: the
	// ambient pre-menu movie above, and (separately) the scenes_3d/*.bfg-
	// backed Scene below -- nothing is shared between the two calls.
	if (!shouldQuit())
		enterScene("jardin");

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

void VangoghEngine::enterScene(const Common::String &name) {
	Scene scene(name);
	if (scene.load())
		scene.run();
}

void VangoghEngine::showAccueil() {
	const Common::Path bmpPath("local/accueil.bmp");

	Common::File bmpFile;
	if (!bmpFile.open(bmpPath)) {
		warning("Vangogh: could not open menu placeholder %s", bmpPath.toString().c_str());
		return;
	}

	Image::BitmapDecoder decoder;
	if (!decoder.loadStream(bmpFile)) {
		warning("Vangogh: failed to decode menu placeholder %s", bmpPath.toString().c_str());
		return;
	}

	debug("Vangogh: showing accueil.bmp menu placeholder (waiting for input)");

	const Graphics::Surface *surface = decoder.getSurface();
	const Common::Point dest((_screen->w - surface->w) / 2, (_screen->h - surface->h) / 2);
	_screen->blitFrom(*surface, dest);
	_screen->update();

	// Automated/headless runs (e.g. `--boot-param=1`, used for CI smoke
	// tests under SDL_VIDEODRIVER=dummy where no real input device
	// exists) skip the wait and advance immediately, exactly as if the
	// player had clicked -- see VangoghEngine::run()'s boot-flow comment
	// for the vertical-slice demo this unlocks.
	if (ConfMan.hasKey("boot_param")) {
		debug("Vangogh: boot_param set, advancing past accueil immediately");
		return;
	}

	// Real menu interactivity is out of scope here: just wait for a
	// keypress/click (or quit) instead of waitMillis()'s fixed duration.
	bool pressed = false;
	while (!shouldQuit() && !pressed) {
		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			switch (event.type) {
			case Common::EVENT_QUIT:
			case Common::EVENT_RETURN_TO_LAUNCHER:
			case Common::EVENT_KEYDOWN:
			case Common::EVENT_LBUTTONDOWN:
			case Common::EVENT_RBUTTONDOWN:
				pressed = true;
				break;
			default:
				break;
			}
		}
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
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
