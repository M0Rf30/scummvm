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

#include "common/endian.h"
#include "common/util.h"

#include <math.h>

namespace Vangogh {

namespace {

inline int32 boxComponent(const int32 *arr, uint32 start, uint32 corner, int axis) {
	return arr[start + 3 * corner + axis];
}

inline bool isSentinel(int32 v) {
	// -32768/32767 (i16 min/max) and one-past-both -- the raw data is
	// otherwise plausible room-scale integers, so a value landing exactly
	// on an i16 boundary reads as "unset"/invalid, not real geometry. See
	// mesh_3dm_parser.py's find_oriented_boxes().
	return v == -32768 || v == -32767 || v == 32767 || v == 32768;
}

// Sorts 4 (x,y) pairs counter-clockwise around their centroid, stably --
// mirrors mesh_3dm_parser.py's _sort_ccw() (Python's sorted() is stable;
// real coordinate data is never expected to tie on angle, but a hand-rolled
// insertion sort over exactly 4 elements keeps that guarantee for free
// without pulling in <algorithm>).
void sortCCW(const int32 x[4], const int32 y[4], int order[4]) {
	double cx = 0.0, cy = 0.0;
	for (int k = 0; k < 4; k++) {
		cx += x[k];
		cy += y[k];
		order[k] = k;
	}
	cx /= 4.0;
	cy /= 4.0;

	double angle[4];
	for (int k = 0; k < 4; k++)
		angle[k] = atan2((double)y[k] - cy, (double)x[k] - cx);

	for (int a = 1; a < 4; a++) {
		const int key = order[a];
		const double keyAngle = angle[key];
		int b = a - 1;
		while (b >= 0 && angle[order[b]] > keyAngle) {
			order[b + 1] = order[b];
			b--;
		}
		order[b + 1] = key;
	}
}

/**
 * Tests whether an oriented-hexahedron prism starts at i32 index @p start.
 * See findOrientedBoxes() in box3di.h for the full check list; this is a
 * field-for-field/check-for-check port of mesh_3dm_parser.py's
 * find_oriented_boxes()'s inner corners_at(), including its constants.
 */
bool tryBoxAt(const int32 *arr, uint32 start, int tol, HotspotBox &out) {
	for (int hAxis = 0; hAxis < 3; hAxis++) {
		int other[2];
		int oi = 0;
		for (int a = 0; a < 3; a++)
			if (a != hAxis)
				other[oi++] = a;

		int32 bh[4], th[4];
		for (int k = 0; k < 4; k++) {
			bh[k] = boxComponent(arr, start, k, hAxis);
			th[k] = boxComponent(arr, start, 4 + k, hAxis);
		}

		int32 bhMin = bh[0], bhMax = bh[0], thMin = th[0], thMax = th[0];
		for (int k = 1; k < 4; k++) {
			bhMin = MIN(bhMin, bh[k]);
			bhMax = MAX(bhMax, bh[k]);
			thMin = MIN(thMin, th[k]);
			thMax = MAX(thMax, th[k]);
		}
		if (bhMax - bhMin > tol || thMax - thMin > tol)
			continue;

		int32 bx[4], by[4];
		for (int k = 0; k < 4; k++) {
			bx[k] = boxComponent(arr, start, k, other[0]);
			by[k] = boxComponent(arr, start, k, other[1]);
		}
		int order[4];
		sortCCW(bx, by, order);
		int32 sbx[4], sby[4];
		for (int k = 0; k < 4; k++) {
			sbx[k] = bx[order[k]];
			sby[k] = by[order[k]];
		}

		int32 tx[4], ty[4];
		for (int k = 0; k < 4; k++) {
			tx[k] = boxComponent(arr, start, 4 + k, other[0]);
			ty[k] = boxComponent(arr, start, 4 + k, other[1]);
		}

		// Greedy nearest-unused pairwise match between the (sorted) bottom
		// footprint and the (raw-order) top footprint -- NOT a positional
		// comparison, since the file stores each face's corners in
		// raster/grid order, not walk order (see 3dc-format.md sec.3.2).
		bool used[4] = {false, false, false, false};
		bool ok = true;
		for (int k = 0; k < 4 && ok; k++) {
			int match = -1;
			for (int j = 0; j < 4; j++) {
				if (used[j])
					continue;
				if (ABS(sbx[k] - tx[j]) <= tol && ABS(sby[k] - ty[j]) <= tol) {
					match = j;
					break;
				}
			}
			if (match < 0)
				ok = false;
			else
				used[match] = true;
		}
		if (!ok)
			continue;

		const double h0 = (bh[0] + bh[1] + bh[2] + bh[3]) / 4.0;
		const double h1 = (th[0] + th[1] + th[2] + th[3]) / 4.0;
		const double bottomHeight = MIN(h0, h1);
		const double topHeight = MAX(h0, h1);
		if (topHeight - bottomHeight < 50)
			continue;

		int64 area2 = 0;
		for (int k = 0; k < 4; k++) {
			const int kk = (k + 1) % 4;
			area2 += (int64)sbx[k] * sby[kk] - (int64)sbx[kk] * sby[k];
		}
		if (ABS(area2) < 5000)
			continue;

		bool sentinel = false;
		for (int k = 0; k < 4 && !sentinel; k++) {
			if (isSentinel(bh[k]) || isSentinel(th[k]) || isSentinel(sbx[k]) || isSentinel(sby[k]))
				sentinel = true;
		}
		if (sentinel)
			continue;

		out.heightAxis = hAxis;
		out.otherAxes[0] = other[0];
		out.otherAxes[1] = other[1];
		out.bottomHeight = bottomHeight;
		out.topHeight = topHeight;
		// box_to_vertices(): both faces reuse the SAME (bottom) footprint
		// order, only the height differs -- the top face's own raw corner
		// order was only needed to confirm the match above.
		for (int k = 0; k < 4; k++) {
			out.vertices[k][hAxis] = bottomHeight;
			out.vertices[k][other[0]] = sbx[k];
			out.vertices[k][other[1]] = sby[k];
			out.vertices[4 + k][hAxis] = topHeight;
			out.vertices[4 + k][other[0]] = sbx[k];
			out.vertices[4 + k][other[1]] = sby[k];
		}
		return true;
	}
	return false;
}

// --------------------------------------------------------------------------
// Camera/projection math -- field-for-field port of scene_hotspots.py's
// bam_to_rad()/_rot_x()/_rot_y()/_matmul()/_matvec()/project().
// --------------------------------------------------------------------------
inline double bamToRadians(int32 v) {
	return ((v & 0xFFF) / 4096.0) * 2.0 * M_PI;
}

typedef double Mat3[3][3];

void rotX(double a, Mat3 out) {
	const double c = cos(a), s = sin(a);
	out[0][0] = 1; out[0][1] = 0; out[0][2] = 0;
	out[1][0] = 0; out[1][1] = c; out[1][2] = -s;
	out[2][0] = 0; out[2][1] = s; out[2][2] = c;
}

void rotY(double a, Mat3 out) {
	const double c = cos(a), s = sin(a);
	out[0][0] = c;  out[0][1] = 0; out[0][2] = s;
	out[1][0] = 0;  out[1][1] = 1; out[1][2] = 0;
	out[2][0] = -s; out[2][1] = 0; out[2][2] = c;
}

void matMul(const Mat3 a, const Mat3 b, Mat3 out) {
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			double sum = 0.0;
			for (int k = 0; k < 3; k++)
				sum += a[i][k] * b[k][j];
			out[i][j] = sum;
		}
	}
}

