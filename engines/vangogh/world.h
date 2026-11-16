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

#ifndef VANGOGH_WORLD_H
#define VANGOGH_WORLD_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

#include "vangogh/anim3da.h"
#include "vangogh/box3di.h"
#include "vangogh/clip.h"
#include "vangogh/gamestate.h"
#include "vangogh/model3d.h"
#include "vangogh/renderer3d.h"

namespace Graphics {
struct Surface;
}

namespace Vangogh {

/** Scene enum of PEINTRE.exe (dat_54d8c0 / dat_5e3e30), world.md Appendix C. */
enum SceneId {
	kSceneMusee = 0,
	kSceneAuberge,
	kSceneHopiext,
	kSceneMaisonet,
	kSceneMangeurs,
	kSceneCafe,
	kSceneChambre,
	kSceneMaisonj,
	kSceneHopiint,
	kScenePont,
	kSceneTerrasse,
	kSceneJardin,
	kSceneChamp,
	kSceneEglise,
	kNumScenes
};

const int kSphereRadius = 185;		///< sub_425d30: camera collision sphere
const float kPickRange = 1200.0f;	///< dat_4eee84: museum click/hover distance

/** Cursor kinds used outside the cursor table code (the exe's cursor table indices). */
enum { kCursorArrow = 0, kCursorFinger = 11 };

/** Per-frame input, already mapped from ScummVM events by the engine (the exe reads DirectInput scancodes). */
struct WorldInput {
	bool up, down, left, right;	///< arrows (held)
	bool pgUp, pgDown;			///< pitch (held)
	bool alt;					///< strafe modifier (held)
	bool f1;					///< toggle mouse-look (pressed this frame)
	bool backspace;				///< leave scene (pressed this frame)
	bool space;					///< inventory (pressed this frame)
	bool esc;					///< options (pressed this frame)
	bool click;					///< left button or Enter (pressed this frame)
	int mouseX, mouseY;			///< absolute, 0..639 / 0..479

	WorldInput() : up(false), down(false), left(false), right(false), pgUp(false), pgDown(false), alt(false),
		f1(false), backspace(false), space(false), esc(false), click(false), mouseX(320), mouseY(240) {}
};

/**
 * The live 3D game world (world.md sec.8/9, flow.md): owns the current scene's
 * model, collision volume, camera and movement state, and runs one 66 ms
 * frame at a time: movement (sub_42a4e0) -> ground follow (sub_42ac60) ->
 * collision (sub_425d30) -> render -> pick (sub_442460) -> object clicks and
 * exits -> scene change. The engine owns the clock and presents surface().
 */
class World {
public:
	World();

	/** Decodes the 14 mouse cursors out of curseurs.spr; false if the file is unusable. */
	bool init();

	/** Scene name for @p id; chambre is chambrev (variant 0) or chambreb (variant 1). */
	Common::String sceneName(int id) const;
	/** Scene id for a name ("musee", "chambre", "chambreb", ...), or -1. */
	static int sceneIdByName(const Common::String &name);

	/**
	 * Loads scene @p id and places the camera for arriving from @p prev: the
	 * recorded transition pose for (prev,id), the museum pose keyed by
	 * @p prev when id is the museum, else the scene default. The movement
	 * state is reset; the collision volume, hidden nodes, pickable types,
	 * animation poses and sounds are rebuilt from the game state (the scene
	 * init functions of the exe, scenelogic.cpp).
	 */
	bool enterScene(int id, int prev);

	/** Runs one frame of @p dt original ticks (dt = ms * 30 / 1000) and leaves the rendered frame in surface(). */
	void frame(const WorldInput &in, float dt);

	const Graphics::Surface &surface() const { return _clipActive ? _clip.surface() : _renderer.surface(); }
	int scene() const { return _scene; }
	/** Index of the active collision volume (0 = BOX.3DI). */
	int box() const { return _box; }

	/** Current camera, with the angles as stored by the movement code (signed pitch, yaw 0..4095). */
	Pose3D pose() const;
	/** Teleports the camera (debug); angles are masked to 12 bits. */
	void setPose(const Pose3D &p);

