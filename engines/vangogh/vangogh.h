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
#include "vangogh/navgraph.h"
#include "vangogh/scene.h"

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
	 * Loads data/local/accueil.bmp, centers it on screen, and runs the
	 * approximated Win32 menu (scene-flow.md sec.1.3's reversed BUTTON
	 * rects): a "Go"/start click (or Enter) begins a new game, a "Quit"
	 * click (or Escape) exits. The player-name EDIT control and the
	 * save-slot rows are visual-only/logged no-ops (no save/load in this
	 * milestone). Returns true if the game should start, false if the
	 * player chose to quit (or the window was closed) here. Automated/
	 * headless runs (ConfMan "boot_param") skip the wait and return true
	 * immediately, exactly as if Start had been clicked.
	 */
	bool showAccueil();

	/**
	 * The post-menu navigation loop: starts at musee (scene 0 -- new
	 * games always start at the museum, scene-flow.md sec.1.5/sec.0.6)
	 * and repeatedly calls enterScene(), following whatever transition/
	 * hub/quit action each visit resolves (navgraph.h's NavAction), until
	 * quit, the window closes, or a visit resolves no action at all.
	 * Under automated/headless runs (ConfMan "boot_param"), only the
	 * FIRST visit simulates a hotspot click (see Scene::run()); every
	 * scene reached that way still fully loads/projects/plays out, but
	 * the chain stops growing after that one real transition.
	 */
	void runGame();

	/**
	 * Waits for up to the given number of milliseconds, remaining
	 * responsive to quit/return-to-launcher requests.
	 */
	void waitMillis(uint32 ms);

	Common::String _lastSceneName;
	Common::Array<Scene::ProjectedHotspot> _lastHotspots;
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
	 * backdrops -- see Scene::load()/Scene::run(). Used by both
	 * runGame()'s navigation loop and the `scene <name>` console command.
	 * @p simulateClick forwards to Scene::run() (see its doc comment).
	 * Caches the visited scene's name and projected hotspots (see
	 * lastSceneName()/lastSceneHotspots(), used by the `hotspots` console
	 * command) and returns whatever action the scene resolved.
	 */
	NavAction enterScene(const Common::String &name, bool simulateClick = false);

	/** Name of the most recently entered scene (via enterScene()), or empty if none yet. */
	const Common::String &lastSceneName() const { return _lastSceneName; }

	/** The most recently entered scene's projectedHotspots() snapshot -- see the `hotspots` console command. */
	const Common::Array<Scene::ProjectedHotspot> &lastSceneHotspots() const { return _lastHotspots; }

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
