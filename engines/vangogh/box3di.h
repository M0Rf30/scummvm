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

#ifndef VANGOGH_BOX3DI_H
#define VANGOGH_BOX3DI_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

namespace Vangogh {

class BFGFile;

/**
 * One `.3DI` collision/ground volume (`BOX.3DI`, `BOX1.3DI`, ...): a soup of
 * one-sided triangles with precomputed plane, edge planes and bounding box.
 * Format reference: PEINTRE.exe sub_43a7c0 (relocator) and
 * sub_425300 / sub_440fa0 (consumers), vangogh-re notes/world.md sec.2.
 *
 * Record body (little-endian dwords; every "pointer" is stored as
 * `bodyOffset + 1`):
 *   [0] vertex count, [1] -> int32[3] vertices, [2] triangle count,
 *   [3] -> 0x60-byte triangles, [4] normal count, [5] -> int32[3] normals.
 * Triangle (0x60 bytes): +0x00/+4/+8 vertex pointers, +0x0c normal pointer
 * (1.15 fixed point), +0x10 plane offset d (n.p >> 15 == d on the plane),
 * +0x14.. three edge normals (1.15) and +0x38.. three edge offsets (a point is
 * inside edge i when edgeNormal.p >> 15 - edgeD >= 0), +0x48 AABB min,
 * +0x54 AABB max.
 */
class CollisionMesh {
public:
	struct Tri {
		int32 v[3][3];
		int32 n[3];
		int32 d;
		int32 edge[3][3];
		int32 edgeD[3];
		int32 bmin[3];
		int32 bmax[3];
	};

	/** Loads the 3DI record @p recordName (e.g. "BOX1.3DI") out of @p bfg; false if missing or malformed. */
	bool load(const BFGFile &bfg, const char *recordName);
	void clear() { _tris.clear(); }

	uint size() const { return _tris.size(); }
	const Tri &tri(uint i) const { return _tris[i]; }

private:
	Common::Array<Tri> _tris;
};

/**
 * The exe's collision world (0x6f7700): exactly one 3DI mesh is active at a
 * time (SetBox(n) removes the current box and adds the new one) and is used
 * for two things: the camera ground-follow ray (sub_42ac60) and the camera
 * sphere push-out (sub_425c00 / sub_4256b0). All maths is the exe's integer /
 * truncating-float maths; nothing here allocates per call.
 */
class CollisionWorld {
public:
	CollisionWorld() : _mesh(nullptr) {}

	/** Makes @p mesh (may be null) the active volume; the mesh must outlive its activation. */
	void setActive(const CollisionMesh *mesh);
	const CollisionMesh *active() const { return _mesh; }

	/**
	 * sub_42ac60 ground probe: casts the segment from @p cam to
	 * (x+1, y+1000+@p probeOffset, z+1) against the active triangles and, on
	 * the nearest hit, returns true with @p outY = hitY - 650.
	 */
	bool groundY(const int32 cam[3], int32 probeOffset, int32 &outY) const;

	/**
	 * sub_425c00: sweeps a sphere of @p radius from @p prev (updated in place)
	 * towards @p cur in steps of at most radius/2, pushing it out of the
	 * triangles it touches. Returns the number of corrections; if non-zero
	 * @p prev holds the corrected centre (the new camera position). As in the
	 * exe, @p prev is left one step beyond @p cur when nothing was hit.
	 */
	int moveSphere(const int32 cur[3], int32 prev[3], int32 radius);

private:
	int pushOut(int32 pos[3], int32 radius);

	const CollisionMesh *_mesh;
	Common::Array<uint> _cand;	///< broad-phase candidate triangle indices, preallocated to the mesh size
};

} // End of namespace Vangogh

#endif // VANGOGH_BOX3DI_H
