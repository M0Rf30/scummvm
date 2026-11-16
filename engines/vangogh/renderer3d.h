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

#ifndef VANGOGH_RENDERER3D_H
#define VANGOGH_RENDERER3D_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"
#include "graphics/surface.h"

namespace Vangogh {

class Model3D;

/**
 * One of the 4 fixed viewport rects of the exe's projection setup
 * (sub_43b460): level 0 is the full 640x480 view, 1..3 are tighter zooms.
 */
struct ZoomViewport {
	int16 width, height, offsetX, offsetY;
};

/** The 4 literal zoom-level viewports, indices 0-3 (see ZoomViewport). */
extern const ZoomViewport kZoomViewports[4];

/** Focal-length-style projection scale, identical at all 4 zoom levels (sub_43b460). */
const int kCameraFocalLength = 480;

/**
 * Camera as passed to the original SetCamera(x, y, z, rotA, rotB, rotC):
 * rotA = pitch about X, rotB = yaw about Y (12-bit BAM, 4096 = 360 degrees),
 * rotC (roll) is always 0 (world.md sec.9.1).
 */
struct Pose3D {	// aggregate; value-initialise (Pose3D()) for the origin
	int32 x, y, z;
	int32 rotA;	///< pitch
	int32 rotB;	///< yaw
};

/**
 * Software renderer for Model3D, a float port of PEINTRE.exe's scene
 * rasteriser (render3d.md sec.4/5, tools/r3d_render.py): view-space
 * transform through the node tree, near clip, screen-winding back-face
 * cull, and a per-pixel 1/z buffer with perspective-correct texturing.
 * Span types: 3 opaque, -6 colour-keyed (texel 0 transparent, wrapping),
 * -4 50% blend with the framebuffer, 1 flat colour. Output is RGB565.
 */
class Renderer3D {
public:
	Renderer3D();
	~Renderer3D();

	/** Selects one of the 4 zoom viewports (0 = full 640x480). */
	void setZoom(int zoom);

	/**
	 * Prepares the per-node state for @p model (all nodes visible). Must be
	 * called whenever the model changes, before render()/pick().
	 */
	void bindModel(const Model3D &model);

	/** Hides/shows a node's own faces (the exe's HideObject/ShowObject, node flag bit 0); hidden nodes are neither drawn nor picked. */
	void setNodeHidden(uint node, bool hidden) { _hidden[node] = hidden ? 1 : 0; }

	/** Renders @p model from @p pose. All buffers are preallocated; render() itself does not allocate. */
	void render(const Model3D &model, const Pose3D &pose);

	/**
	 * Port of sub_442460 (PickAtMouse): after render(), returns the node whose
	 * front-facing triangle contains screen pixel (@p x, @p y) with the smallest
	 * view depth, or -1. Hit-tests the projected triangles of the last render().
	 * @p outDepth, if given, receives the hit's view-space depth.
	 */
	int pick(const Model3D &model, int x, int y, float *outDepth = nullptr);

	/** View-space position of @p node's origin as of the last render(). */
	void nodeViewPos(uint node, float out[3]) const;

	const Graphics::Surface &surface() const { return _fb; }

private:
	struct Vec3 { float x, y, z; };
	struct Clipped { float x, y, z, u, v; };
	struct ScreenV { float x, y, iz, uz, vz; };

	void transformModel(const Model3D &model, const Pose3D &pose);
	void drawFace(const Model3D &model, int type, const uint16 *pal, const byte *tex, uint16 flat, uint faceIndex);
	void pickTriangle(const ScreenV &a, const ScreenV &b, const ScreenV &c);
	void rasterTriangle(const ScreenV &a, const ScreenV &b, const ScreenV &c, int type,
		const uint16 *pal, const byte *tex, uint16 flat);

	Graphics::Surface _fb;
	Common::Array<byte> _hidden;	///< per node, 1 = faces skipped
	bool _picking;
	float _pickX, _pickY, _pickDepth;
	int _pickNode, _curNode;
	float *_zb;		///< 1/z, 0 = empty
	Common::Array<float> _nodeM;	///< 9 floats per node, view matrix
	Common::Array<float> _nodeP;	///< 3 floats per node, view position
	Common::Array<byte> _nodeSeen;
	Common::Array<uint16> _stack;
	Common::Array<Vec3> _view;		///< view-space position per model vertex

	int _x0, _y0, _w, _h;
	float _fx, _cx, _cy;
};

/** Default SetScene pose of a scene (world.md sec.9.2); false if none is known (identity returned). */
bool defaultPoseForScene(const Common::String &scene, Pose3D &out);

/**
 * Loads scenes_3d/<scene>.bfg through SearchMan, renders it from @p pose
 * (the scene default if null) and writes a PNG to @p outPath. Needs no
 * graphics output, so it works headless.
 */
bool renderSceneToPNG(const Common::String &scene, const Common::String &outPath, const Pose3D *pose);

} // End of namespace Vangogh

#endif // VANGOGH_RENDERER3D_H