	/** Name of the pickable object under the mouse as of the last frame ("" if none). */
	const char *hoverName() const { return _hoverName; }
	/** Cursor kind (0..13, the exe's cursor table index) to show for the last frame. */
	int cursorKind() const { return _cursorKind; }
	/** Makes the OS cursor match cursorKind() (touches CursorMan only when the kind changed). */
	void updateCursor();
	/** Cursor frame as a CLUT8 cell, for compositing screenshots. Hot spot is at (w/2+hotX, h/2+hotY). */
	bool cursorImage(int kind, const byte *&pixels, int &w, int &h, int &hotX, int &hotY, const byte *&palette) const;

	/** Number of frames left in a museum painting glide (0 = none). */
	int glideFrames() const { return _glideLeft; }
	/** True once after Esc was pressed; the engine opens the options menu. */
	bool takeMenuRequest();

	/**
	 * SetBox(n): switches the active collision volume to the scene's n-th BOX
	 * variant (0 = the default BOX.3DI). The exe does this from puzzle logic.
	 */
	bool setBox(int n);

	/** Pickable type of node name @p name in the current scene (255 = inert, 0 if unknown). */
	int objectType(const char *name) const;

	// ---- Game state (gamestate.h) ----

	GameState &state() { return _state; }
	const GameState &state() const { return _state; }
	/** Starts a fresh game: the .data state, chambre variant 0, and re-enters nothing (call enterScene). */
	void newGame();
	/** Copies the camera and the scene ids into the state block (the exe does this before saving or entering a 2D puzzle). */
	void snapshotState();
	/** The 2D puzzle commit of puzzle @p id: sets A[id] and the page, as the exe does on return from a puzzle. */
	void solvePuzzle(int id);

	// ---- Objects (scenelogic.cpp) ----

	uint numObjects() const { return _numObjects; }
	/** Object @p i of the scene: name, runtime pickable type and whether its faces are hidden. */
	bool objectInfo(uint i, const char *&name, int &type, bool &hidden) const;
	/** Sets the runtime pickable type of @p name (the exe rewrites these bytes from init and tick code). */
	bool setObjectType(const char *name, int type);
	/** HideObject/ShowObject (sub_43bc60/sub_43bc90) on the node called @p name. */
	bool setObjectHidden(const char *name, bool hidden);

	// ---- 3DA animations (scenelogic.cpp) ----

	struct AnimInfo {
		const char *clip;	///< .3DA record
		const char *node;	///< the object the exe names for it
		bool active;
		int elapsed, duration;
		uint channels, animated;
	};
	uint numAnims() const { return _numAnims; }
	bool animInfo(uint i, AnimInfo &out) const;
	/** Index of the animation whose clip is @p name ("portail", "portail.3da" or its index), or -1. */
	int findAnim(const Common::String &name) const;
	/** Activates animation @p i from tick 1, as the interaction scripts do (`active = 1`). */
	bool startAnim(uint i);
	bool stopAnim(uint i);
	/** Poses the model at tick @p t of animation @p i without touching its running state. */
	bool poseAnim(uint i, int t);
	/** Advances every active animation by @p dt original ticks (1..10), as the scene tick does. */
	void stepAnims(int dt);
	/**
	 * Debug interface shared by the console and the test scripts: "list" (or an empty @p sel) describes every
	 * animation; otherwise @p op is start, stop, end (last pose), pose (at tick @p arg), tick (advance @p arg ticks)
	 * or check (3DA/3DC channel mapping cross-check) on the animation selected by @p sel. Appends the result to @p out.
	 */
	bool animCommand(const Common::String &sel, const Common::String &op, int arg, Common::String &out);
	/** Lists the scene's objects: name, runtime type, hidden. */
	void describeObjects(Common::String &out) const;
	/** Gating report: floods the floor the camera can walk on in the active collision volume (cell count) and, in the museum, tells which paintings are within click range. */
	void describeReach(Common::String &out);

	// ---- Arrival and departure clips (scenelogic.cpp) ----

