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

#ifndef VANGOGH_H
#define VANGOGH_H

#include "common/scummsys.h"
#include "common/system.h"
#include "common/error.h"
#include "common/fs.h"
#include "common/hash-str.h"
#include "common/random.h"
#include "common/util.h"
#include "engines/engine.h"
#include "engines/savestate.h"
#include "graphics/screen.h"

#include "vangogh/detection.h"

namespace Vangogh {

class VangoghEngine : public Engine {
private:
	const ADGameDescription *_gameDescription;
	Common::RandomSource _randomSource;

	/**
	 * Plays the intro sequence described by data/local/intro.ini: each
	 * line is "<name> <milliseconds>", naming a TGA image (without
	 * extension) to show for that long.
	 */
	void showIntro();

	/**
	 * Loads and centers a single intro image on screen, then waits for
	 * the given duration (or until the engine is asked to quit).
	 */
	void showSplashImage(const Common::String &name, uint32 durationMs);

	/**
	 * Loads data/local/accueil.bmp, centers it on screen, and waits for a
	 * keypress/click (or quit) before returning. Menu placeholder shown
	 * after the intro sequence and the ambient "jardin" movie, pending a
	 * real main menu implementation.
	 */
	void showAccueil();

	/**
	 * Waits for up to the given number of milliseconds, remaining
	 * responsive to quit/return-to-launcher requests.
	 */
	void waitMillis(uint32 ms);
protected:
	// Engine APIs
	Common::Error run() override;
public:
	Graphics::Screen *_screen = nullptr;
public:
	VangoghEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~VangoghEngine() override;

	uint32 getFeatures() const;

	/**
	 * Returns the game Id
	 */
	Common::String getGameId() const;

	/**
	 * Plays an HNM6 movie from data/movies/<name>.hnm to completion (or
	 * until skipped via any keypress/click, or the engine is asked to
	 * quit). Uses HNMPlayer (see hnmplayer.h), a thin non-blocking wrapper
	 * around Video::HNMDecoder shared with Scene's looping backdrop
	 * playback; embedded APC audio, if any, is handled automatically by
	 * HNMDecoder itself. See hnm-apc-compat.md in the reversing notes for
	 * why this needs no engine-specific decoding work.
	 */
	void playVideo(const Common::String &name);

	/**
	 * Decodes an 'a'-type TGP image from data/gfx/<name>.TGP, centers it on
	 * screen and shows it for the given duration (or until quit).
	 */
	void showTGPImage(const Common::String &name, uint32 durationMs);

	/**
	 * Decodes cell @p cellIndex of an SPR sprite from
	 * data/sprites/<name>.spr, centers it on screen and shows it for the
	 * given duration (or until quit). Codec-C compressed cells are blitted
	 * with a transparent color key so skipped pixels show whatever is
	 * already on screen; raw cells have no transparency and are blitted
	 * fully opaque. See spr.h for the on-disk format.
	 */
	void showSPRCell(const Common::String &name, uint32 cellIndex, uint32 durationMs);

	/**
	 * Enters the scene loop for data/scenes_3d/<name>.bfg + its HNM
	 * backdrop -- see Scene::load()/Scene::run(). Used by both the
	 * boot-flow vertical-slice demo (run()) and the `scene <name>`
	 * console command.
	 */
	void enterScene(const Common::String &name);

	/**
	 * Gets a random number
	 */
	uint32 getRandomNumber(uint maxNum) {
		return _randomSource.getRandomNumber(maxNum);
	}

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsReturnToLauncher;
	}
};

extern VangoghEngine *g_engine;
#define SHOULD_QUIT ::Vangogh::g_engine->shouldQuit()

} // End of namespace Vangogh

#endif // VANGOGH_H
