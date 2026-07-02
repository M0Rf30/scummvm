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
#include "common/archive.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "engines/util.h"
#include "graphics/surface.h"
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