	bool clipActive() const { return _clipActive; }
	const MovieClip &clip() const { return _clip; }
	/** Plays data/movies/<name>.hnm full screen; when it ends (or is skipped by a click) scene @p thenScene is entered from @p thenPrev. */
	bool playClip(const char *name, int thenScene, int thenPrev);
	/** Leaves the scene for the museum (Backspace): plays the "r" clip first if the scene's puzzles are all solved. */
	void leaveToMuseum();

private:
	enum { kMaxObjects = 32, kNumCursors = 14 };

	struct Cursor {
		Common::Array<byte> pixels;
		int w, h, hotX, hotY;
	};

	void resetMovement();
	void movement(const WorldInput &in, float dt);
	void collide();
	void pickAndAct(const WorldInput &in);
	bool checkCameraExits();
	void onObjectClick(const char *name, int type, float distance);
	void startPaintingClick(int dest);
	void startGlide(int dest);
	void stepGlide();
	void changeScene(int dest, int prev);
	void cameraPose(Pose3D &p) const;
	void locateObjects();

	// scenelogic.cpp
	void initSceneState(Pose3D *pose);
	void loadAnims(const BFGFile &bfg);
	void loadSounds();
	void sceneTick(int dt);
	void animDone(uint i);
	bool arrivalClip(int scene, char *name, size_t size) const;
	void stepClip(const WorldInput &in);
	void endClip();
	void selectBoxMaisonj();
	void hideObj(const char *name) { setObjectHidden(name, true); }
	void showObj(const char *name) { setObjectHidden(name, false); }
	void setTypes(int type, const char *const *names);
	void playSound(const char *name, bool loop = false);
	/** Camera-relative horizontal distance to node @p name as of the last render (sub_43c4f0 + sqrt(x^2+z^2)). */
	float objectDistance(const char *name) const;
	void animToEnd(uint i);
	int animEndFrame(uint i) const;

	Renderer3D _renderer;
	Model3D _model;
	CollisionMesh _boxes[5];	///< SetBox variants of the current scene; [0] = BOX.3DI
	bool _boxLoaded[5];
	int _box;					///< index of the active collision volume
	CollisionWorld _collision;

	GameState _state;
	int _scene;
	int _prevScene;
	int32 _groundProbe;		///< dat_4f9548

	// Movement state, int16 exactly as the exe's (dat_6997xx).
	int16 _fwd, _strafe, _pitchSpd, _turn;
	int16 _rotA, _rotB;
	int16 _pos[3];
	int32 _prevPos[3];		///< collision sweep origin (sub_425d30 `prev`)
	bool _mouseLook;

	// Pickables of the current scene (copied at entry; the types are mutable runtime state).
	struct Object {
		const char *name;
		int node;
		byte type;
	};
	Object _objects[kMaxObjects];
	uint _numObjects;
	int _hoverObject;
	char _hoverName[14];

	int _cursorKind;
	int _shownKind;
	Cursor _cursors[kNumCursors];
	byte _cursorPalette[256 * 3];
	bool _cursorsReady;

	int _glideDest;
	int _glideLeft;
	Pose3D _glideFrom, _glideTo;

	bool _menuRequest;

	// 3DA animation slots of the current scene (the exe's LoadAnims<scene> table).
	enum { kMaxAnims = 8 };
	struct AnimSlot {
		Anim3DA clip;
		const char *file;
		const char *node;
		int elapsed;		///< ENTRY.cur, starts at 1
		bool active;
		bool loaded;
		byte step;			///< ticks added per frame: full dt, dt >> 1, or 1
		byte end;			///< what happens when it runs out: stop, loop, or never run
	};
	AnimSlot _anims[kMaxAnims];
	uint _numAnims;

	// Runtime (not saved) scene state: the exe's 698fxx globals.
	int _framesInScene;	///< frames rendered since entry, proximity checks need a valid view position
	int _timer[3];		///< scene tick timers: butterflies (jardin), robot dialogue state and voice index (musee)
	int _timerLimit;	///< jardin butterfly switch threshold
	uint16 _voice;		///< sound handle of the running voice line, 0 = none
	bool _trainSound;

	// Arrival/departure clip.
	MovieClip _clip;
	bool _clipActive;
	int _clipScene, _clipPrev;
};

} // End of namespace Vangogh

#endif // VANGOGH_WORLD_H
