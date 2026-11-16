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

#include "vangogh/renderer3d.h"
#include "vangogh/bfg.h"
#include "vangogh/box3di.h"
#include "vangogh/model3d.h"

#include "common/debug.h"
#include "common/file.h"
#include "common/util.h"
#include "graphics/pixelformat.h"
#include "image/png.h"

#include <math.h>

namespace Vangogh {

namespace {

const int kScreenW = 640;
const int kScreenH = 480;
const float kNearPlane = 64.0f;	// sub_43b6b0(0x40) in SetScene

struct NamedPose {
	const char *name;
	Pose3D pose;
};

// world.md sec.9.2: (x, y, z, rotA, rotB). musee is the boot pose (previous scene 0).
const NamedPose kDefaultPoses[] = {
	{ "auberge",  {     0,    0,     0,    0,    0 } },
	{ "maisonet", {  1149,  141, -1491,    0,  275 } },
	{ "mangeurs", {  -106,    6,   100, 3946,  145 } },
	{ "cafe",     {   393, -175,  -136, 3976,   92 } },
	{ "chambreb", {  -628, -336,  -268, 3946,  150 } },
	{ "chambrev", {  -628, -336,  -268, 3946,  150 } },
	{ "maisonj",  { -3350,  489, -5996,   60, 3944 } },
	{ "hopiint",  {   -67,  -78,  -345, 4036,   17 } },
	{ "pont",     {  7805, 1167,  2183,   30, 3265 } },
	{ "terrasse", {  -123, -102,  -846, 4066,   31 } },
	{ "jardin",   {   -46, -335,  1263,    0, 4052 } },
	{ "champ",    {  2140,  253,  2270, 4006,  214 } },
	{ "eglise",   { -7189,  802,   102,  210, 3756 } },
	{ "musee",    {   -39, -209,   361, 4066,   20 } },
};

inline int floorToInt(float f) {
	const int i = (int)f;
	return (f < (float)i) ? i - 1 : i;
}

} // end of anonymous namespace

bool defaultPoseForScene(const Common::String &scene, Pose3D &out) {
	for (uint i = 0; i < ARRAYSIZE(kDefaultPoses); i++) {
		if (scene.equalsIgnoreCase(kDefaultPoses[i].name)) {
			out = kDefaultPoses[i].pose;
			return true;
		}
	}
	out = Pose3D();
	return false;
}

Renderer3D::Renderer3D() : _zb(nullptr), _x0(0), _y0(0), _w(kScreenW), _h(kScreenH), _fx(480.0f), _cx(320.0f), _cy(240.0f) {
	_fb.create(kScreenW, kScreenH, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
	_zb = new float[kScreenW * kScreenH];
	setZoom(0);
}

Renderer3D::~Renderer3D() {
	delete[] _zb;
	_fb.free();
}

void Renderer3D::setZoom(int zoom) {
	assert(zoom >= 0 && zoom < 4);
	const ZoomViewport &vp = kZoomViewports[zoom];
	_x0 = vp.offsetX;
	_y0 = vp.offsetY;
	_w = vp.width;
	_h = vp.height;
	_fx = (float)(kCameraFocalLength * _w) / 640.0f;	// sub_43b464, aspect 1
	_cx = _w / 2.0f + _x0;
	_cy = _h / 2.0f + _y0;
}

void Renderer3D::transformModel(const Model3D &model, const Pose3D &pose) {
	const uint numNodes = model.numNodes();
	_nodeM.resize(numNodes * 9);
	_nodeP.resize(numNodes * 3);
	_nodeSeen.resize(numNodes);
	_stack.resize(numNodes);
	memset(_nodeSeen.begin(), 0, numNodes);

	// World -> view: V = M^T, M = Ry(rotB) * Rx(rotA) (world.md sec.9.1; yaw undone first, then pitch).
	const float a = 2.0f * (float)M_PI * (float)(pose.rotA & 0xfff) / 4096.0f;
	const float b = 2.0f * (float)M_PI * (float)(pose.rotB & 0xfff) / 4096.0f;
	const float sa = sinf(a), ca = cosf(a), sb = sinf(b), cb = cosf(b);
	const float V[9] = {
		cb,       0.0f, -sb,
		sa * sb,  ca,   cb * sa,
		sb * ca,  -sa,  cb * ca
	};

	// The camera node is the parent of the 3DC root: root.view = V * (root.pos - cam), root.M = V * root.rot.
	const Model3D::Node &root = model.node(0);
	const float rel[3] = { (float)(root.pos[0] - pose.x), (float)(root.pos[1] - pose.y), (float)(root.pos[2] - pose.z) };
	for (int r = 0; r < 3; r++) {
		_nodeP[r] = V[r * 3] * rel[0] + V[r * 3 + 1] * rel[1] + V[r * 3 + 2] * rel[2];
		for (int c = 0; c < 3; c++)
			_nodeM[r * 3 + c] = V[r * 3] * root.rot[c] + V[r * 3 + 1] * root.rot[3 + c] + V[r * 3 + 2] * root.rot[6 + c];
	}
	_nodeSeen[0] = 1;

	uint sp = 0;
	_stack[sp++] = 0;
	while (sp) {
		const uint n = _stack[--sp];
		const float *Mn = &_nodeM[n * 9];
		const float *pn = &_nodeP[n * 3];
		for (int c = model.node(n).firstChild; c >= 0; c = model.node(c).nextSibling) {
			if (_nodeSeen[c])
				continue;	// malformed (cyclic) tree
			_nodeSeen[c] = 1;
			const Model3D::Node &cn = model.node(c);
			float *Mc = &_nodeM[c * 9];
			float *pc = &_nodeP[c * 3];
			for (int r = 0; r < 3; r++) {
				for (int k = 0; k < 3; k++)
					Mc[r * 3 + k] = Mn[r * 3] * cn.rot[k] + Mn[r * 3 + 1] * cn.rot[3 + k] + Mn[r * 3 + 2] * cn.rot[6 + k];
				pc[r] = pn[r] + Mn[r * 3] * cn.pos[0] + Mn[r * 3 + 1] * cn.pos[1] + Mn[r * 3 + 2] * cn.pos[2];
			}
			_stack[sp++] = c;
		}
	}

	const uint numVerts = model.numVertices();
	_view.resize(numVerts);
	const Model3D::Vertex *verts = model.vertices();
	for (uint i = 0; i < numVerts; i++) {
		const Model3D::Vertex &v = verts[i];
		Vec3 &o = _view[i];
		if (!_nodeSeen[v.node]) {
			o.x = o.y = 0.0f;
			o.z = -1.0f;	// unreachable node: always clipped
			continue;
		}
		const float *M = &_nodeM[v.node * 9];
		const float *p = &_nodeP[v.node * 3];
		o.x = p[0] + M[0] * v.x + M[1] * v.y + M[2] * v.z;
		o.y = p[1] + M[3] * v.x + M[4] * v.y + M[5] * v.z;
		o.z = p[2] + M[6] * v.x + M[7] * v.y + M[8] * v.z;
	}
}

void Renderer3D::render(const Model3D &model, const Pose3D &pose) {
	memset(_fb.getPixels(), 0, kScreenW * kScreenH * 2);	// background colour 0 = black
	memset(_zb, 0, kScreenW * kScreenH * sizeof(float));
	if (!model.numNodes())
		return;

	transformModel(model, pose);

	// Opaque (3, 1) first, then colour-keyed (-6), then the blended (-4) water, like the reference.
	static const int passTypes[3][2] = { { Model3D::kTextured, Model3D::kFlat }, { Model3D::kColorKeyed, 99 }, { Model3D::kBlended, 99 } };
	for (int pass = 0; pass < 3; pass++) {
		for (uint l = 0; l < model.numLists(); l++) {
			const Model3D::FaceList &fl = model.list(l);
			if (fl.type != passTypes[pass][0] && fl.type != passTypes[pass][1])
				continue;
			const uint16 *pal = fl.bank >= 0 ? model.palette(fl.bank) : nullptr;
			const byte *tex = fl.bank >= 0 ? model.texture(fl.bank) : nullptr;
			for (uint f = 0; f < fl.numFaces; f++)
				drawFace(model, fl.type, pal, tex, fl.color, fl.firstFace + f);
		}
	}
}

void Renderer3D::drawFace(const Model3D &model, int type, const uint16 *pal, const byte *tex, uint16 flat, uint faceIndex) {
	const Model3D::Face &f = model.faces()[faceIndex];
	const float *uvs = model.uvs();

	// Near-plane clip (Sutherland-Hodgman); u,v are affine in view space so they clip linearly.
	Clipped in[3];
	for (int i = 0; i < 3; i++) {
		const Vec3 &p = _view[f.v[i]];
		in[i].x = p.x;
		in[i].y = p.y;
		in[i].z = p.z;
		in[i].u = type == Model3D::kFlat ? 0.0f : uvs[f.uv[i] * 2];
		in[i].v = type == Model3D::kFlat ? 0.0f : uvs[f.uv[i] * 2 + 1];
	}

	Clipped poly[4];
	int n = 0;
	for (int i = 0; i < 3; i++) {
		const Clipped &a = in[i];
		const Clipped &b = in[(i + 1) % 3];
		const bool aIn = a.z >= kNearPlane, bIn = b.z >= kNearPlane;
		if (aIn)
			poly[n++] = a;
		if (aIn != bIn) {
			const float t = (kNearPlane - a.z) / (b.z - a.z);
			Clipped &o = poly[n++];
			o.x = a.x + t * (b.x - a.x);
			o.y = a.y + t * (b.y - a.y);
			o.z = kNearPlane;
			o.u = a.u + t * (b.u - a.u);
			o.v = a.v + t * (b.v - a.v);
		}
	}
	if (n < 3)
		return;

	ScreenV s[4];
	for (int i = 0; i < n; i++) {
		const float iz = 1.0f / poly[i].z;
		s[i].x = _cx + _fx * poly[i].x * iz;
		s[i].y = _cy + _fx * poly[i].y * iz;
		s[i].iz = iz;
		s[i].uz = poly[i].u * iz;
		s[i].vz = poly[i].v * iz;
	}

	// The engine draws a face only when its screen cross product is negative (sub_44c5e0).
	float area = 0.0f;
	for (int i = 0; i < n; i++) {
		const ScreenV &a = s[i], &b = s[(i + 1) % n];
		area += a.x * b.y - b.x * a.y;
	}
	if (area >= 0.0f)
		return;

	for (int k = 1; k < n - 1; k++)
		rasterTriangle(s[0], s[k], s[k + 1], type, pal, tex, flat);
}

void Renderer3D::rasterTriangle(const ScreenV &a, const ScreenV &b, const ScreenV &c, int type,
		const uint16 *pal, const byte *tex, uint16 flat) {
	const float d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
	if (fabsf(d) < 1e-9f)
		return;

	const float minY = MIN(a.y, MIN(b.y, c.y)), maxY = MAX(a.y, MAX(b.y, c.y));
	const int yStart = MAX(floorToInt(minY + 0.5f), _y0);	// first row with centre >= minY
	const int yEnd = MIN(floorToInt(maxY - 0.5f), _y0 + _h - 1);	// last row with centre <= maxY
	if (yEnd < yStart)
		return;

	// Plane gradients of 1/z, u/z, v/z with respect to screen x and y.
	const float inv = 1.0f / d;
	const float dx0 = (b.y - c.y) * inv, dx1 = (c.y - a.y) * inv;
	const float dy0 = (c.x - b.x) * inv, dy1 = (a.x - c.x) * inv;
	float gx[3], gy[3], base[3];
	const float a0[3] = { a.iz, a.uz, a.vz }, a1[3] = { b.iz, b.uz, b.vz }, a2[3] = { c.iz, c.uz, c.vz };
	for (int i = 0; i < 3; i++) {
		gx[i] = (a0[i] - a2[i]) * dx0 + (a1[i] - a2[i]) * dx1;
		gy[i] = (a0[i] - a2[i]) * dy0 + (a1[i] - a2[i]) * dy1;
		base[i] = a2[i];
	}

	const ScreenV *vs[3] = { &a, &b, &c };
	uint16 *fb = (uint16 *)_fb.getPixels();
	const int xMin = _x0, xMax = _x0 + _w - 1;

	for (int y = yStart; y <= yEnd; y++) {
		const float yc = y + 0.5f;
		float xl = 1e30f, xr = -1e30f;
		for (int e = 0; e < 3; e++) {
			const ScreenV *p = vs[e], *q = vs[(e + 1) % 3];
			if (p->y > q->y)
				SWAP(p, q);
			if (yc >= p->y && yc < q->y) {
				const float x = p->x + (yc - p->y) * (q->x - p->x) / (q->y - p->y);
				xl = MIN(xl, x);
				xr = MAX(xr, x);
			}
		}
		if (xl > xr)
			continue;

		const int xs = MAX((int)ceilf(xl - 0.5f), xMin);
		const int xe = MIN(floorToInt(xr - 0.5f), xMax);
		if (xe < xs)
			continue;

		const float xc = xs + 0.5f - c.x, yo = yc - c.y;
		float iz = base[0] + gx[0] * xc + gy[0] * yo;
		float uz = base[1] + gx[1] * xc + gy[1] * yo;
		float vz = base[2] + gx[2] * xc + gy[2] * yo;
		uint16 *row = fb + y * kScreenW;
		float *zrow = _zb + y * kScreenW;

		for (int x = xs; x <= xe; x++, iz += gx[0], uz += gx[1], vz += gx[2]) {
			if (!(iz > zrow[x]))
				continue;

			if (type == Model3D::kFlat) {
				row[x] = flat;
				zrow[x] = iz;
				continue;
			}

			const float rz = 1.0f / iz;
			int u = floorToInt(uz * rz), v = floorToInt(vz * rz);
			if (type == Model3D::kColorKeyed) {
				u &= 255;
				v &= 255;
			} else {
				u = CLIP(u, 0, 255);
				v = CLIP(v, 0, 255);
			}
			const byte texel = tex[(v << 8) | u];
			uint16 col = pal[texel];
			if (type == Model3D::kColorKeyed) {
				if (!texel)
					continue;
			} else if (type == Model3D::kBlended) {
				col = (uint16)(((row[x] & 0xf7de) + (col & 0xf7de)) >> 1);
			}
			row[x] = col;
			zrow[x] = iz;
		}
	}
}

bool renderSceneToPNG(const Common::String &scene, const Common::String &outPath, const Pose3D *pose) {
	const Common::Path bfgPath(Common::String::format("scenes_3d/%s.bfg", scene.c_str()));
	Common::File bfgFile;
	if (!bfgFile.open(bfgPath)) {
		warning("Vangogh: render3d: could not open %s", bfgPath.toString().c_str());
		return false;
	}
	BFGFile bfg;
	if (!bfg.load(bfgFile)) {
		warning("Vangogh: render3d: failed to parse %s", bfgPath.toString().c_str());
		return false;
	}

	Model3D model;
	if (!model.load(bfg, scene))
		return false;

	Pose3D p = Pose3D();
	if (pose)
		p = *pose;
	else if (!defaultPoseForScene(scene, p))
		warning("Vangogh: render3d: no default pose known for '%s', using the origin", scene.c_str());

	Renderer3D renderer;
	renderer.render(model, p);

#ifdef USE_PNG
	Common::DumpFile out;
	if (!out.open(Common::Path(outPath, '/'))) {
		warning("Vangogh: render3d: could not create %s", outPath.c_str());
		return false;
	}
	if (!Image::writePNG(out, renderer.surface())) {
		warning("Vangogh: render3d: PNG encoding failed for %s", outPath.c_str());
		return false;
	}
	return true;
#else
	warning("Vangogh: render3d: this build has no PNG support");
	return false;
#endif
}

} // End of namespace Vangogh
