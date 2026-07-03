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
#include "common/rect.h"
#include "common/scummsys.h"
#include "common/str.h"

namespace Vangogh {

/**
 * A scene's static, hardcoded fixed camera pose: position `(x,y,z)` in the
 * same raw integer units as `.3DI` box coordinates (millimetre-scale),
 * plus 2 BAM-encoded (12-bit, `& 0xfff`, 4096 units/circle) rotation
 * angles -- yaw about Y applied before pitch about X, the convention
 * scene_hotspots.py adopts (see its module docstring's "CONFIDENCE
 * CAVEAT": the exact axis order/sign baked into PEINTRE.exe's
 * fcn.004411c0 was not decoded bit-for-bit, but this reproduces
 * plausible, mostly-on-screen results against real `.3DI` data). Ported
 * from the literal table baked into fcn.00427600 -- see
 * cameraPoseForScene() below for the concrete per-scene numbers.
 */
struct CameraPose {
	int32 x, y, z;
	int32 rotYawBam;   ///< "rotA" in scene_hotspots.py/scene-flow.md sec.3.3.
	int32 rotPitchBam; ///< "rotB".
};

/**
 * One of the 4 fixed viewport rects the real renderer/hit-tester uses
 * (`fcn.0043b460` call sites, scene-flow.md sec.3.3) -- level 0 is the
 * normal full-room view; 1..3 are the progressively tighter object
 * close-up zooms used by the SAME hit-test (`fcn.00425e50`'s per-zoom
 * bounding rects match these exactly). Only level 0 is used for Scene's
 * room-level hotspot picking; the others exist for parity with
 * scene_hotspots.py's ZOOM_VIEWPORTS and possible future close-up work.
 */
struct ZoomViewport {
	int16 width, height, offsetX, offsetY;
};

/** The 4 literal zoom-level viewports, indices 0-3 (see ZoomViewport). */
extern const ZoomViewport kZoomViewports[4];

/** Literal focal-length-style projection scale, identical at all 4 zoom levels (fcn.0043b460). */
const int kCameraFocalLength = 480;

/**
 * Looks up @p sceneName's literal default camera pose, transcribed from
 * PEINTRE.exe's fcn.00427600 (scene-flow.md sec.3.3/sec.6;
 * scene_hotspots.py's CAMERA_POSES) -- 12 of 14 scenes have a recovered
 * literal. `chambrev` shares `chambreb`'s pose (day/night variant of the
 * same room, scene-load-findings.md sec.1). Returns false, filling @p out
 * with a documented IDENTITY fallback (origin position, zero rotation),
 * for the 2 scenes with no recovered literal: `auberge` (the literal this
 * session's reversing found is a suspicious all-zero placeholder, almost
 * certainly inherited/uninitialized state, not a real pose) and
 * `hopiext` (not located in the switches transcribed at all). This is
 * NOT a guess at the real pose -- callers SHOULD log that projections for
 * such a scene are unreliable when this returns false.
 */
bool cameraPoseForScene(const Common::String &sceneName, CameraPose &out);

/**
 * Projects one world-space point (raw `.3DI` units) through @p camera at
 * @p zoomLevel (0-3, see kZoomViewports), field-for-field matching
 * scene_hotspots.py's project(): rotate by yaw then pitch, translate
 * relative to the camera, then perspective-divide by camera-space Z. @p
 * outDepth receives that camera-space Z -- <=0 means the point is behind
 * the camera (not really visible/pickable). Returns false (outputs left
 * unset) only in the degenerate case `|Zc| < 1e-6`, mirroring the Python
 * reference's `return None`.
 */
bool projectPoint(const double world[3], const CameraPose &camera, int zoomLevel, double &outX, double &outY, double &outDepth);

/**
 * One oriented-hexahedron (extruded quadrilateral prism) hotspot/collision
 * volume decoded from a scene's `BOX.3DI` record -- see findOrientedBoxes()
 * below for the on-disk format and the detection algorithm, ported from
 * vangogh-notes round3/mesh_3dm_parser.py (itself re-verified from
 * milestone2/3dc-format.md sec.3, "the one format ... fully validated
 * end-to-end"). All coordinates are in raw file units (millimetres,
 * inferred but not confirmed -- no explicit units field exists on disk).
 */
struct HotspotBox {
	uint32 index = 0;          ///< 0-based order of discovery; matches the Python tool's own enumeration order.
	uint32 startWord = 0;      ///< i32-array offset (from the payload scan start) where this box's 8 corners begin; diagnostic only.
	int heightAxis = 0;        ///< Which raw axis (0/1/2) is this box's "vertical" extrusion axis.
	int otherAxes[2] = {0, 0}; ///< The other two axes, ascending order; these span the box's horizontal footprint plane.
	double bottomHeight = 0.0; ///< Extent along heightAxis where the footprint starts.
	double topHeight = 0.0;    ///< Extent along heightAxis where the footprint ends (topHeight > bottomHeight).