void matVec3(const Mat3 m, const double v[3], double out[3]) {
	for (int i = 0; i < 3; i++)
		out[i] = m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2];
}

struct NamedCameraPose {
	const char *name;
	CameraPose pose;
};

// Literal per-scene default camera poses transcribed from PEINTRE.exe's
// fcn.00427600 (scene-flow.md sec.3.3/sec.6 -- the "default" fallback pose
// per destination scene, NOT the per-from/to-edge override poses also
// present in that function). Field-for-field copy of scene_hotspots.py's
// CAMERA_POSES table. 12 of 14 scenes recovered; auberge/hopiext are
// deliberately absent -- see cameraPoseForScene()'s doc comment.
const NamedCameraPose kCameraPoses[] = {
	{ "musee",     { 4524,  -297, 4758, 4006, 1791 } },
	{ "maisonet",  { 1149,   141,-1491,    0,  275 } },
	{ "mangeurs",  { -106,     6,  100, 3946,  145 } },
	{ "cafe",      {  393,  -175, -136, 3976,   92 } },
	{ "chambreb",  { -628,  -336, -268, 3946,  150 } },
	{ "maisonj",   {-3350,   489,-5996,   60, 3944 } },
	{ "hopiint",   {  -67,   -78, -345, 4036,   17 } },
	{ "pont",      { 7805,  1167, 2183,   30, 3265 } },
	{ "terrasse",  { -123,  -102, -846, 4066,   31 } },
	{ "jardin",    {  -46,  -335, 1263,    0, 4052 } },
	{ "champ",     { 2140,   253, 2270, 4006,  214 } },
	{ "eglise",    {-7189,   802,  102,  210, 3756 } },
};

} // end of anonymous namespace

const ZoomViewport kZoomViewports[4] = {
	{ 640, 480,   0,   0 },
	{ 512, 384,  64,  48 },
	{ 400, 300, 120,  90 },
	{ 320, 240, 160, 120 },
};

