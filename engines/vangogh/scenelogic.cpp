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


// Scene state and scene tick: the part of the exe's per-world init and tick
// functions (world.md sec.10, per-scene "Init" and "Tick") that is driven by
// the saved game state and by 3DA animations: which objects are hidden or
// pickable, which collision volume is active, which animations are posed or
// running, which sounds play, and what a finished animation changes.
// Puzzle clicks themselves are not here; they call startAnim()/the state setters.

#include "vangogh/world.h"
#include "vangogh/bfg.h"
#include "vangogh/page.h"
#include "vangogh/sound.h"
#include "vangogh/vangogh.h"

#include "common/debug.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include <math.h>

namespace Vangogh {

namespace {

// ---------------------------------------------------------------------------
// LoadAnims<scene> tables (anim.md sec.2, extracted from the exe's .data).

enum { kStepDt, kStepHalf, kStepOne };
enum { kEndStop, kEndLoop, kEndDead };

struct AnimDef {
	const char *file;
	const char *node;
	byte step;
	byte end;
	bool active;	///< starts active at scene entry regardless of state
};

const AnimDef kAnimMusee[] = {
	{ "etoile.3da", "etoile", kStepDt, kEndStop, false }, { "robot01.3da", "robot", kStepDt, kEndStop, false },
	{ "robot02.3da", "robot", kStepDt, kEndLoop, false }, { "robot03.3da", "robot", kStepDt, kEndStop, false }
};
const AnimDef kAnimAuberge[] = {
	{ "saccoch.3da", "saccoche", kStepDt, kEndStop, false }, { "portehau.3da", "portehaut", kStepDt, kEndStop, false },
	{ "portebas.3da", "portebas", kStepDt, kEndStop, false }
};
const AnimDef kAnimHopiext[] = {
	{ "grille.3da", "ciel", kStepDt, kEndStop, false }, { "cloche.3da", "cloche", kStepDt, kEndStop, false }
};
const AnimDef kAnimMaisonet[] = {
	{ "poule2.3da", "poule", kStepDt, kEndStop, false }, { "pelle.3da", "pelle", kStepDt, kEndStop, false },
	{ "oisaller.3da", "oiso", kStepHalf, kEndStop, false }, { "oisrturn.3da", "oiso", kStepHalf, kEndStop, false }
};
const AnimDef kAnimMangeurs[] = {
	{ "buche.3da", "buche", kStepDt, kEndStop, false }, { "chaise.3da", "chaise1", kStepDt, kEndStop, false },
	{ "fagot.3da", "fagot", kStepDt, kEndStop, false }, { "fenetre.3da", "fenetre", kStepDt, kEndStop, false },
	{ "portpoel.3da", "portepoel", kStepOne, kEndStop, false }
};
const AnimDef kAnimCafe[] = {
	{ "billard.3da", "ke", kStepDt, kEndStop, false }, { "portebar.3da", "barporte", kStepOne, kEndStop, false }
};
const AnimDef kAnimChambreV[] = {
	{ "portev.3da", "porte", kStepOne, kEndStop, false }
};
const AnimDef kAnimChambreB[] = {
	{ "chaise.3da", "CHAISE1", kStepDt, kEndStop, false }, { "chaussur.3da", "perpompe", kStepDt, kEndLoop, false },
	{ "mirroir.3da", "mirroircas", kStepDt, kEndStop, false }, { "papier.3da", "papier", kStepDt, kEndStop, false },
	{ "porte.3da", "porte", kStepDt, kEndStop, false }, { "tiroir.3da", "TIROIR", kStepDt, kEndStop, false }
};
const AnimDef kAnimMaisonj[] = {
	{ "escargot.3da", "escargot", kStepDt, kEndLoop, false }, { "nuages.3da", "nuages", kStepDt, kEndLoop, false },
	{ "portail.3da", "portail", kStepDt, kEndStop, false }, { "porte-mj.3da", "portemj", kStepDt, kEndStop, false },
	{ "train.3da", "TRAIN02", kStepDt, kEndStop, true }
};
const AnimDef kAnimHopiint[] = {
	{ "hopiint.3da", "ARMOIRE", kStepDt, kEndStop, false }, { "fou.3da", "FOU", kStepDt, kEndLoop, false }
};
const AnimDef kAnimPont[] = {
	{ "pond.3da", "ciel", kStepHalf, kEndStop, false }, { "trappe.3da", "trappe", kStepHalf, kEndStop, false },
	{ "arbres.3da", "ciel", kStepHalf, kEndDead, false }, { "pommes.3da", "apple01", kStepDt, kEndStop, false },
	{ "train.3da", "train01", kStepHalf, kEndStop, true }
};
const AnimDef kAnimTerrasse[] = {
	{ "terrasse.3da", "storeho", kStepDt, kEndStop, false }
};
const AnimDef kAnimJardin[] = {
	{ "barriere.3da", "barriere", kStepDt, kEndStop, false }, { "cerfvol.3da", "cerf", kStepDt, kEndStop, false },
	{ "papiyon1.3da", "fuzz", kStepDt, kEndLoop, true }, { "papiyon2.3da", "fuzz", kStepDt, kEndStop, false },
	{ "papiyon3.3da", "fuzz", kStepDt, kEndLoop, false }
};
const AnimDef kAnimChamp[] = {
	{ "korbeaux.3da", "corbopere", kStepDt, kEndStop, false }, { "faux.3da", "animfaux01", kStepDt, kEndStop, false }
};
const AnimDef kAnimEglise[] = {
	{ "eglise.3da", "cerf", kStepHalf, kEndLoop, true }
};

struct SceneAnims {
	const AnimDef *defs;
	uint count;
};
#define SA(a) { a, ARRAYSIZE(a) }
const SceneAnims kSceneAnims[kNumScenes] = {
	SA(kAnimMusee), SA(kAnimAuberge), SA(kAnimHopiext), SA(kAnimMaisonet), SA(kAnimMangeurs), SA(kAnimCafe),
	SA(kAnimChambreV), SA(kAnimMaisonj), SA(kAnimHopiint), SA(kAnimPont), SA(kAnimTerrasse), SA(kAnimJardin),
	SA(kAnimChamp), SA(kAnimEglise)
};
#undef SA

// ---------------------------------------------------------------------------
// Sounds each world loads at init; a leading '*' marks the looping ambience.

const char *const kSndMusee[] = { "*musee", "robot2", "etoile" };
const char *const kSndAuberge[] = { "*auberge" };
const char *const kSndHopiext[] = { "*hopi_ext", "portail", "cloche" };
const char *const kSndMaisonet[] = { "*ferme", "poule", "pelle", "oiseau" };
const char *const kSndMangeurs[] = { "*mangeurs", "coucou", "bebe", "fenclaq", "finfeu", "feu", "bouilloi", "chaise", "porpoel" };
const char *const kSndCafe[] = { "billiard", "*cafe_int", "pendule", "serrure", "tictac" };
const char *const kSndChambreV[] = { "*chambrVG", "placard" };
const char *const kSndChambreB[] = { "*chambrVG", "chaiseVG", "placard", "shoes", "miroir", "tiroir", "papier" };
const char *const kSndMaisonj[] = { "*maisjaun", "pastrain", "placard", "portail01" };
const char *const kSndHopiint[] = { "*hopi_int", "armoire", "fou" };
const char *const kSndPont[] = { "*pont", "pastrain", "pontlev2", "trappe" };
const char *const kSndTerrasse[] = { "*cafenuit", "store" };
const char *const kSndJardin[] = { "*jardin", "clochjar", "barriere" };
const char *const kSndChamp[] = { "*champ", "faux", "pistolet", "corbeaux" };
const char *const kSndEglise[] = { "*eglise" };

struct SceneSounds {
	const char *const *names;
	uint count;
};
#define SS(a) { a, ARRAYSIZE(a) }
const SceneSounds kSceneSounds[kNumScenes] = {
	SS(kSndMusee), SS(kSndAuberge), SS(kSndHopiext), SS(kSndMaisonet), SS(kSndMangeurs), SS(kSndCafe),
	SS(kSndChambreV), SS(kSndMaisonj), SS(kSndHopiint), SS(kSndPont), SS(kSndTerrasse), SS(kSndJardin),
	SS(kSndChamp), SS(kSndEglise)
};
#undef SS

// ---------------------------------------------------------------------------
// Arrival ("<scene>") and departure ("<scene>r") clips, sub_427290 / sub_426680.

const char *const kArrivalClips[kNumScenes] = {
	nullptr, nullptr, nullptr, "maisa", "mangeurs", "cafe", "chamba", "maisonj", "hopi", "pont", "terrasse", "jardin",
	"champ", "eglise"
};
const char *const kDepartureClips[kNumScenes] = {
	nullptr, nullptr, nullptr, "maisr", "mangeurr", "cafer", "chambr", "maisonjr", "hopir", "pontr", "terr", nullptr,
	"champr", "eglr"
};

const char *const kRobotParts[] = { "robot", "ecran", "cou", "oeild", "tete", "oeilg", nullptr };

} // end of anonymous namespace

// ---------------------------------------------------------------------------
// Game state.

void World::newGame() {
	_state.newGame();
}

void World::snapshotState() {
	const Pose3D p = pose();
	_state.set(kOfsCamRotA, (uint32)p.rotA);
	_state.set(kOfsCamRotB, (uint32)p.rotB);
	_state.set(kOfsCamRotC, 0);
	_state.set(kOfsCamX, (uint32)p.x);
	_state.set(kOfsCamY, (uint32)p.y);
	_state.set(kOfsCamZ, (uint32)p.z);
	_state.setByte(kOfsScene, (byte)_scene);
	_state.setByte(kOfsPrevScene, (byte)_prevScene);
}

void World::solvePuzzle(int id) {
	if (id < 0 || id >= GameState::kNumPuzzles)
		return;
	_state.setPuzzleSolved(id, true);
	_state.setByte(kOfsPage, (byte)id);
}

// ---------------------------------------------------------------------------
// Objects.

bool World::objectInfo(uint i, const char *&name, int &type, bool &hidden) const {
	if (i >= _numObjects)
		return false;
	name = _objects[i].name;
	type = _objects[i].type;
	hidden = _objects[i].node >= 0 && _renderer.nodeHidden(_objects[i].node);
	return true;
}

bool World::setObjectType(const char *name, int type) {
	for (uint i = 0; i < _numObjects; i++) {
		if (scumm_stricmp(_objects[i].name, name) == 0) {
			_objects[i].type = (byte)type;
			return true;
		}
	}
	debug("Vangogh: %s has no object '%s' to retype", sceneName(_scene).c_str(), name);
	return false;
}

void World::setTypes(int type, const char *const *names) {
	for (; *names; names++)
		setObjectType(*names, type);
}

bool World::setObjectHidden(const char *name, bool hidden) {
	const int n = _model.findNode(name);
	if (n < 0) {
		debug("Vangogh: %s has no node '%s' to %s", sceneName(_scene).c_str(), name, hidden ? "hide" : "show");
		return false;
	}
	_renderer.setNodeHidden(n, hidden);
	return true;
}

float World::objectDistance(const char *name) const {
	const int n = _model.findNode(name);
	if (n < 0 || _framesInScene < 1)
		return 1e9f;
	float v[3];
	_renderer.nodeViewPos(n, v);
	return sqrtf(v[0] * v[0] + v[2] * v[2]);
}

void World::describeObjects(Common::String &out) const {
	for (uint i = 0; i < _numObjects; i++) {
		const char *name;
		int type;
		bool hidden;
		objectInfo(i, name, type, hidden);
		float c[3];
		if (nodeCentre(name, c))
			out += Common::String::format("  %-12s type=%3d%s at %.0f,%.0f,%.0f\n", name, type, hidden ? " hidden" : "", c[0], c[1], c[2]);
		else
			out += Common::String::format("  %-12s type=%3d%s\n", name, type, hidden ? " hidden" : "");
	}
}

void World::describeReach(Common::String &out) {
	static const char *const kTargets[] = {
		"m01_02", "m01_03", "m03_01", "m03_02", "m03_03", "m03_04", "m03_05", "m03_06", "m04_01", "m04_02", "m04_03",
		"vase", "etoile", "robot"
	};
	const CollisionMesh &mesh = _boxes[_box];
	if (!_boxLoaded[_box] || !mesh.size()) {
		out += "reach: the scene has no active collision volume\n";
		return;
	}

	// Flood-fill the floor the camera can actually walk on: grid cells joined by sphere sweeps against the active
	// collision volume (so doors and walls count), starting from the camera. The painting click needs the camera
	// within kPickRange (horizontally), so a painting is reachable if some walkable cell is that close.
	const int step = 100;
	int32 lo[2] = { 0x7fffffff, 0x7fffffff }, hi[2] = { -0x7fffffff, -0x7fffffff };
	for (uint i = 0; i < mesh.size(); i++) {
		for (int k = 0; k < 3; k++) {
			lo[0] = MIN(lo[0], mesh.tri(i).v[k][0]); hi[0] = MAX(hi[0], mesh.tri(i).v[k][0]);
			lo[1] = MIN(lo[1], mesh.tri(i).v[k][2]); hi[1] = MAX(hi[1], mesh.tri(i).v[k][2]);
		}
	}
	const int nx = (hi[0] - lo[0]) / step + 1, nz = (hi[1] - lo[1]) / step + 1;
	if (nx <= 0 || nz <= 0 || nx > 512 || nz > 512) {
		out += "reach: collision volume is too large for the grid\n";
		return;
	}
	Common::Array<byte> seen(nx * nz);
	Common::Array<uint32> queue;
	queue.reserve(nx * nz);
	memset(seen.begin(), 0, seen.size());
	const int sx = CLIP((_pos[0] - lo[0]) / step, 0, nx - 1), sz = CLIP((_pos[2] - lo[1]) / step, 0, nz - 1);
	seen[sz * nx + sx] = 1;
	queue.push_back(sz * nx + sx);
	const int32 y = _pos[1];
	for (uint head = 0; head < queue.size(); head++) {
		const int cx = queue[head] % nx, cz = queue[head] / nx;
		static const int kDir[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		for (int d = 0; d < 4; d++) {
			const int mx = cx + kDir[d][0], mz = cz + kDir[d][1];
			if (mx < 0 || mz < 0 || mx >= nx || mz >= nz || seen[mz * nx + mx])
				continue;
			const int32 to[3] = { lo[0] + mx * step, y, lo[1] + mz * step };
			int32 floorY;
			if (!_collision.groundY(to, 500, floorY))
				continue;
			int32 from[3] = { lo[0] + cx * step, y, lo[1] + cz * step };
			if (_collision.moveSphere(to, from, kSphereRadius))
				continue;
			seen[mz * nx + mx] = 1;
			queue.push_back(mz * nx + mx);
		}
	}

	int32 ylo = 0x7fffffff, yhi = -0x7fffffff;
	for (uint i = 0; i < mesh.size(); i++) {
		for (int k = 0; k < 3; k++) {
			ylo = MIN(ylo, mesh.tri(i).v[k][1]);
			yhi = MAX(yhi, mesh.tri(i).v[k][1]);
		}
	}
	out += Common::String::format("collision volume BOX%d (BOX.3DI = 0): %u of %d grid cells (%d units) are walkable from the camera; extent x %d..%d y %d..%d z %d..%d\n",
		_box, queue.size(), nx * nz, step, lo[0], hi[0], ylo, yhi, lo[1], hi[1]);
	for (uint t = 0; _scene == kSceneMusee && t < ARRAYSIZE(kTargets); t++) {
		const int n = _model.findNode(kTargets[t]);
		if (n < 0)
			continue;
		float w[3];
		_model.worldPos(n, w);
		float best = 1e30f;
		for (uint q = 0; q < queue.size(); q++) {
			const float dx = lo[0] + (int)(queue[q] % nx) * step - w[0], dz = lo[1] + (int)(queue[q] / nx) * step - w[2];
			best = MIN(best, dx * dx + dz * dz);
		}
		best = sqrtf(best);
		out += Common::String::format("  %-8s nearest walkable point %6.0f away  %s\n", kTargets[t], best,
			best < kPickRange ? "REACHABLE" : "out of reach");
	}
}

bool World::animCommand(const Common::String &sel, const Common::String &op, int arg, Common::String &out) {
	if (sel.empty() || op == "list") {
		for (uint i = 0; i < _numAnims; i++) {
			AnimInfo a;
			animInfo(i, a);
			out += Common::String::format("  %u %-14s node %-10s %s t=%d/%d channels=%u animated=%u\n", i, a.clip, a.node,
				a.active ? "ACTIVE" : "idle  ", a.elapsed, a.duration, a.channels, a.animated);
		}
		return true;
	}
	const int i = findAnim(sel);
	if (i < 0) {
		out += Common::String::format("no animation '%s' in %s\n", sel.c_str(), sceneName(_scene).c_str());
		return false;
	}
	if (op == "start") {
		return startAnim(i);
	} else if (op == "stop") {
		return stopAnim(i);
	} else if (op == "end") {
		animToEnd(i);
		return true;
	} else if (op == "pose") {
		return poseAnim(i, arg);
	} else if (op == "tick") {
		_anims[i].active = true;
		stepAnims(CLIP(arg, 1, 10));
		return true;
	} else if (op == "check") {
		// Cross-check of the positional channel -> node mapping (anim.md sec.1.2): every channel that moves a node
		// should start at that node's rest position (the 3DC and 3DA were exported from the same scene).
		const AnimSlot &a = _anims[i];
		if (!a.loaded) {
			out += "animation not loaded\n";
			return false;
		}
		out += Common::String::format("  %s: 3DA channels=%u, 3DC nodes=%u, %u animate a node\n", a.file, a.clip.numChannels(),
			_model.numNodes(), a.clip.numAnimated());
		for (uint k = 0; k < a.clip.numChannels() && k < _model.numNodes(); k++) {
			uint nRot, nPos;
			a.clip.channelKeys(k, nRot, nPos);
			if (nRot < 2 && nPos < 2)
				continue;
			float rot;
			const int pos = a.clip.restPoseError(_model, k, rot);
			out += Common::String::format("    channel %2u -> node %-10s rotKeys=%u posKeys=%u key0 position delta %d\n", k,
				_model.node(k).name, nRot, nPos, pos);
		}
		return true;
	}
	out += "unknown operation (list, start, stop, end, pose, tick, check)\n";
	return false;
}

// ---------------------------------------------------------------------------
// Sound.

void World::playSound(const char *name, bool loop) {
	if (!g_engine || !g_engine->_sound)
		return;
	const uint16 h = g_engine->_sound->load(name);
	if (h)
		g_engine->_sound->play(h, loop);
}

void World::loadSounds() {
	if (!g_engine || !g_engine->_sound)
		return;
	Sound *snd = g_engine->_sound;
	snd->stopAll();
	_voice = 0;
	const SceneSounds &ss = kSceneSounds[_scene];
	for (uint i = 0; i < ss.count; i++) {
		const bool loop = ss.names[i][0] == '*';
		const uint16 h = snd->load(ss.names[i] + (loop ? 1 : 0));
		if (h && loop)
			snd->play(h, true);
	}
}

// ---------------------------------------------------------------------------
// 3DA animations.

void World::loadAnims(const BFGFile &bfg) {
	const SceneAnims &sa = _scene == kSceneChambre
		? SceneAnims{ _state.get(kCbVariant) ? kAnimChambreB : kAnimChambreV, _state.get(kCbVariant) ? (uint)ARRAYSIZE(kAnimChambreB) : (uint)ARRAYSIZE(kAnimChambreV) }
		: kSceneAnims[_scene];
	_numAnims = MIN<uint>(sa.count, kMaxAnims);
	for (uint i = 0; i < _numAnims; i++) {
		AnimSlot &a = _anims[i];
		a.file = sa.defs[i].file;
		a.node = sa.defs[i].node;
		a.step = sa.defs[i].step;
		a.end = sa.defs[i].end;
		a.elapsed = 1;
		a.loaded = a.clip.load(bfg, a.file);
		if (!a.loaded)
			warning("Vangogh: %s: animation %s is missing", sceneName(_scene).c_str(), a.file);
		a.active = a.loaded && sa.defs[i].active;
	}
	// Scene is set by enterScene() before this call.
	_trainSound = false;
}

int World::animEndFrame(uint i) const {
	const int d = _anims[i].clip.duration();
	return _scene == kSceneAuberge ? d / 2 : d;
}

bool World::animInfo(uint i, AnimInfo &out) const {
	if (i >= _numAnims)
		return false;
	const AnimSlot &a = _anims[i];
	out.clip = a.file;
	out.node = a.node;
	out.active = a.active;
	out.elapsed = a.elapsed;
	out.duration = a.clip.duration();
	out.channels = a.clip.numChannels();
	out.animated = a.clip.numAnimated();
	return true;
}

int World::findAnim(const Common::String &name) const {
	char *end = nullptr;
	const long n = strtol(name.c_str(), &end, 10);
	if (!name.empty() && *end == '\0')
		return (n >= 0 && n < (long)_numAnims) ? (int)n : -1;
	for (uint i = 0; i < _numAnims; i++) {
		Common::String f(_anims[i].file);
		const int dot = f.findLastOf('.');
		const Common::String base = dot > 0 ? Common::String(f.c_str(), dot) : f;
		if (name.equalsIgnoreCase(f) || name.equalsIgnoreCase(base))
			return i;
	}
	return -1;
}

bool World::startAnim(uint i) {
	if (i >= _numAnims || !_anims[i].loaded || _anims[i].end == kEndDead)
		return false;
	_anims[i].elapsed = 1;
	_anims[i].active = true;
	return true;
}

bool World::stopAnim(uint i) {
	if (i >= _numAnims)
		return false;
	_anims[i].active = false;
	return true;
}

bool World::poseAnim(uint i, int t) {
	if (i >= _numAnims || !_anims[i].loaded)
		return false;
	_anims[i].clip.apply(_model, t);
	return true;
}

void World::animToEnd(uint i) {
	// The init code's "last pose": the frame before the end, animation idle.
	if (i >= _numAnims || !_anims[i].loaded)
		return;
	const int t = MAX(1, animEndFrame(i) - 1);
	_anims[i].active = false;
	_anims[i].elapsed = t;
	_anims[i].clip.apply(_model, t);
}

void World::stepAnims(int dt) {
	for (uint i = 0; i < _numAnims; i++) {
		AnimSlot &a = _anims[i];
		if (!a.active || !a.loaded)
			continue;
		a.elapsed += a.step == kStepHalf ? MAX(1, dt >> 1) : (a.step == kStepOne ? 1 : dt);

		if (_scene == kSceneMusee && i == 0 && a.elapsed >= 0x34 && !_state.isSet(kOfsStarTaken)) {
			// The star hovers at frame 52 until it is picked up.
			a.elapsed = 0x34;
			a.active = false;
			a.clip.apply(_model, a.elapsed);
			continue;
		}
		if (_scene == kSceneMaisonj && i == 4 && a.elapsed >= 0x47 && !_trainSound) {
			_trainSound = true;
			playSound("pastrain");
		}

		if (animEndFrame(i) <= a.elapsed) {
			if (a.end == kEndLoop) {
				a.elapsed = 1;
				a.clip.apply(_model, 1);
			} else {
				a.active = false;
				animDone(i);
			}
			continue;
		}
		a.clip.apply(_model, a.elapsed);
	}
}

void World::selectBoxMaisonj() {
	const bool gate = _state.isSet(kMjGateOpen), door = _state.isSet(kMjDoorOpen);
	setBox(gate ? (door ? 2 : 1) : (door ? 3 : 0));
}

/** What a finished animation changes: the state flag, the collision volume, the pickable types. */
void World::animDone(uint i) {
	GameState &s = _state;
	switch (_scene) {
	case kSceneMusee:
		if (i == 1) {			// robot01 done: robot02 idle loop
			startAnim(2);
		} else if (i == 3) {	// robot03 (never started by the exe)
			for (uint k = 0; k < 4; k++)
				_anims[k].active = false;
			hideObj("robot");
		}
		break;
	case kSceneAuberge:
		if (i == 0) {
			_anims[0].elapsed = 1;
			s.set(kAuBagOpen, 1);
			const char *const bag[] = { "saccoche", "sac", "sac01", nullptr };
			setTypes(255, bag);
		} else if (i == 1) {
			s.set(kAuUpperDoorOpen, 1);
			setBox(2);
			setObjectType("portehaut", 255);
		} else {
			s.set(kAuLowerDoorOpen, 1);
			setBox(1);
			setObjectType("portebas", 255);
		}
		break;
	case kSceneHopiext:
		if (i == 1) {			// bell rang: the grille starts to lift
			_anims[1].elapsed = 1;
			startAnim(0);
			playSound("portail");	// the cloche sound itself plays on the click
		} else {
			s.set(kHxGridOpen, 1);
			setBox(1);
			const char *const t[] = { "cloche", "korde1", "korde2", "korde3", "korde4", "grilledrt", "grillegche", nullptr };
			setTypes(255, t);
		}
		break;
	case kSceneMaisonet:
		if (i == 0) {
			s.set(kMeHenFled, 1);
			_anims[0].elapsed = _anims[0].clip.duration() - 1;
		} else if (i == 1) {
			s.set(kMeDigDone, 1);
			setObjectType("pelle", 255);
			if (!s.isSet(kMeEarthTaken)) {
				showObj("terre");
				setObjectType("terre", 2);
			}
		} else if (i == 2) {
			s.set(kMeBirdFlew, 1);
			startAnim(3);
		} else {
			startAnim(2);
		}
		break;
	case kSceneMangeurs:
		if (i == 3) {
			_anims[3].elapsed = _anims[3].clip.duration() - 1;
		} else if (i == 4) {
			setObjectType("portepoel", 255);
			setObjectType("fagot", 4);
		}
		break;
	case kSceneMaisonj:
		if (i == 2) {
			s.set(kMjGateOpen, 1);
			const char *const t[] = { "portail", "portail01", nullptr };
			setTypes(255, t);
			selectBoxMaisonj();
		} else if (i == 3) {
			s.set(kMjDoorOpen, 1);
			setObjectType("portemj", 255);
			selectBoxMaisonj();
		} else if (i == 4) {
			_anims[4].elapsed = 1;
		}
		break;
	case kSceneCafe:
		if (i == 0) {			// billiard done: the shadow ball can be taken
			_billiardRt = true;
			setObjectType("ombre", 2);
			setObjectType("ke", 255);
		}
		break;
	case kSceneHopiint:
		if (i == 0) {
			s.set(kHiWardrobeOpen, 1);
			setObjectType("ARMOIRE", 255);
		}
		break;
	case kScenePont:
		if (i == 0) {
			s.set(kPoBridgeDown, 1);
			setBox(0);
			const char *const t[] = { "pouli02", "pouli03", nullptr };
			setTypes(255, t);
		} else if (i == 1) {
			s.set(kPoTrapOpen, 1);
			setObjectType("trappe", 255);
		} else if (i == 3) {
			s.set(kPoAppleFallen, 1);
		} else if (i == 4 && !_trainSound) {
			_trainSound = true;
			playSound("pastrain");
		}
		break;
	case kSceneTerrasse:
		s.set(kTeStoreUp, 1);
		setObjectType("MANIVELLE", 255);
		break;
	case kSceneJardin:
		if (i == 0) {
			s.set(kJaGateOpen, 1);
			setObjectType("barriere", 255);
			setBox(1);
		} else if (i == 3) {	// papiyon2 done: papiyon1 again
			startAnim(2);
		}
		break;
	case kSceneChamp:
		if (i == 1) {
			s.set(kChScytheDone, 1);
			setObjectType("faux", 255);
		}
		break;
	case kSceneChambre:
		if (!s.get(kCbVariant)) {
			if (i == 0) {
				s.set(kCbVDoorOpen, 1);
				setObjectType("porte", 255);
			}
			break;
		}
		if (i == 0) {
			s.set(kCbChairMoved, 1);
			setObjectType("porte", 4);
			setObjectType("CHAISE1", 255);
		} else if (i == 3) {
			s.set(kCbPaperOpen, 1);
			setObjectType("papierferm", 255);
		} else if (i == 4) {
			s.set(kCbWardrobeOpen, 1);
			setObjectType("porte", 255);
			setObjectType("camee", 2);
			setObjectType("ombrelle", 2);
		} else if (i == 5) {
			s.set(kCbDrawerOpen, 1);
			setObjectType("TIROIR", 3);
		}
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Scene init (the exe's per-world init functions, world.md sec.10).

void World::initSceneState(Pose3D *pose) {
	GameState &s = _state;
	loadSounds();
	memset(_timer, 0, sizeof(_timer));
	_timerLimit = 5;

	switch (_scene) {
	case kSceneMusee: {
		setBox(1);
		hideObj("prtholl");
		hideObj("prtarles");
		hideObj("prtauvers");

		// Wing gating: which collision volume the museum starts in.
		bool a1to3 = s.puzzleSolved(1) && s.puzzleSolved(2) && s.puzzleSolved(3);
		bool a4to18 = true, any1to3 = false, any4to18 = false, any19to24 = false;
		for (int i = 1; i <= 3; i++)
			any1to3 |= !s.puzzleSolved(i);
		for (int i = 4; i <= 18; i++) {
			a4to18 &= s.puzzleSolved(i);
			any4to18 |= !s.puzzleSolved(i);
		}
		for (int i = 19; i <= 24; i++)
			any19to24 |= !s.puzzleSolved(i);
		if (s.puzzleSolved(0) && !s.isSet(kOfsActEntered1)) {
			setBox(2);
			hideObj("prtarles");
			s.set(kOfsCurAct1, 0); s.set(kOfsCurAct2, 0); s.set(kOfsCurAct3, 0);
		}
		if (a1to3 && !s.isSet(kOfsActEntered2)) {
			setBox(3);
			hideObj("prtarles");
			s.set(kOfsCurAct1, 0); s.set(kOfsCurAct2, 0); s.set(kOfsCurAct3, 0);
		}
		if (a4to18 && !s.isSet(kOfsActEntered3)) {
			setBox(0);
			hideObj("prtauvers");
			s.set(kOfsCurAct1, 0); s.set(kOfsCurAct2, 0); s.set(kOfsCurAct3, 0);
		}
		if (s.get(kOfsCurAct1) == 1 && s.isSet(kOfsActEntered1) && any1to3) {
			setBox(4);
			showObj("prtholl");
		}
		if (s.get(kOfsCurAct2) == 1 && s.isSet(kOfsActEntered2) && any4to18) {
			setBox(4);
			showObj("prtarles");
		}
		if (s.get(kOfsCurAct3) == 1 && s.isSet(kOfsActEntered3) && any19to24) {
			setBox(4);
			showObj("prtauvers");
		}

		if (s.puzzleSolved(0))
			setObjectType("vase", 255);
		else if (s.isSet(kOfsStarTaken))
			setObjectType("vase", 3);
		if (s.isSet(kOfsEnding))
			setBox(1);
		if (s.isSet(kOfsRobotOff))
			setTypes(255, kRobotParts);

		const byte sf = s.getByte(kOfsSunflowers);
		if ((sf == 1 && !s.isSet(kOfsActEntered1)) || (sf == 3 && !s.isSet(kOfsActEntered2)) || (sf == 0xf && !s.isSet(kOfsActEntered3)))
			s.setByte(kOfsSunflowers, 0);

		if (!s.isSet(kOfsStarDeposited)) {
			// First visit: camera at the boot pose, robot flags reset.
			if (pose) {
				pose->x = -39; pose->y = -209; pose->z = 361; pose->rotA = 4066; pose->rotB = 20;
			}
			s.set(kOfsRobotMet, 0);
			for (uint i = 1; i < 4; i++)
				_anims[i].active = false;
		} else if (s.isSet(kOfsRobotMet)) {
			startAnim(2);
		}

		// Solved paintings are recoloured (sub_43c0b0: the painting's own texture -> its RVB* "restored" texture)
		// and become 0x3c markers.
		struct Painting {
			const char *name;
			int first, last;
			int second;
			const char *base, *rvb;
		};
		static const Painting kPaintings[] = {
			{ "m01_02", 1, 1, -1, "PATATHOM", "RVBPATAT" }, { "m01_03", 2, 3, -1, "MANGEUR", "RVBMANGE" },
			{ "m03_01", 4, 6, -1, "CAFE", "RVBCAFE" }, { "m03_02", 7, 11, -1, "CHAMBRE", "RVBCHAMB" },
			{ "m03_03", 12, 12, -1, "MAISONJ", "RVBMAISO" }, { "m03_04", 13, 15, -1, "HOSTO", "RVBHOSTO" },
			{ "m03_05", 16, 17, -1, "TERRASSE", "RVBTERRA" }, { "m03_06", 18, 18, -1, "PONT", "RVBPONT" },
			{ "m04_02", 22, 22, 24, "CHAMP", "RVBCHAMP" }, { "m04_03", 23, 23, -1, "EGLISE", "RVBEGLIS" }
		};
		_model.retexture("ROBI3", "ROBI3N");
		_robotTalk = _robotUV = _robotUVStep = 0;
		_robotUVDir = 1;
		for (uint k = 0; k < ARRAYSIZE(kPaintings); k++) {
			const Painting &p = kPaintings[k];
			bool all = true;
			for (int i = p.first; i <= p.last; i++)
				all &= s.puzzleSolved(i);
			if (p.second >= 0)
				all &= s.puzzleSolved(p.second);
			if (all) {
				setObjectType(p.name, 0x3c);
				_model.retexture(p.base, p.rvb);
			}
		}
		break;
	}

	case kSceneAuberge:
		if (s.isSet(kAuBagOpen)) {
			animToEnd(0);
			const char *const bag[] = { "saccoche", "sac", "sac01", nullptr };
			setTypes(255, bag);
		}
		if (s.isSet(kAuStethoscopeTaken))
			hideObj("stetoscop");
		if (s.isSet(kAuLowerDoorOpen)) {
			animToEnd(2);
			setBox(1);
			setObjectType("portebas", 255);
		}
		if (s.isSet(kAuUpperDoorOpen)) {
			animToEnd(1);
			setBox(2);
			setObjectType("portehaut", 255);
		}
		if (s.getByte(kOfsSunflowers) == 5)
			setObjectType("tableau", 3);
		if (s.puzzleSolved(20))
			setObjectType("casquette", 0x3c);
		break;

	case kSceneHopiext:
		if (!s.isSet(kHxGridOpen)) {
			const char *const t[] = { "cloche", "korde1", "korde2", "korde3", "korde4", "grilledrt", "grillegche", nullptr };
			setTypes(4, t);
		} else {
			animToEnd(0);
			const char *const t[] = { "cloche", "korde1", "korde2", "korde3", "korde4", "grilledrt", "grillegche", nullptr };
			setTypes(255, t);
			setBox(1);
		}
		if (s.puzzleSolved(14))
			setObjectType("arbre11", 0x3c);
		break;

	case kSceneMaisonet:
		if (s.isSet(kMeHenFled) && _anims[0].loaded) {
			_anims[0].elapsed = _anims[0].clip.duration() - 1;
			poseAnim(0, _anims[0].elapsed);
		}
		if (s.isSet(kMeDigDone)) {
			// The exe finishes the shovel with the hen clip's length (4f7d14).
			if (_anims[1].loaded)
				_anims[1].clip.apply(_model, MAX<int>(1, _anims[0].clip.duration() - 1));
			setObjectType("pelle", 255);
		}
		if (s.isSet(kMeFeatherTaken))
			hideObj("plume");
		if (s.isSet(kMeEarthTaken) || !s.isSet(kMeDigDone))
			hideObj("terre");
		else
			setObjectType("terre", 2);
		if (s.puzzleSolved(1))
			setObjectType("nid", 0x3c);
		break;

	case kSceneMangeurs:
		hideObj("vapeur");
		if (s.isSet(kMgStoveOpen)) {
			animToEnd(4);
			setObjectType("portepoel", 255);
			setObjectType("fagot", 4);
		}
		// sub_41f390: timers, the window, the fire and the kettle (decomp 13268-13315).
		_mgTick = _mgCry = _mgBoil = 0;
		_mgCrying = _mgWindowRt = false;
		_mgCoucou = true;
		if (s.isSet(kMgKindlingPlaced)) {
			hideObj("fagot");
			setObjectType("buche", 4);
		}
		if (s.isSet(kMgWindowClosed)) {
			animToEnd(3);
			_mgWindowRt = _mgCrying = true;
			setObjectType("bersso", 3);
		} else {
			playSound("fenclaq", true);
		}
		if (s.isSet(kMgLogPlaced)) {
			hideObj("buche");
			playSound("feu", true);
			if (!s.isSet(kMgKettleTaken))
				playSound("bouilloi", true);
		}
		if (s.puzzleSolved(2))
			setObjectType("bersso", 0x3c);
		if (s.puzzleSolved(3))
			setObjectType("POT", 0x3c);
		if (s.isSet(kMgPotatoTaken))
			hideObj("patat01");
		if (s.isSet(kMgKettleTaken))
			hideObj("theieres");
		if (s.isSet(kMgCuckooPending)) {
			playSound("coucou");
			s.set(kMgCuckooPending, 0);
		}
		break;

	case kSceneCafe:
		_clockTimer = _clockLast = _clockState = 0;
		_clockEnable = true;
		_billiardRt = false;
		Common::strlcpy(_mirrorTex, "MIRROIRG", sizeof(_mirrorTex));
		setObjectType("mirroir", 255);
		if (!s.isSet(kCfClockTaken)) {
			if (g_engine && g_engine->_sound) {
				const uint16 h = g_engine->_sound->load("tictac");
				if (h)
					g_engine->_sound->play(h, true);
			}
			setObjectType("orloge", 255);
		} else {
			hideObj("orloge");
		}
		if (s.isSet(kCfBilliardDone)) {
			hideObj("ombre");
			setObjectType("ke", 255);
			animToEnd(0);
		} else {
			setObjectType("ke", 4);
			setObjectType("ombre", 255);
		}
		if (s.isSet(kCfMapTaken))
			hideObj("carte");
		if (s.isSet(kCfBriketTaken))
			hideObj("briket");
		if (s.isSet(kCfPitcherTaken))
			hideObj("pichet");
		if (s.isSet(kCfTeapotTaken))
			hideObj("theiere");
		if (s.isSet(kCfDoorOpen)) {
			hideObj("clef");
			animToEnd(1);
		}
		if (s.puzzleSolved(4))
			setObjectType("lampe4", 0x3c);
		if (s.puzzleSolved(5))
			setObjectType("gant", 0x3c);
		if (s.puzzleSolved(6))
			setObjectType("mirroir", 0x3c);
		break;

	case kSceneChambre:
		if (!s.get(kCbVariant)) {
			if (!s.isSet(kCbVDoorOpen)) {
				setObjectType("porte", 4);
			} else {
				animToEnd(0);
				setObjectType("porte", 255);
			}
			break;
		}
		hideObj("mirroircas");
		hideObj("mirroirmor");
		if (s.isSet(kCbObject09Taken)) hideObj("Object09");
		if (s.isSet(kCbObject07Taken)) hideObj("Object07");
		if (s.isSet(kCbFrameTaken)) hideObj("cadre");
		if (s.isSet(kCbObject02Taken)) hideObj("Object02");
		if (s.isSet(kCbUmbrellaTaken)) hideObj("ombrelle");
		if (s.isSet(kCbCameoTaken)) hideObj("camee");
		if (s.isSet(kCbMirrorShardTaken)) hideObj("mirroirmor");
		if (s.isSet(kCbShoesTaken)) {
			hideObj("shoes");
			hideObj("shoes01");
		}
		if (s.isSet(kCbWardrobeOpen)) {
			animToEnd(4);
			setObjectType("porte", 255);
			setObjectType("camee", 2);
			setObjectType("ombrelle", 2);
		} else {
			setObjectType("camee", 255);
			setObjectType("ombrelle", 255);
			setObjectType("porte", 4);
		}
		if (s.isSet(kCbObject09Taken))
			setObjectType("TIROIR", 4);
		if (s.isSet(kCbMirrorBroken)) {
			hideObj("mirroir1");
			showObj("mirroircas");
			animToEnd(2);
			if (!s.isSet(kCbMirrorShardTaken))
				showObj("mirroirmor");
		}
		if (s.isSet(kCbDrawerOpen)) {
			animToEnd(5);
			setObjectType("TIROIR", 3);
		}
		if (s.isSet(kCbChairMoved)) {
			animToEnd(0);
			setObjectType("CHAISE1", 255);
			setObjectType("porte", 4);
		}
		if (!s.isSet(kCbPaperOpen)) {
			poseAnim(3, 1);
			setObjectType("papierferm", 4);
		} else {
			animToEnd(3);
			setObjectType("papierferm", 255);
		}
		if (s.puzzleSolved(7)) {
			// Decomp 11360-11365: the pillow solved turns both canvases into the razor-sketch texture.
			setObjectType("tabpay", 3);
			setObjectType("tab01", 3);
			_model.retexture("TOILES2", "RASOIR");
			_model.retexture("TOILES", "RASOIR");
		}
		// Decomp 11366-11369: page 11 was left with the mirror broken; the room plays "miroir" once.
		if (s.mirrorSoundPending && !s.isSet(kCbMirrorSoundPlayed)) {
			s.set(kCbMirrorSoundPlayed, 1);
			playSound("miroir");
		}
		if (s.puzzleSolved(7)) setObjectType("oreiller", 0x3c);
		if (s.puzzleSolved(8)) setObjectType("tabpay", 0x3c);
		if (s.puzzleSolved(9)) setObjectType("TIROIR", 0x3c);
		if (s.puzzleSolved(10)) setObjectType("tab02", 0x3c);
		if (s.puzzleSolved(11)) setObjectType("tab01", 0x3c);
		if (!s.isSet(kCbShoesEnabled)) {
			setObjectType("shoes", 255);
			setObjectType("shoes01", 255);
			_anims[1].active = false;
		} else {
			_anims[1].active = _anims[1].loaded && !s.isSet(kCbShoesTaken);
		}
		break;

	case kSceneMaisonj:
		// Decomp 12385-12403: the house starts in its MJ* textures, WMAP*, and turns yellow once pot02 was used.
		_model.retexture("MJ01", "WMAP01");
		_model.retexture("MJ02", "WMAP02");
		if (s.isSet(kMjYellowDone))
			yellowHouse();
		if (s.isSet(kMjYellowDone) || s.isSet(kMjRainSeen))
			setObjectType("pot01", 255);
		else
			setObjectType("pot01", 4);
		setObjectType("pot02", 255);
		hideObj("nuages");
		if (s.isSet(kMjSnailTaken) || !s.isSet(kMjSnailReleased))
			hideObj("escargot");
		if (s.puzzleSolved(12))
			setObjectType("pendule", 0x3c);
		if (!s.isSet(kMjGateOpen)) {
			setObjectType("portail", 4);
			setObjectType("portail01", 4);
		} else {
			animToEnd(2);
		}
		if (!s.isSet(kMjDoorOpen))
			setObjectType("portemj", 4);
		else
			animToEnd(3);
		selectBoxMaisonj();
		break;

	case kSceneHopiint:
		if (!s.isSet(kHiWardrobeOpen)) {
			setObjectType("ARMOIRE", 4);
		} else {
			animToEnd(0);
			setObjectType("ARMOIRE", 255);
		}
		if (s.isSet(kHiCrossTaken)) hideObj("CROIX");
		if (s.isSet(kHiLampTaken)) hideObj("LAMPE");
		if (s.isSet(kHiBonnetTaken)) hideObj("BONNET");
		if (s.isSet(kHiCandleTaken)) hideObj("BOUGIE");
		if (s.puzzleSolved(13))
			setObjectType("ECHIK", 0x3c);
		if (s.puzzleSolved(15))
			setObjectType("MIRROIRVG", 0x3c);
		break;

	case kScenePont:
		if (s.isSet(kPoAppleTaken)) hideObj("apple01");
		if (s.isSet(kPoInkTaken)) hideObj("encre01");
		setObjectType("trappe", s.isSet(kPoTrapOpen) ? 255 : 4);
		if (s.isSet(kPoTrapOpen))
			animToEnd(1);
		if (s.isSet(kPoBridgeDown)) {
			const char *const t[] = { "pouli03", "pouli02", nullptr };
			setTypes(255, t);
			animToEnd(0);
		} else {
			const char *const t[] = { "pouli03", "pouli02", nullptr };
			setTypes(4, t);
			setBox(1);
		}
		if (s.isSet(kPoHandleMounted))
			hideObj("poignee04");
		else
			hideObj("poignee02");
		if (s.puzzleSolved(18))
			setObjectType("chemise", 0x3c);
		break;

	case kSceneTerrasse:
		_groundProbe = 0xf0;
		if (s.isSet(kTeLetterTaken)) hideObj("lettre");
		if (s.isSet(kTeDieTaken)) {
			hideObj("de");
			setObjectType("platode", 255);
		}
		if (s.isSet(kTeGlassesTaken)) hideObj("lunette");
		if (!s.isSet(kTeStoreUp)) {
			setObjectType("MANIVELLE", 4);
			poseAnim(0, 1);
		} else {
			setObjectType("MANIVELLE", 255);
			animToEnd(0);
		}
		if (s.puzzleSolved(17)) setObjectType("KASKET", 0x3c);
		if (s.puzzleSolved(16)) setObjectType("drapo", 0x3c);
		if (!s.isSet(kTeIntroPlayed)) {
			s.set(kTeIntroPlayed, 1);
			if (g_engine && g_engine->_sound) {
				_voice = g_engine->_sound->load("_apierre");
				if (_voice)
					g_engine->_sound->play(_voice, false);
			}
		}
		break;

	case kSceneJardin:
		if (s.isSet(kJaFuzzTaken)) hideObj("fuzz");
		if (s.isSet(kJaRatoTaken)) hideObj("rato");
		if (s.isSet(kJaGateOpen)) {
			animToEnd(0);
			setObjectType("barriere", 255);
			setBox(1);
		} else {
			setObjectType("barriere", 4);
		}
		if (s.isSet(kJaKiteFlown))
			hideObj("cerf");
		if (s.puzzleSolved(19))
			setObjectType("partoche", 0x3c);
		break;

	case kSceneChamp:
		if (s.getByte(kOfsSunflowers) != 4) {
			hideObj("faux");
		} else if (!s.isSet(kChScytheDone)) {
			setObjectType("faux", 4);
		} else {
			animToEnd(1);
			setObjectType("faux", 255);
		}
		if (s.isSet(kChGunTaken))
			hideObj("gun");
		if (s.puzzleSolved(22))
			setObjectType("coclicot", 0x3c);
		if (s.puzzleSolved(24))
			setObjectType("sabots", 0x3c);
		break;

	case kSceneEglise:
		if (s.get(kEgKiteAway) == 1 || s.get(kEgKiteDone) == 1)
			hideObj("cerf");
		if (s.isSet(kEgSheafTaken))
			hideObj("gerbe");
		if (s.puzzleSolved(23))
			setObjectType("eglise10", 0x3c);
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Scene tick: the proximity triggers that run without a click.

void World::sceneTick(int dt) {
	GameState &s = _state;
	switch (_scene) {
	case kSceneMusee:
		// Robot greeting (first visit): proximity starts robot01 and the dialogue.
		if (!_anims[1].active && !_anims[2].active && !_anims[3].active && !s.isSet(kOfsRobotMet)
				&& objectDistance("robot") < 1500.0f) {
			startAnim(1);
			if (g_engine && g_engine->_sound) {
				_voice = g_engine->_sound->load("_a50_01");
				if (_voice)
					g_engine->_sound->play(_voice, false);
			}
			_timer[0] = 1;		// dialogue running
			_timer[1] = 1;		// next voice line
			playSound("robot2", true);
			s.set(kOfsRobotMet, 1);
			setTypes(255, kRobotParts);
		}
		if (_timer[0] == 1 && (!_voice || !g_engine || !g_engine->_sound || !g_engine->_sound->isPlaying(_voice))) {
			if (_timer[1] != 2) {
				_timer[1]++;
				if (g_engine && g_engine->_sound) {
					_voice = g_engine->_sound->load("_a50_01c");
					if (_voice)
						g_engine->_sound->play(_voice, false);
				}
				// sub_42b320's talking screen: ROBI3N -> ROBI3 while the second line plays.
				_robotTalk = 1;
				_model.retexture("ROBI3N", "ROBI3");
			} else {
				_timer[0] = 0;
				if (_robotTalk) {
					_robotTalk = 0;
					_model.retexture("ROBI3", "ROBI3N");
				}
				if (!s.isSet(kOfsStarTaken) && _anims[0].elapsed != 0x34) {
					startAnim(0);
					playSound("etoile");
				}
				setTypes(4, kRobotParts);
			}
		}
		if (_robotTalk) {
			// sub_42b320: while the robot talks the screen texture scrolls 127 texels per step, 6 steps down and back.
			_robotUV += dt;
			if (_robotUV >= 0xb) {
				_robotUV = 0;
				const int ecran = _model.findNode("ecran");
				if (ecran >= 0) {
					_model.shiftNodeUV(ecran, 0.0f, _robotUVDir ? 127.0f : -127.0f, false);
					_robotUVStep += _robotUVDir ? 1 : -1;
					if (_robotUVDir && _robotUVStep == 6)
						_robotUVDir = 0;
					else if (!_robotUVDir && _robotUVStep == 0)
						_robotUVDir = 1;
				}
			}
		}
		// TickMusee (decomp 17893): with the ending flag set, walking down the hall to z <= 0xdab plays "fin".
		if (s.isSet(kOfsEnding) && !_finRequest && (int16)_pos[2] <= 0xdab)
			_finRequest = true;
		break;

	case kSceneCafe:
		// Clock (cb 0x417cd0): every 200 ticks the hands texture steps; the third step rings and makes it pickable.
		_clockTimer += dt;
		if (_clockTimer - _clockLast >= 200 && _clockEnable && !s.isSet(kCfClockTaken)) {
			_clockState++;
			_clockLast = _clockTimer;
			const int orloge = _model.findNode("orloge");
			if (orloge >= 0) {
				if (_clockState == 1) {
					_model.shiftNodeUV(orloge, 128.0f, 0.0f, false);
				} else if (_clockState == 2) {
					_model.shiftNodeUV(orloge, -128.0f, 128.0f, false);
				} else if (_clockState == 3) {
					_model.shiftNodeUV(orloge, 128.0f, 0.0f, false);
					_clockEnable = false;
					playSound("pendule");
					setObjectType("orloge", 2);
				}
			}
		}
		mirrorTick();
		break;

	case kSceneMangeurs:
		mangeursTick(dt);
		break;

	case kSceneMaisonet:
		// Hen: she runs off when you get within 2000; the bird flies off when you get within 5000 of the nest.
		if (!_anims[0].active && _anims[0].elapsed == 1 && !s.isSet(kMeHenFled) && objectDistance("poule") < 2000.0f) {
			startAnim(0);
			playSound("poule");
		}
		if (!_anims[2].active && !_anims[3].active && !s.isSet(kMeBirdFlew) && objectDistance("nid") < 5000.0f) {
			playSound("oiseau");
			startAnim(2);
		}
		break;

	case kSceneJardin:
		// Butterflies: papiyon1 flutters, every so often papiyon2 takes over for one pass.
		if (_anims[2].active) {
			_timer[2] += dt;
			if (_timer[2] > _timerLimit) {
				_timer[2] = 0;
				_timerLimit += 0x14;
				stopAnim(2);
				startAnim(3);
			}
		}
		break;

	case kSceneHopiint: {
		const bool near = objectDistance("ECHIK") < 1500.0f;
		if (near && !_anims[1].active) {
			startAnim(1);
			playSound("fou");
		} else if (!near && _anims[1].active) {
			stopAnim(1);
		}
		break;
	}

	case kScenePont:
		if (!s.isSet(kPoAppleFallen) && !_anims[3].active && objectDistance("apple01") < 2000.0f) {
			startAnim(3);
		}
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Textures (sub_43ded0 registrations, sub_43c0b0 swaps).

void World::registerTextures(const BFGFile &bfg) {
	struct Alias {
		const char *name, *file;
	};
	static const char *const kMusee[] = {
		"ROBI3", "ROBI3N", "RVBTERRA", "RVBMAISO", "RVBCAFE", "RVBPONT", "RVBPATAT", "RVBCHAMB", "RVBPOSTE",
		"RVBHOSTO", "RVBMANGE", "RVBCHAMP", "RVBJARDI", "RVBEGLIS", nullptr
	};
	static const char *const kCafe[] = { "MIRROIR1", "MIRROIR2", "MIRROIR3", "MIRROIRG", nullptr };
	static const Alias kMaisonj[] = {
		{ "WMAP01", "WMJ01" }, { "WMAP02", "WMJ02" }, { "BMAP01", "BMJ01" }, { "BMAP02", "BMJ02" },
		{ "JMAP01", "JMJ01" }, { "JMAP02", "JMJ02" }
	};

	switch (_scene) {
	case kSceneMusee:
		for (const char *const *n = kMusee; *n; n++)
			_model.registerTexture(bfg, *n, *n);
		break;
	case kSceneCafe:
		for (const char *const *n = kCafe; *n; n++)
			_model.registerTexture(bfg, *n, *n);
		break;
	case kSceneMaisonj:
		for (uint i = 0; i < ARRAYSIZE(kMaisonj); i++)
			_model.registerTexture(bfg, kMaisonj[i].name, kMaisonj[i].file);
		break;
	default:
		break;
	}
}

void World::yellowHouse() {
	_model.retexture("WMAP01", "JMAP01");
	_model.retexture("WMAP02", "JMAP02");
}

/** The cafe mirror: its texture follows the camera's z (decomp 11167-11194); only the middle band is a puzzle. */
void World::mirrorTick() {
	const int z = (int16)_pos[2];
	const char *tex;
	int type = 255;
	if (z > 0xaf0)
		tex = "MIRROIR3";
	else if (z > 0x8fc)
		tex = "MIRROIR2";
	else if (z > 0x7d0)
		tex = "MIRROIR1";
	else if (z > 0x320) {
		tex = "MIRROIRG";
		type = _state.puzzleSolved(6) ? 0x3c : 3;
	} else if (z > 0x1f4)
		tex = "MIRROIR1";
	else if (z > 0x71)
		tex = "MIRROIR2";
	else
		tex = "MIRROIR3";
	if (scumm_stricmp(tex, _mirrorTex) != 0) {
		_model.retexture(_mirrorTex, tex);
		Common::strlcpy(_mirrorTex, tex, sizeof(_mirrorTex));
	}
	setObjectType("mirroir", type);
}

// ---------------------------------------------------------------------------
// Mangeurs (sub_41f9f0 tail, decomp 13498-13545).

void World::closeMangeursWindow() {
	_mgWindowRt = true;
	playSound("chaise");
	stopSoundNamed("bebe");
	stopSoundNamed("fenclaq");
	startAnim(3);
	_state.set(kMgWindowClosed, 1);
	setObjectType("bersso", 3);
	setObjectType("fenetre", 255);
}

void World::mangeursTick(int dt) {
	GameState &s = _state;
	// The baby starts to cry after 0x33 ticks and wakes up the window.
	if (_mgTick >= 0x33 && !_mgCrying && _mgCoucou && !s.isSet(kMgKettleTaken)) {
		setObjectType("fenetre", 4);
		playSound("bebe", true);
		_mgCrying = true;
	}
	// ...which closes by itself after 0x1f5 ticks if nobody does it.
	if (_mgCrying && !_mgWindowRt) {
		_mgCry += dt;
		if (_mgCry >= 0x1f5)
			closeMangeursWindow();
	}
	_mgTick += dt;
	_mgBoil += dt;
	// The kettle boils 0x97 ticks after the fire is lit.
	if (_mgBoil >= 0x97 && s.isSet(kMgFireLit) && !s.isSet(kMgKettleTaken) && objectType("theieres") != 2) {
		showObj("vapeur");
		playSound("bouilloi", true);
		setObjectType("theieres", 2);
	}
	// Fire flicker (sub_41f0e0: u += 85) and steam (sub_41f130: u += 64), masked to the texture.
	const int fire = _model.findNode("fire");
	if (fire >= 0)
		_model.shiftNodeUV(fire, 85.0f, 0.0f, true);
	const int vapeur = _model.findNode("vapeur");
	if (vapeur >= 0)
		_model.shiftNodeUV(vapeur, 64.0f, 0.0f, true);
}

// ---------------------------------------------------------------------------
// Clicks.

namespace {

bool is(const char *a, const char *b) {
	return scumm_stricmp(a, b) == 0;
}

bool isRobotPart(const char *name) {
	for (const char *const *p = kRobotParts; *p; p++) {
		if (is(name, *p))
			return true;
	}
	return false;
}

}

void World::launchPage(const char *name) {
	const int page = Page::launcherPage(_scene, name);
	if (page >= 0)
		_pageRequest = page;
}

void World::stopSoundNamed(const char *name) {
	if (!g_engine || !g_engine->_sound)
		return;
	const uint16 h = g_engine->_sound->load(name);
	if (h)
		g_engine->_sound->stop(h);
}

void World::saveCheckpoint() {
	// sub_4011e0 -> sub_433580: the exe's GAME save after a placement.
	if (g_engine)
		g_engine->autosaveAfterPlacement(-1);
}

void World::startCarry(const char *name, int cursor) {
	// dat_54d864 = 1; dat_54dc00 = node; the node is hidden and the pointer becomes the object.
	hideObj(name);
	_carry = cursor;
	Common::strlcpy(_carryName, name, sizeof(_carryName));
}

/** A click while a scene object is carried (the "dat_54d864 == 1" branches of the scene ticks). */
void World::onCarryClick(const char *name) {
	GameState &s = _state;
	const int carry = _carry;
	char obj[sizeof(_carryName)];
	Common::strlcpy(obj, _carryName, sizeof(obj));
	_carry = kCarryNone;
	debug("Vangogh: carrying %s, click on '%s'", obj, name);

	bool used = false;
	switch (_scene) {
	case kSceneMangeurs:
		if (is(name, "fire") && carry == kCursorFagot) {
			// Kindling in the hearth.
			playSound("finfeu");
			s.set(kMgKindlingPlaced, 1);
			const int fire = _model.findNode("fire");
			if (fire >= 0)
				_model.shiftNodeUV(fire, 0.0f, 85.0f, false);	// sub_41f0b0
			setObjectType("buche", 4);
			saveCheckpoint();
			used = true;
		} else if (is(name, "fire") && carry == kCursorBuche) {
			// The log lights the fire; the kettle starts to boil.
			playSound("feu", true);
			s.set(kMgLogPlaced, 1);
			s.set(kMgFireLit, 1);
			_mgBoil = 0;
			saveCheckpoint();
			used = true;
		}
		break;
	case kSceneCafe:
		if (is(name, "barporte") && carry == kCursorKey) {
			playSound("serrure");
			startAnim(1);
			s.set(kCfDoorOpen, 1);
			saveCheckpoint();
			used = true;
		}
		break;
	case kScenePont:
		if (is(name, "pouli03") && carry == kCursorCrank) {
			showObj("poignee02");
			if (!s.isSet(kPoBridgeDown) && !_anims[0].active) {
				playSound("pontlev2");
				startAnim(0);
				saveCheckpoint();
			}
			s.set(kPoHandleMounted, 1);
			used = true;
		}
		break;
	default:
		break;
	}
	if (!used)
		showObj(obj);	// sub_43bc90: the object goes back where it was
}

/** What picking an object up changes besides its flag (the exe's per-object pickup branches). */
void World::pickupEffects(const char *name) {
	GameState &s = _state;
	switch (_scene) {
	case kSceneMusee:
		if (is(name, "etoile")) {
			// dat_4f96f8 = 1 (vase live), dat_4f9f78 = 0xa6 (the star anim resumes), vase type 3.
			s.set(kOfsVaseEnabled, 1);
			setObjectType("vase", 3);
			if (_anims[0].loaded) {
				_anims[0].elapsed = 0xa6;
				_anims[0].active = true;
			}
		}
		break;
	case kSceneChambre:
		if (is(name, "Object09"))
			setObjectType("TIROIR", 4);
		break;
	case kSceneCafe:
		if (is(name, "orloge")) {
			stopSoundNamed("pendule");
			stopSoundNamed("tictac");
		}
		break;
	case kSceneMangeurs:
		if (is(name, "theieres"))
			stopSoundNamed("bouilloi");
		break;
	case kSceneTerrasse:
		if (is(name, "platode")) {
			hideObj("de");
			setObjectType("platode", 255);
		}
		break;
	case kSceneChamp:
		if (is(name, "gun")) {
			playSound("pistolet");
			startAnim(0);
			playSound("corbeaux");
		}
		break;
	default:
		break;
	}
}

bool World::sceneClick(const char *name, int type, float dist) {
	GameState &s = _state;
	switch (_scene) {
	case kSceneMusee:
		if (is(name, "vase")) {
			if (s.isSet(kOfsVaseEnabled) && !s.puzzleSolved(0))
				launchPage(name);
			return true;
		}
		if (isRobotPart(name)) {
			if (!_timer[0] && !s.isSet(kOfsRobotOff)) {
				// The robot greets on demand: a50_01, then a50_01c.
				if (g_engine && g_engine->_sound) {
					_voice = g_engine->_sound->load("_a50_01");
					if (_voice)
						g_engine->_sound->play(_voice, false);
				}
				_timer[0] = 1;
				_timer[1] = 1;
				setTypes(255, kRobotParts);
			}
			return true;
		}
		return false;

	case kSceneAuberge:
		if (is(name, "saccoche") || is(name, "sac") || is(name, "sac01")) {
			if (!_anims[0].active && !s.isSet(kAuBagOpen)) {
				const char *const bag[] = { "saccoche", "sac", "sac01", nullptr };
				setTypes(255, bag);
				startAnim(0);
			}
			return true;
		}
		if (is(name, "portebas") && type == 6) {
			startAnim(2);
			setObjectType("portebas", 255);
			return true;
		}
		if (is(name, "portehaut") && type == 6) {
			// The exe clears the portebas type here, not portehaut's (asm 0x42183d); the stepper clears portehaut's.
			if (!_anims[1].active)
				startAnim(1);
			setObjectType("portebas", 255);
			return true;
		}
		return false;

	case kSceneHopiext:
		if (is(name, "cloche") || is(name, "korde1") || is(name, "korde2") || is(name, "korde3") || is(name, "korde4")) {
			if (!s.isSet(kHxGridOpen)) {
				startAnim(1);
				playSound("cloche");
				s.set(kHxGridOpen, 1);
			}
			return true;
		}
		if (is(name, "grilledrt") || is(name, "grillegche")) {
			if (!s.isSet(kHxGridOpen)) {
				startAnim(0);
				s.set(kHxGridOpen, 1);
			}
			return true;
		}
		if (is(name, "arbre11")) {
			if (dist < 1500.0f)
				launchPage(name);
			return true;
		}
		return false;

	case kSceneHopiint:
		if (is(name, "ARMOIRE")) {
			if (!s.isSet(kHiWardrobeOpen) && !_anims[0].active) {
				playSound("armoire");
				startAnim(0);
			}
			return true;
		}
		return false;

	case kSceneMaisonet:
		if (is(name, "pelle")) {
			if (!s.isSet(kMeDigDone) && !_anims[1].active) {
				playSound("pelle");
				startAnim(1);
				showObj("terre");
			}
			return true;
		}
		if (is(name, "terre") || is(name, "plume")) {
			pickUpObject(name);
			return true;
		}
		return false;

	case kSceneMangeurs:
		if (is(name, "fagot")) {
			if (type != 255)
				startCarry("fagot", kCursorFagot);
			return true;
		}
		if (is(name, "buche")) {
			if (s.isSet(kMgKindlingPlaced))
				startCarry("buche", kCursorBuche);
			return true;
		}
		if (is(name, "fenetre")) {
			if (type != 255 && !_mgWindowRt)
				closeMangeursWindow();
			return true;
		}
		if (is(name, "portepoel")) {
			if (!s.isSet(kMgStoveOpen)) {
				startAnim(4);
				s.set(kMgStoveOpen, 1);
				playSound("porpoel");
			}
			return true;
		}
		return false;

	case kSceneCafe:
		if (is(name, "ke")) {
			if (!_billiardRt && !_anims[0].active && !s.isSet(kCfBilliardDone)) {
				playSound("billiard");
				startAnim(0);
			}
			return true;
		}
		if (is(name, "clef")) {
			startCarry("clef", kCursorKey);
			return true;
		}
		return false;

	case kSceneChambre:
		if (!s.get(kCbVariant)) {
			if (is(name, "porte")) {
				if (!s.isSet(kCbVDoorOpen) && !_anims[0].active) {
					playSound("placard");
					startAnim(0);
				}
				return true;
			}
			return false;
		}
		if (is(name, "CHAISE1")) {
			if (!s.isSet(kCbChairMoved) && !_anims[0].active) {
				playSound("chaiseVG");
				startAnim(0);
			}
			return true;
		}
		if (is(name, "porte")) {
			if (s.isSet(kCbChairMoved) && !s.isSet(kCbWardrobeOpen) && !_anims[4].active) {
				playSound("placard");
				startAnim(4);
			}
			return true;
		}
		if (is(name, "TIROIR")) {
			if (s.isSet(kCbDrawerOpen))
				launchPage(name);
			else if (s.isSet(kCbObject09Taken) && !_anims[5].active) {
				playSound("tiroir");
				startAnim(5);
			}
			return true;
		}
		if (is(name, "papierferm")) {
			if (!s.isSet(kCbPaperOpen) && !_anims[3].active) {
				startAnim(3);
				if (g_engine && g_engine->_sound) {
					_voice = g_engine->_sound->load("_sm03_012");
					if (_voice)
						g_engine->_sound->play(_voice, false);
				}
			}
			return true;
		}
		if (is(name, "shoes") || is(name, "shoes01") || is(name, "Object06") || is(name, "Object10")) {
			if (s.isSet(kCbShoesEnabled))
				pickUpObject("shoes");
			return true;
		}
		return false;

	case kSceneMaisonj:
		if (is(name, "pot01")) {
			if (!s.isSet(kMjRainSeen)) {
				s.set(kMjSnailReleased, 1);
				showObj("escargot");
				s.set(kMjRainSeen, 1);
				startAnim(0);
				setObjectType("pot02", 4);
			}
			if (!s.isSet(kMjYellowDone)) {
				// The rain: a movie, then the same world again (the exe's mode 2, dat_5e3e30 = 2).
				const Pose3D here = pose();
				if (playClip("pluie", kSceneMaisonj, _prevScene)) {
					_clipKeepPose = true;
					_clipPose = here;
				}
			}
			return true;
		}
		if (is(name, "pot02")) {
			if (s.isSet(kMjRainSeen)) {
				yellowHouse();
				s.set(kMjYellowDone, 1);
				s.set(kCbVariant, 1);
				setObjectType("pot01", 255);
				setObjectType("pot02", 255);
			}
			return true;
		}
		if (is(name, "portail") || is(name, "portail01")) {
			if (!s.isSet(kMjGateOpen) && !_anims[2].active) {
				playSound("portail01");
				startAnim(2);
			}
			return true;
		}
		if (is(name, "portemj")) {
			if (!s.isSet(kMjDoorOpen) && !_anims[3].active) {
				playSound("placard");
				startAnim(3);
			}
			return true;
		}
		if (is(name, "escargot")) {
			pickUpObject(name);
			return true;
		}
		return false;

	case kScenePont:
		if (is(name, "trappe")) {
			if (!s.isSet(kPoTrapOpen) && !_anims[1].active) {
				playSound("trappe");
				startAnim(1);
			}
			return true;
		}
		if (is(name, "pouli02")) {
			if (!_anims[0].active && !s.isSet(kPoBridgeDown)) {
				// sub_41d590 ShowHandleMounted: the crank is not needed.
				showObj("poignee02");
				hideObj("poignee04");
				setObjectType("pouli03", 255);
				setObjectType("pouli02", 255);
				s.set(kPoHandleMounted, 1);
				playSound("pontlev2");
				startAnim(0);
			}
			return true;
		}
		if (is(name, "poignee04")) {
			startCarry("poignee04", kCursorCrank);
			return true;
		}
		return false;

	case kSceneTerrasse:
		if (is(name, "MANIVELLE")) {
			if (!s.isSet(kTeStoreUp) && !_anims[0].active) {
				startAnim(0);
				playSound("store");
			}
			return true;
		}
		if (is(name, "drapo")) {
			if (s.isSet(kTeStoreUp))
				launchPage(name);
			return true;
		}
		return false;

	case kSceneJardin:
		if (is(name, "cerf")) {
			if (!s.isSet(kJaKiteFlown)) {
				startAnim(1);
				playSound("clochjar");
				s.set(kJaKiteFlown, 1);
				s.set(kEgKiteAway, 0);
			}
			return true;
		}
		if (is(name, "barriere")) {
			if (type == 4 && !_anims[0].active) {
				startAnim(0);
				playSound("barriere");
			}
			return true;
		}
		return false;

	case kSceneChamp:
		if (is(name, "faux")) {
			if (!s.isSet(kChScytheDone) && !_anims[1].active) {
				startAnim(1);
				playSound("faux");
			}
			return true;
		}
		if (is(name, "gun")) {
			pickUpObject(name);
			return true;
		}
		return false;

	default:
		return false;
	}
}

// ---------------------------------------------------------------------------
// Arrival and departure clips.

bool World::arrivalClip(int scene, char *name, size_t size) const {
	if (scene < 0 || scene >= kNumScenes || !kArrivalClips[scene])
		return false;
	// sub_427290: the sequence plays only while the world's puzzles are not all solved.
	if (_state.sceneSolved(scene))
		return false;
	Common::strlcpy(name, kArrivalClips[scene], size);
	return true;
}

bool World::playClip(const char *name, int thenScene, int thenPrev) {
	if (g_engine && g_engine->_sound)
		g_engine->_sound->stopAll();
	if (!_clip.open(name)) {
		warning("Vangogh: could not play clip %s", name);
		return false;
	}
	_clipActive = true;
	_clipKeepPose = false;
	_clipScene = thenScene;
	_clipPrev = thenPrev;
	_hoverObject = -1;
	_hoverName[0] = '\0';
	_cursorKind = kCursorArrow;
	return true;
}

void World::stepClip(const WorldInput &in) {
	if (in.click || _clip.done()) {
		endClip();
		return;
	}
	_clip.step();
}

void World::endClip() {
	_clip.close();
	_clipActive = false;
	changeScene(_clipScene, _clipPrev);
	if (_clipKeepPose) {
		_clipKeepPose = false;
		setPose(_clipPose);
	}
}

void World::startPaintingClick(int dest) {
	// sub_426b40: any painting click ends the robot's intro; the first click into a wing starts that act (exclusive curAct marker).
	GameState &s = _state;
	s.set(kOfsRobotOff, 1);
	const int wing = dest <= kSceneMangeurs ? 0 : (dest <= kSceneTerrasse ? 1 : 2);
	const int entered = kOfsActEntered1 + wing * 4;
	if (!s.isSet(entered)) {
		s.set(kOfsCurAct1, wing == 0);
		s.set(kOfsCurAct2, wing == 1);
		s.set(kOfsCurAct3, wing == 2);
		s.set(entered, 1);
	}
	startGlide(dest);
}

void World::leaveToMuseum() {
	// sub_426680: the "r" sequence plays only for a solved world, and marks its page as departed.
	const char *clip = (_scene >= 0 && _scene < kNumScenes) ? kDepartureClips[_scene] : nullptr;
	if (clip && _state.sceneSolved(_scene)) {
		const byte page = _state.getByte(kOfsPage);
		if (page < GameState::kNumPuzzles)
			_state.set(kOfsDeparted + page * 4, 1);
		if (playClip(clip, kSceneMusee, _scene))
			return;
	}
	changeScene(kSceneMusee, _scene);
}

} // End of namespace Vangogh