	/**
	 * All 8 corners: [0..3] = bottom face, [4..7] = top face, both using
	 * the SAME CCW-sorted footprint (see findOrientedBoxes() -- the top
	 * face's own raw corner order is discarded once it's confirmed to
	 * match the bottom footprint within tolerance, exactly like
	 * box_to_vertices() in the reversing notes).
	 */
	double vertices[8][3] = {};

	/**
	 * Real per-scene fixed-camera projection, ported from
	 * scene_hotspots.py's project()+dump_scene()'s bbox reduction:
	 * projects all 8 corners through @p camera at @p zoomLevel, keeps
	 * only the corners in FRONT of the camera (camera-space Z>0, exactly
	 * dump_scene()'s `in_front` filter), and returns their screen-space
	 * bounding box plus the NEAREST (smallest positive) camera-space Z
	 * among them in @p outNearestDepth -- for nearest-depth-wins hit
	 * testing across multiple overlapping boxes. Returns false (leaving
	 * both outputs unset) if every corner is behind the camera/degenerate
	 * at this pose, mirroring dump_scene()'s "entirely behind camera or
	 * degenerate -- not visible/pickable" branch.
	 */
	bool projectToScreen(const CameraPose &camera, int zoomLevel, Common::Rect &outRect, double &outNearestDepth) const;
};

/**
 * Detects oriented-hexahedron prisms in a flat little-endian i32 payload --
 * a brute-force, heuristic sliding-window scan ported field-for-field from
 * mesh_3dm_parser.py's find_oriented_boxes()/box_to_vertices() (see
 * round3/mesh_3dm-geometry.md and milestone2/3dc-format.md sec.3 for the
 * reversing trail). There is no length-prefixed box list on disk -- boxes
 * are simply wherever 8 consecutive (x,y,z) triples happen to describe one,
 * interleaved with other, not-yet-understood sub-records (see
 * 3dc-format.md sec.3.2's "unit vectors scaled by 32768" remainder) that
 * this scan silently steps over one i32 at a time.
 *
 * At each candidate start position, 8 consecutive (x,y,z) triples are read
 * (corners 0..3 = "bottom", 4..7 = "top") and, for each candidate height
 * axis in turn (0, 1, then 2 -- the first one that fits wins, matching the
 * Python reference's `for h_axis in range(3): ... return` short-circuit):
 *   1. both quadruples must be flat along that axis (extent <= @p tol);
 *   2. the bottom footprint (the other two axes), CCW-sorted around its own
 *      centroid, must match the top footprint's 4 points within @p tol,
 *      pairwise (a greedy nearest-unused match, not a sorted comparison --
 *      see the .cpp);
 *   3. the resulting height must span at least 50 raw units;
 *   4. the footprint's shoelace area (x2) must be at least 5000 raw units^2;
 *   5. none of the height or footprint values may equal a
 *      likely-invalid-data sentinel (-32768, -32767, 32767, 32768).
 * A match consumes exactly 24 i32 (the 8 corners) and scanning resumes
 * right after; a non-match slides the window by one i32 and retries --
 * including the exact off-by-one in the outer loop bound (`i < count - 24`,
 * not `<=`) that the Python reference has, so counts match it exactly.
 *
 * @param arr Little-endian-decoded i32 payload (the record's bytes AFTER
 *            its own 24-byte shared header -- see decodeHotspotBoxes()).
 * @param count Number of i32 entries in @p arr.
 * @param tol Coordinate tolerance for the flatness/matching checks above;
 *            defaults to 3, matching the Python reference's default.
 */
Common::Array<HotspotBox> findOrientedBoxes(const int32 *arr, uint32 count, int tol = 3);

/**
 * Convenience wrapper around findOrientedBoxes(): skips a decoded `.3DI`
 * record's shared 24-byte header (next/size/type/2x reserved/count, see
 * 3dc-format.md sec.1 and sec.3.1 -- `count` sits right before the payload
 * findOrientedBoxes() scans, at the same offset the Python reference's
 * `struct.unpack_from(f'<{n}i', box_blob, 24)` starts from), and converts
 * the remaining bytes from little-endian i32.
 *
 * @param boxRecordBytes A `BOX.3DI` record's full decompressed bytes (as
 *                        returned by BFGFile::getRecord()), header included.
 */
Common::Array<HotspotBox> decodeHotspotBoxes(const Common::Array<byte> &boxRecordBytes, int tol = 3);

} // End of namespace Vangogh

#endif // VANGOGH_BOX3DI_H
