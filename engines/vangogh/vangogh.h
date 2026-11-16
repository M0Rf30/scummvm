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
#include "vangogh/ending.h"
#include "vangogh/page.h"
#include "vangogh/world.h"
#include "vangogh/sound.h"

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
	 * The live game: the museum 3D world, one 66 ms World::frame() at a time,
	 * until quit or return-to-launcher. The ScummVM launcher takes the place
	 * of the original profile/start dialog.
	 */
	void runWorld();

	/**
	 * Headless test harness (ConfMan "vangogh_script"): runs the script file
	 * at the given native path against a World, feeding synthetic input at a
	 * fixed 66 ms tick and dumping PNGs. See runScript() in vangogh.cpp for
	 * the command list.
	 */
	void runScript(const Common::String &path);

	/** Copies the current frame (page or world) to the screen. */
	void presentWorld(const World &world);

	/**
	 * One tick of the live game, shared by runWorld() and runScript(): the 2D page when one is open (40 ms), else the
	 * 3D world (66 ms). Handles opening a page from a launcher click, leaving it (transition_commit target) and the
	 * resume of a loaded page save. @p dt is the world's tick count.
	 */
	void gameStep(World &world, WorldInput &in, float dt);
	/** Opens page @p page over the current world state. */
	bool openPage(World &world, int page);
	/** Frame period of the current mode in milliseconds. */
	uint32 framePeriod() const;
	/** What is on screen: the ending, the open page, or the world. */
	const Graphics::Surface &frameSurface(const World &world) const;

	World *_world;
	Page *_page;
	Ending _ending;		///< the end of the game (movies, credits, quit); active() once started

	// Saves (see vangogh.cpp). Payload: "VGSV", version, resume flag, then GameState::sync.
	bool _pageOpen;				///< a 2D page is open (set by the page code): saves record resume flag 1
	bool _bootLoading;			///< launcher save_slot load in progress: no autosave over the slot being loaded
	bool _pendingPageResume;	///< the loaded save asks to resume inside the 2D page named by the state block

	Common::Error writeSaveState(Common::WriteStream *stream, GameState &state, uint32 resumeFlag);
	/** Resume on quit: the exe's GGAME, kept in the autosave slot. */
	void saveResume();
	/**
	 * Re-enters the world from @p state per the exe's load rules (flow.md 3.6): scene, previous scene and camera
	 * from the block, NbTournesols sanity reset; @p resumeFlag 1 queues a 2D page resume.
	 */
	bool restoreState(const GameState &state, uint32 resumeFlag);
	/** Loads the launcher's save_slot, if any (once the world exists). */
	void bootLoadSlot();

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
	Sound *_sound;
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
	 * around Video::HNMDecoder; embedded APC audio, if any, is handled automatically by
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

	/** The running World, or null outside runWorld()/runScript() (used by the console). */
	World *world() const { return _world; }
	/** The ending sequence (the console and the script harness start and inspect it). */
	Ending &ending() { return _ending; }

	/**
	 * Gets a random number
	 */
	uint32 getRandomNumber(uint maxNum) {
		return _randomSource.getRandomNumber(maxNum);
	}

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime;
	}

	// ---- Saves ----

	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;
	/** Only in the 3D world or an idle page: not during arrival/departure clips or museum glides. */
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canSaveAutosaveCurrently() override { return !_bootLoading && canSaveGameStateCurrently(); }
	int getAutosaveSlot() const override { return 0; }

	/**
	 * The exe writes GAME<user><item> when item @p itemId is placed on its page (2D state 0x21); here it goes to the
	 * autosave slot. Called by the page code after the placement is committed to the game state.
	 */
	void autosaveAfterPlacement(int itemId);

	/** The page code tells whether a 2D page is open (saved as the resume flag). */
	void setPageOpen(bool open) { _pageOpen = open; }
	/** True once after a load that resumes inside a 2D page (the state block's page byte names it). */
	bool takePageResume() { const bool r = _pendingPageResume; _pendingPageResume = false; return r; }

	/**
	 * Converts an original GAME<uu><ss>.BIN (0x46c bytes) or GGAME<u>.BIN (0x470 bytes) read from @p stream into
	 * ScummVM slot @p slot.
	 */
	bool importOriginalSave(Common::SeekableReadStream *stream, int slot);
};

extern VangoghEngine *g_engine;
#define SHOULD_QUIT ::Vangogh::g_engine->shouldQuit()

} // End of namespace Vangogh

#endif // VANGOGH_H
