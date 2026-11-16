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

#include "vangogh/box3di.h"
#include "vangogh/bfg.h"

#include "common/endian.h"
#include "common/debug.h"
#include "common/textconsole.h"
#include "common/util.h"

#include <math.h>

namespace Vangogh {

namespace {

// Decompressed record = 0x14-byte runtime header (type at +8) + body (see model3d.cpp).
const uint32 kRecHeader = 0x14;
const uint32 kRecTypeOffset = 8;
const uint32 kRecType3DI = 5;
const uint32 kTriSize = 0x60;
const uint32 kMaxTris = 65536;

// Ground-follow constants (sub_42ac60): the eye sits 0x28a above the floor hit.
const int32 kEyeHeight = 0x28a;
const int32 kProbeBase = 1000;

/** (a * b) >> 15 truncating toward zero, the exe's `(p + (p >> 31 & 0x7fff)) >> 15`. */
inline int32 mul15(int32 a, int32 b) {
	return (int32)(((int64)a * (int64)b) / 32768);
}

inline int32 dot15(const int32 a[3], const int32 p[3]) {
	return mul15(a[0], p[0]) + mul15(a[1], p[1]) + mul15(a[2], p[2]);
}

inline int32 planeDist(const CollisionMesh::Tri &t, const int32 p[3]) {	// signed distance, positive on the normal side
	return dot15(t.n, p) - t.d;
}

inline int32 edgeDist(const CollisionMesh::Tri &t, int i, const int32 p[3]) {	// sub_43f130, positive inside
	return dot15(t.edge[i], p) - t.edgeD[i];
}

/** sub_401352: integer distance, truncated. */
int32 dist3d(const int32 a[3], const int32 b[3]) {
	const double dx = (double)a[0] - b[0], dy = (double)a[1] - b[1], dz = (double)a[2] - b[2];
	return (int32)sqrt(dx * dx + dy * dy + dz * dz);
}

/** sub_43f760: closest point to @p p on segment A-B, truncated to integers. */
void closestOnSegment(const int32 p[3], const int32 A[3], const int32 B[3], int32 out[3]) {
	const double ux = (double)B[0] - A[0], uy = (double)B[1] - A[1], uz = (double)B[2] - A[2];
	const double len2 = ux * ux + uy * uy + uz * uz;
	double t = 0.0;
	if (len2 > 0.0)
		t = CLIP(((p[0] - (double)A[0]) * ux + (p[1] - (double)A[1]) * uy + (p[2] - (double)A[2]) * uz) / len2, 0.0, 1.0);
	out[0] = (int32)(A[0] + t * ux);
	out[1] = (int32)(A[1] + t * uy);
	out[2] = (int32)(A[2] + t * uz);
}

/** sub_43f350: orthogonal projection of @p p onto the triangle's plane (through vertex 0). */
void projectToPlane(const CollisionMesh::Tri &t, const int32 p[3], int32 out[3]) {
	const double n0 = t.n[0], n1 = t.n[1], n2 = t.n[2];
	const double nn = n0 * n0 + n1 * n1 + n2 * n2;
	if (nn == 0.0) {
		out[0] = p[0];
		out[1] = p[1];
		out[2] = p[2];
		return;
	}
	const double k = (n0 * ((double)t.v[0][0] - p[0]) + n1 * ((double)t.v[0][1] - p[1]) + n2 * ((double)t.v[0][2] - p[2])) / nn;
	out[0] = (int32)(p[0] + n0 * k);
	out[1] = (int32)(p[1] + n1 * k);
	out[2] = (int32)(p[2] + n2 * k);
}

/**
 * sub_425300 with the world's 0xf flags: sphere (@p p, @p r) against one
 * triangle. Returns +-2 for a contact inside the face prism (@p out = the
 * projection onto the plane), +-1 for an edge/vertex contact within r
 * (@p out = the closest point) and 0 for none; the sign is the side of the
 * plane the centre is on (positive = front).
 */
int sphereTri(const CollisionMesh::Tri &t, const int32 p[3], int32 r, int32 out[3]) {
	const int32 dist = planeDist(t, p);
	if (!(-r < dist && dist < r))
		return 0;

	const int sign = (dist >= 0) ? 1 : -1;
	uint bits = 0;
	for (int i = 0; i < 3; i++) {
		const int32 e = edgeDist(t, i, p);
		if (e < -r)
			return 0;
		if (e <= -1)
			bits |= 1u << i;
	}

	switch (bits) {
	case 0:
		projectToPlane(t, p, out);
		return sign * 2;
	case 1:
		closestOnSegment(p, t.v[0], t.v[1], out);
		break;
	case 2:
		closestOnSegment(p, t.v[1], t.v[2], out);
		break;
	case 3:
		memcpy(out, t.v[1], sizeof(int32) * 3);
		break;
	case 4:
		closestOnSegment(p, t.v[2], t.v[0], out);
		break;
	case 5:
		memcpy(out, t.v[0], sizeof(int32) * 3);
		break;
	case 6:
		memcpy(out, t.v[2], sizeof(int32) * 3);
		break;
	default:
		return 0;
	}

	const int64 dx = (int64)out[0] - p[0], dy = (int64)out[1] - p[1], dz = (int64)out[2] - p[2];
	return (dx * dx + dy * dy + dz * dz < (int64)r * r) ? sign : 0;
}

} // end of anonymous namespace

bool CollisionMesh::load(const BFGFile &bfg, const char *recordName) {
	_tris.clear();

	Common::Array<byte> rec;
	if (!bfg.getRecord(recordName, rec))
		return false;
	if (rec.size() < kRecHeader + 24 || READ_LE_UINT32(&rec[kRecTypeOffset]) != kRecType3DI) {
		warning("Vangogh: %s is not a 3DI record", recordName);
		return false;
	}

	const byte *body = &rec[kRecHeader];
	const uint32 bodySize = rec.size() - kRecHeader;
	const uint32 numTris = READ_LE_UINT32(body + 8);
	const uint32 triBase = READ_LE_UINT32(body + 12) - 1;
	if (numTris == 0 || numTris > kMaxTris || triBase > bodySize || (bodySize - triBase) / kTriSize < numTris) {
		warning("Vangogh: %s: bad triangle table (%u at 0x%x)", recordName, numTris, triBase);
		return false;
	}

	// Reads the int32[3] a stored pointer refers to.
	auto readVec = [&](uint32 stored, int32 out[3]) -> bool {
		const uint32 off = stored - 1;
		if (stored == 0 || off > bodySize || bodySize - off < 12)
			return false;
		for (int k = 0; k < 3; k++)
			out[k] = (int32)READ_LE_UINT32(body + off + k * 4);
		return true;
	};

	_tris.resize(numTris);
	for (uint32 i = 0; i < numTris; i++) {
		const byte *rp = body + triBase + i * kTriSize;
		Tri &t = _tris[i];
		const bool ok = readVec(READ_LE_UINT32(rp + 0x00), t.v[0]) && readVec(READ_LE_UINT32(rp + 0x04), t.v[1]) &&
			readVec(READ_LE_UINT32(rp + 0x08), t.v[2]) && readVec(READ_LE_UINT32(rp + 0x0c), t.n);
		if (!ok) {
			warning("Vangogh: %s: triangle %u has a bad pointer", recordName, i);
			_tris.clear();
			return false;
		}
		t.d = (int32)READ_LE_UINT32(rp + 0x10);
		for (int k = 0; k < 3; k++) {
			for (int c = 0; c < 3; c++)
				t.edge[k][c] = (int32)READ_LE_UINT32(rp + 0x14 + k * 12 + c * 4);
			t.edgeD[k] = (int32)READ_LE_UINT32(rp + 0x38 + k * 4);
			t.bmin[k] = (int32)READ_LE_UINT32(rp + 0x48 + k * 4);
			t.bmax[k] = (int32)READ_LE_UINT32(rp + 0x54 + k * 4);
		}
	}

	debug("Vangogh: %s: %u collision triangles", recordName, numTris);
	return true;
}

void CollisionWorld::setActive(const CollisionMesh *mesh) {
	_mesh = mesh;
	_cand.clear();
	if (mesh)
		_cand.resize(mesh->size());
}

bool CollisionWorld::groundY(const int32 cam[3], int32 probeOffset, int32 &outY) const {
	if (!_mesh)
		return false;

	const int32 B[3] = { cam[0] + 1, cam[1] + kProbeBase + probeOffset, cam[2] + 1 };
	int32 lo[3], hi[3];
	for (int k = 0; k < 3; k++) {
		lo[k] = MIN(cam[k], B[k]);
		hi[k] = MAX(cam[k], B[k]);
	}

	int32 best = 0x40000000;
	int32 bestY = 0;
	for (uint i = 0; i < _mesh->size(); i++) {
		const CollisionMesh::Tri &t = _mesh->tri(i);
		if (t.bmin[0] > hi[0] || t.bmax[0] < lo[0] || t.bmin[1] > hi[1] || t.bmax[1] < lo[1] || t.bmin[2] > hi[2] || t.bmax[2] < lo[2])
			continue;

		// sub_440fa0: the segment must cross the plane...
		const int sA = planeDist(t, cam) >= 0 ? 1 : -1;
		const int sB = planeDist(t, B) >= 0 ? 1 : -1;
		if (sA * sB >= 0)
			continue;

		// ...at a point inside all three edge planes (sub_43f4f0 intersection, sub_43f130 edge tests).
		const double dx = (double)B[0] - cam[0], dy = (double)B[1] - cam[1], dz = (double)B[2] - cam[2];
		const double denom = (double)t.n[0] * dx + (double)t.n[1] * dy + (double)t.n[2] * dz;
		if (denom == 0.0)
			continue;
		const double k = ((double)t.n[0] * ((double)t.v[0][0] - cam[0]) + (double)t.n[1] * ((double)t.v[0][1] - cam[1]) +
			(double)t.n[2] * ((double)t.v[0][2] - cam[2])) / denom;
		const int32 hit[3] = { (int32)(cam[0] + k * dx), (int32)(cam[1] + k * dy), (int32)(cam[2] + k * dz) };
		if (edgeDist(t, 0, hit) < 0 || edgeDist(t, 1, hit) < 0 || edgeDist(t, 2, hit) < 0)
			continue;

		const int32 d = dist3d(cam, hit);
		if (d < best) {
			best = d;
			bestY = hit[1];
		}
	}

	if (best >= 0x40000000)
		return false;
	outY = bestY - kEyeHeight;
	return true;
}

int CollisionWorld::pushOut(int32 pos[3], int32 r) {
	if (!_mesh)
		return 0;

	// Broad phase (sub_4408e0): triangles whose AABB overlaps the sphere's AABB.
	uint numCand = 0;
	for (uint i = 0; i < _mesh->size(); i++) {
		const CollisionMesh::Tri &t = _mesh->tri(i);
		bool overlap = true;
		for (int k = 0; k < 3 && overlap; k++)
			overlap = t.bmax[k] >= pos[k] - r && t.bmin[k] <= pos[k] + r;
		if (overlap)
			_cand[numCand++] = i;
	}
	if (!numCand)
		return 0;

	int corrections = 0;
	int32 contact[3];

	// Pass 1: face contacts on the front side push the centre to r * 1.05 along the normal.
	const int32 faceRadius = (int32)((double)r * (double)1.05f);
	for (uint c = 0; c < numCand; c++) {
		const CollisionMesh::Tri &t = _mesh->tri(_cand[c]);
		if (sphereTri(t, pos, r, contact) == 2 && (contact[0] || contact[1] || contact[2])) {
			for (int k = 0; k < 3; k++)
				pos[k] = contact[k] + (int32)(((int64)t.n[k] * faceRadius) >> 15);
			corrections++;
		}
	}

	// Pass 2: edge/vertex contacts push the centre radially out to exactly r.
	for (uint c = 0; c < numCand; c++) {
		const CollisionMesh::Tri &t = _mesh->tri(_cand[c]);
		if (sphereTri(t, pos, r, contact) != 1)
			continue;

		float d[3], p[3];
		for (int k = 0; k < 3; k++) {
			p[k] = (float)pos[k];
			d[k] = p[k] - (float)contact[k];
		}
		const float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
		if (len >= 0.01f) {
			const float s = (float)r / len;
			for (int k = 0; k < 3; k++)
				p[k] = d[k] * s + (float)contact[k];
		}
		for (int k = 0; k < 3; k++)
			pos[k] = (int32)p[k];
		corrections++;
	}
	return corrections;
}

int CollisionWorld::moveSphere(const int32 cur[3], int32 prev[3], int32 radius) {
	// sub_425c00: n = 1 below radius/2 of travel, else 2*dist/radius + 1.
	const int32 dist = dist3d(cur, prev);
	int32 n = 1;
	if (dist >= radius / 2)
		n = ((dist * 2 >= radius) ? (dist * 2) / radius : 0) + 1;

	int32 step[3];
	for (int k = 0; k < 3; k++) {
		step[k] = (cur[k] - prev[k]) / n;
		prev[k] += step[k];
	}

	while (n > 0) {
		const int hit = pushOut(prev, radius);
		if (hit)
			return hit;
		for (int k = 0; k < 3; k++)
			prev[k] += step[k];
		n--;
	}
	return 0;
}

} // End of namespace Vangogh