bool cameraPoseForScene(const Common::String &sceneName, CameraPose &out) {
	// chambreb/chambrev are the day/night variant pair for scene enum id
	// 6 (scene-load-findings.md sec.1); only one literal pose was
	// recovered and nothing suggests the two variants differ, so
	// chambrev shares chambreb's.
	const Common::String lookupName = sceneName.equalsIgnoreCase("chambrev") ? Common::String("chambreb") : sceneName;

	for (uint i = 0; i < ARRAYSIZE(kCameraPoses); i++) {
		if (lookupName.equalsIgnoreCase(kCameraPoses[i].name)) {
			out = kCameraPoses[i].pose;
			return true;
		}
	}

	// auberge/hopiext: no literal recovered (see this function's doc
	// comment in box3di.h). Documented fallback: an identity camera at
	// the world origin -- deliberately NOT a guess at the real pose.
	out = CameraPose();
	return false;
}

bool projectPoint(const double world[3], const CameraPose &camera, int zoomLevel, double &outX, double &outY, double &outDepth) {
	assert(zoomLevel >= 0 && zoomLevel < 4);

	Mat3 rx, ry, r;
	rotX(bamToRadians(camera.rotPitchBam), rx);
	rotY(bamToRadians(camera.rotYawBam), ry);
	matMul(rx, ry, r); // R = Rx(pitch) * Ry(yaw) -- applying R to a vector rotates by yaw first, then pitch.

	const double rel[3] = {
		world[0] - camera.x,
		world[1] - camera.y,
		world[2] - camera.z,
	};
	double cam[3];
	matVec3(r, rel, cam);

	const double zc = cam[2];
	if (fabs(zc) < 1e-6)
		return false;

	const ZoomViewport &vp = kZoomViewports[zoomLevel];
	const double cx = vp.width / 2.0 + vp.offsetX;
	const double cy = vp.height / 2.0 + vp.offsetY;
	const double scaleX = kCameraFocalLength;
	const double scaleY = scaleX * ((double)vp.height / (double)vp.width);

	outX = cx + scaleX * cam[0] / zc;
	outY = cy + scaleY * cam[1] / zc;
	outDepth = zc;
	return true;
}

bool HotspotBox::projectToScreen(const CameraPose &camera, int zoomLevel, Common::Rect &outRect, double &outNearestDepth) const {
	bool any = false;
	double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0, nearestZ = 0.0;

	for (int k = 0; k < 8; k++) {
		double sx, sy, sz;
		if (!projectPoint(vertices[k], camera, zoomLevel, sx, sy, sz))
			continue; // degenerate (on the camera plane) -- skip, like Python's `None`.
		if (sz <= 0.0)
			continue; // behind the camera -- dump_scene()'s `in_front` filter.

		if (!any) {
			minX = maxX = sx;
			minY = maxY = sy;
			nearestZ = sz;
			any = true;
		} else {
			minX = MIN(minX, sx);
			maxX = MAX(maxX, sx);
			minY = MIN(minY, sy);
			maxY = MAX(maxY, sy);
			nearestZ = MIN(nearestZ, sz);
		}
	}

	if (!any)
		return false; // entirely behind camera or degenerate -- not visible/pickable at this pose.

	const int32 left = CLIP<int32>((int32)floor(minX), -32768, 32767);
	const int32 right = CLIP<int32>((int32)ceil(maxX), left + 1, 32767);
	const int32 top = CLIP<int32>((int32)floor(minY), -32768, 32767);
	const int32 bottom = CLIP<int32>((int32)ceil(maxY), top + 1, 32767);

	outRect = Common::Rect((int16)left, (int16)top, (int16)right, (int16)bottom);
	outNearestDepth = nearestZ;
	return true;
}

Common::Array<HotspotBox> findOrientedBoxes(const int32 *arr, uint32 count, int tol) {
	Common::Array<HotspotBox> boxes;
	if (count < 24)
		return boxes;

	// Deliberately `<`, matching the Python reference's own
	// `while i < len(arr) - 24` exactly (including its one-i32-short outer
	// bound) so counts are bit-for-bit comparable.
	const uint32 limit = count - 24;
	uint32 i = 0;
	while (i < limit) {
		HotspotBox box;
		if (tryBoxAt(arr, i, tol, box)) {
			box.index = boxes.size();
			box.startWord = i;
			boxes.push_back(box);
			i += 24;
		} else {
			i++;
		}
	}
	return boxes;
}

Common::Array<HotspotBox> decodeHotspotBoxes(const Common::Array<byte> &boxRecordBytes, int tol) {
	// The record's shared 24-byte header (next/size/type/2x reserved/count
	// -- 3dc-format.md sec.1 and sec.3.1) precedes the payload the scan
	// walks; the Python reference slices its input the same way
	// (`struct.unpack_from(f'<{n}i', box_blob, 24)`).
	const uint32 kHeaderSize = 24;
	if (boxRecordBytes.size() <= kHeaderSize)
		return Common::Array<HotspotBox>();

	const uint32 n = (boxRecordBytes.size() - kHeaderSize) / 4;
	Common::Array<int32> arr(n);
	for (uint32 i = 0; i < n; i++)
		arr[i] = READ_LE_INT32(boxRecordBytes.data() + kHeaderSize + i * 4);

	return findOrientedBoxes(arr.data(), n, tol);
}

} // End of namespace Vangogh
