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


#ifndef VANGOGH_ANIM3DA_H
#define VANGOGH_ANIM3DA_H

#include "common/array.h"
#include "common/scummsys.h"

namespace Vangogh {

class BFGFile;
class Model3D;

/**
 * A `.3DA` node animation: one keyframe channel per node of the scene's
 * `.3DC`, evaluated exactly like the exe's apply() (sub_438800 -> sub_438310,
 * anim.md sec.1.3): time is an integer tick, key lookup is a binary search,
 * position keys blend linearly with an 8-bit weight and rotation keys with the
 * quaternion slerp of sub_442180, and a channel with fewer than two keys of a
 * kind leaves that component at the 3DC rest pose.
 *
 * Channel i drives node i of the model (the mapping is positional). A clip may
 * carry fewer channels than the model has nodes (jardin papiyon3, maisonj
 * nuages, mangeurs chaise); the exe would read past its channel table there, so
 * the surplus nodes are simply not animated.
 */
class Anim3DA {
public:
	/** Loads record @p recordName (e.g. "billiard.3DA") out of @p bfg; false if missing or malformed. */
	bool load(const BFGFile &bfg, const char *recordName);
	void clear();

	bool loaded() const { return !_channels.empty(); }
	/** Clip length in ticks (ENTRY.nFrames, sub_438300). */
	uint32 duration() const { return _duration; }
	uint numChannels() const { return _channels.size(); }
	/** Key counts of channel @p i; false if out of range. */
	bool channelKeys(uint i, uint &numRot, uint &numPos) const;
	/** Channels that actually move a node (at least two rotation or position keys). */
	uint numAnimated() const;

	/** Poses every animated node of @p model at tick @p t (clamped to the key range). */
	void apply(Model3D &model, int t) const;

	/**
	 * Checks channel i's first keys against the model's rest pose (the cross-check
	 * of anim.md sec.1.2): returns the largest |position delta| and, in
	 * @p rotDelta, the largest |matrix element delta| (0 when the channel has no such keys).
	 */
	int restPoseError(const Model3D &model, uint channel, float &rotDelta) const;

private:
	struct RotKey {
		int32 time;
		int32 q[4];	///< x, y, z, w, 1.15 fixed point
	};
	struct PosKey {
		int32 time;
		int32 p[3];
	};
	struct Channel {
		uint32 firstRot, numRot;
		uint32 firstPos, numPos;
	};

	static void quatToMatrix(const float q[4], float m[9]);
	void evalRot(const Channel &c, int t, float m[9]) const;
	void evalPos(const Channel &c, int t, int32 p[3]) const;

	Common::Array<Channel> _channels;
	Common::Array<RotKey> _rot;
	Common::Array<PosKey> _pos;
	uint32 _duration = 0;
};

} // End of namespace Vangogh

#endif // VANGOGH_ANIM3DA_H
