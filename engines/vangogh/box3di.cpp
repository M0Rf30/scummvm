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

} // end of anonymous namespace

Common::Rect HotspotBox::projectToScreen() const {
	// TODO(SceneFlowRE): see the struct comment in box3di.h -- placeholder
	// orthographic projection only, arbitrary scale+offset, not the real
	// camera model.
	const double kPixelsPerMm = 1.0 / 8.0;
	const double originX = 320.0;
	const double originY = 240.0;

	double minU = vertices[0][otherAxes[0]];
	double maxU = minU;
	double minV = vertices[0][otherAxes[1]];
	double maxV = minV;
	for (int k = 1; k < 4; k++) {
		const double u = vertices[k][otherAxes[0]];
		const double v = vertices[k][otherAxes[1]];
		minU = MIN(minU, u);
		maxU = MAX(maxU, u);
		minV = MIN(minV, v);
		maxV = MAX(maxV, v);
	}

	const int32 left = CLIP<int32>((int32)(originX + minU * kPixelsPerMm), -32768, 32767);
	const int32 right = CLIP<int32>((int32)(originX + maxU * kPixelsPerMm), left + 1, 32767);
	const int32 top = CLIP<int32>((int32)(originY + minV * kPixelsPerMm), -32768, 32767);
	const int32 bottom = CLIP<int32>((int32)(originY + maxV * kPixelsPerMm), top + 1, 32767);

	return Common::Rect((int16)left, (int16)top, (int16)right, (int16)bottom);
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
