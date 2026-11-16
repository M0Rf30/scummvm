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

#include "vangogh/model3d.h"
#include "vangogh/bfg.h"

#include "common/endian.h"
#include "common/debug.h"
#include "common/util.h"

namespace Vangogh {

namespace {

// Decompressed record = 0x14-byte runtime header (type at +8) + body.
const uint32 kRecHeader = 0x14;
const uint32 kRecTypeOffset = 8;
const uint32 kRecType3DC = 1;
const uint32 kRecType3DM = 3;

const uint32 kMaterialSize = 0x2c;
const uint32 kVertexSize = 0x28;
const uint32 kUVSize = 8;
const uint32 kMaxNodes = 4096;

// Shade row of every textured pixel: row = 31 - nodeShade, nodeShade = 15 (render3d.md sec.3).
const uint32 kShadeRow = 16;
const uint32 kPaletteBytes = 0x8000;

/** Bounds-checked little-endian reader over the .3DC body. */
struct Body {
	const byte *p;
	uint32 size;
	uint32 base;	///< nodeOff[0] - 1: stored pointer + base = body offset

	bool has(uint32 off, uint32 len) const { return off <= size && len <= size - off; }
	uint32 u32(uint32 off) const { return READ_LE_UINT32(p + off); }
	int32 i32(uint32 off) const { return READ_LE_INT32(p + off); }
	/** Reads a relocated pointer field; 0 stays 0. */
	uint32 ptr(uint32 off) const {
		const uint32 v = u32(off);
		return v ? v + base : 0;
	}
};

void copyName(char *dst, const byte *src, uint32 len) {
	uint32 i = 0;
	for (; i < len && src[i]; i++)
		dst[i] = (char)src[i];
	dst[i] = '\0';
}

} // end of anonymous namespace

void Model3D::clear() {
	_nodes.clear();
	_vertices.clear();
	_uvs.clear();
	_faces.clear();
	_lists.clear();
	_palettes.clear();
	_textures.clear();
}

int Model3D::findNode(const char *name) const {
	for (uint i = 0; i < _nodes.size(); i++) {
		if (scumm_stricmp(_nodes[i].name, name) == 0)
			return i;
	}
	return -1;
}

void Model3D::worldPos(uint i, float out[3]) const {
	// p_world = P_parent + R_parent * (P_node), applied from the node up to the root.
	float p[3] = { (float)_nodes[i].pos[0], (float)_nodes[i].pos[1], (float)_nodes[i].pos[2] };
	uint guard = _nodes.size();
	for (int n = _nodes[i].parent; n >= 0 && guard--; n = _nodes[n].parent) {
		const Node &a = _nodes[n];
		const float x = p[0], y = p[1], z = p[2];
		p[0] = a.pos[0] + a.rot[0] * x + a.rot[1] * y + a.rot[2] * z;
		p[1] = a.pos[1] + a.rot[3] * x + a.rot[4] * y + a.rot[5] * z;
		p[2] = a.pos[2] + a.rot[6] * x + a.rot[7] * y + a.rot[8] * z;
	}
	out[0] = p[0];
	out[1] = p[1];
	out[2] = p[2];
}

bool Model3D::load(const BFGFile &bfg, const Common::String &scene) {
	clear();

	Common::Array<byte> rec;
	if (!bfg.getRecord(scene + ".3DC", rec))
		return false;

	if (rec.size() < kRecHeader + 8 || READ_LE_UINT32(&rec[kRecTypeOffset]) != kRecType3DC) {
		warning("Vangogh: %s.3DC is not a type-1 model record", scene.c_str());
		return false;
	}

	if (!parse(rec, bfg)) {
		clear();
		return false;
	}
	return true;
}

int32 Model3D::loadBank(const BFGFile &bfg, const char *file) {
	const Common::String name = Common::String(file) + ".3DM";
	Common::Array<byte> rec;
	if (!bfg.hasRecord(name) || !bfg.getRecord(name, rec))
		return -1;
	if (rec.size() < kRecHeader + kPaletteBytes || READ_LE_UINT32(&rec[kRecTypeOffset]) != kRecType3DM) {
		warning("Vangogh: %s is not a usable type-3 texture record", name.c_str());
		return -1;
	}

	const byte *body = &rec[kRecHeader];
	const uint32 bodySize = rec.size() - kRecHeader;
	const int32 bank = (int32)(_palettes.size() / 256);

	// 32 shade rows x 256 dwords; the RGB565 colour is the high half.
	for (uint i = 0; i < 256; i++)
		_palettes.push_back(READ_LE_UINT16(body + (kShadeRow * 256 + i) * 4 + 2));

	// A few banks ship 255 or 257 rows; clip / zero-pad to 256.
	const uint32 texBytes = kTexSize * kTexSize;
	const uint32 avail = MIN<uint32>(bodySize - kPaletteBytes, texBytes);
	const uint32 start = _textures.size();
	_textures.resize(start + texBytes);
	byte *dst = _textures.begin() + start;
	memcpy(dst, body + kPaletteBytes, avail);
	memset(dst + avail, 0, texBytes - avail);
	return bank;
}

bool Model3D::parse(const Common::Array<byte> &rec, const BFGFile &bfg) {
	Body b;
	b.p = &rec[kRecHeader];
	b.size = rec.size() - kRecHeader;

	const uint32 numNodes = b.u32(0);
	if (numNodes == 0 || numNodes > kMaxNodes || !b.has(4, 4 * numNodes + 4)) {
		warning("Vangogh: bad 3DC node count %u", numNodes);
		return false;
	}

	Common::Array<uint32> nodeOffs;
	for (uint i = 0; i < numNodes; i++)
		nodeOffs.push_back(b.u32(4 + 4 * i));
	b.base = nodeOffs[0] - 1;

	// Material table.
	const uint32 matOff = 4 + 4 * numNodes;
	const uint32 numMats = b.u32(matOff);
	if (!b.has(matOff + 4, numMats * kMaterialSize)) {
		warning("Vangogh: bad 3DC material table");
		return false;
	}
	struct Material {
		char key[17];
		char file[17];
		uint16 color;
		int32 bank;	///< -2 = not yet loaded
	};
	Common::Array<Material> mats;
	mats.resize(numMats);
	for (uint i = 0; i < numMats; i++) {
		const byte *m = b.p + matOff + 4 + i * kMaterialSize;
		copyName(mats[i].key, m, 16);
		copyName(mats[i].file, m + 16, 16);
		mats[i].color = READ_LE_UINT32(m + 32) & 0xffff;
		mats[i].bank = -2;
	}

	// Pass 1: nodes, vertex and UV arrays. maps: body dword index -> global index.
	const uint32 mapSize = b.size / 4 + 1;
	Common::Array<int32> vertMap, uvMap;
	vertMap.resize(mapSize);
	uvMap.resize(mapSize);
	for (uint i = 0; i < mapSize; i++)
		vertMap[i] = uvMap[i] = -1;

	_nodes.resize(numNodes);
	for (uint i = 0; i < numNodes; i++) {
		const uint32 o = nodeOffs[i];
		if (!b.has(o, 0xd8)) {
			warning("Vangogh: 3DC node %u out of range", i);
			return false;
		}
		Node &n = _nodes[i];
		copyName(n.name, b.p + o, 12);
		for (int k = 0; k < 3; k++)
			n.pos[k] = b.i32(o + 0x1c + 4 * k);
		for (int k = 0; k < 9; k++)
			n.rot[k] = b.i32(o + 0x28 + 4 * k) / 32768.0f;

		const uint32 nv = b.u32(o + 0x7c), vp = b.ptr(o + 0x80);
		if (nv) {
			if (!b.has(vp, nv * kVertexSize) || (vp & 3)) {
				warning("Vangogh: node '%s' vertex array out of range", n.name);
				return false;
			}
			for (uint k = 0; k < nv; k++) {
				const uint32 vo = vp + k * kVertexSize;
				Vertex v;
				v.x = (float)b.i32(vo + 4);
				v.y = (float)b.i32(vo + 8);
				v.z = (float)b.i32(vo + 12);
				v.node = i;
				vertMap[vo / 4] = _vertices.size();
				_vertices.push_back(v);
			}
		}

		const uint32 nuv = b.u32(o + 0x84), uvp = b.ptr(o + 0x88);
		if (nuv) {
			if (!b.has(uvp, nuv * kUVSize) || (uvp & 3)) {
				warning("Vangogh: node '%s' UV array out of range", n.name);
				return false;
			}
			for (uint k = 0; k < nuv; k++) {
				const uint32 uo = uvp + k * kUVSize;
				uvMap[uo / 4] = _uvs.size() / 2;
				_uvs.push_back(b.i32(uo) / 65536.0f);
				_uvs.push_back(b.i32(uo + 4) / 65536.0f);
			}
		}
	}

	// Tree links by offset.
	for (uint i = 0; i < numNodes; i++) {
		const uint32 o = nodeOffs[i];
		const uint32 links[3] = { b.ptr(o + 0x10), b.ptr(o + 0x14), b.ptr(o + 0x18) };
		int16 *dst[3] = { &_nodes[i].parent, &_nodes[i].firstChild, &_nodes[i].nextSibling };
		for (int k = 0; k < 3; k++) {
			*dst[k] = -1;
			if (!links[k])
				continue;
			for (uint j = 0; j < numNodes; j++) {
				if (nodeOffs[j] == links[k]) {
					*dst[k] = j;
					break;
				}
			}
		}
	}

	// Pass 2: face lists (list A) of each node.
	for (uint i = 0; i < numNodes; i++) {
		uint32 lp = b.ptr(nodeOffs[i] + 0xa4);
		for (uint guard = 0; lp && guard < 10000; guard++) {
			if (!b.has(lp, 0x30)) {
				warning("Vangogh: face list out of range in node '%s'", _nodes[i].name);
				return false;
			}
			FaceList fl;
			fl.type = b.i32(lp + 4);
			fl.node = i;
			fl.bank = -1;
			fl.color = 0;
			fl.firstFace = _faces.size();
			fl.numFaces = 0;

			char name[13];
			copyName(name, b.p + lp + 0xc, 12);
			const uint32 count = b.u32(lp + 0x1c);
			const uint32 facep = b.ptr(lp + 0x20);
			const uint32 recSize = b.u32(lp + 0x2c);
			const uint32 next = b.ptr(lp);

			// Material key: exact, else prefix (12-char names truncate longer keys).
			int mat = -1;
			for (uint m = 0; m < numMats && mat < 0; m++) {
				if (!scumm_stricmp(mats[m].key, name))
					mat = m;
			}
			for (uint m = 0; m < numMats && mat < 0 && strlen(name) >= 12; m++) {
				if (!scumm_strnicmp(mats[m].key, name, 12))
					mat = m;
			}

			const bool flat = (fl.type == kFlat);
			if (mat >= 0) {
				if (flat) {
					fl.color = mats[mat].color;
				} else {
					if (mats[mat].bank == -2)
						mats[mat].bank = mats[mat].file[0] ? loadBank(bfg, mats[mat].file) : -1;
					fl.bank = mats[mat].bank;
				}
			}

			const uint32 minRec = flat ? 0x2c : 0x40;
			const bool usable = count && recSize >= minRec && (flat || fl.bank >= 0) && b.has(facep, count * recSize);
			if (usable) {
				for (uint k = 0; k < count; k++) {
					const uint32 r = facep + k * recSize;
					Face f;
					bool good = true;
					static const uint32 vOffs[3] = { 8, 0x14, 0x20 };
					static const uint32 uvOffs[3] = { 0x34, 0x38, 0x3c };
					for (int c = 0; c < 3; c++) {
						const uint32 va = b.ptr(r + vOffs[c]);
						const int32 gv = ((va & 3) || va / 4 >= mapSize) ? -1 : vertMap[va / 4];
						f.v[c] = gv;
						good &= gv >= 0;
						f.uv[c] = 0;
						if (!flat) {
							const uint32 ua = b.ptr(r + uvOffs[c]);
							const int32 gu = ((ua & 3) || ua / 4 >= mapSize) ? -1 : uvMap[ua / 4];
							f.uv[c] = gu;
							good &= gu >= 0;
						}
					}
					if (good)
						_faces.push_back(f);
				}
				fl.numFaces = _faces.size() - fl.firstFace;
			}
			if (fl.numFaces)
				_lists.push_back(fl);
			lp = next;
		}
	}

	debug(1, "Vangogh: model: %u nodes, %u vertices, %u faces in %u lists, %u texture banks",
		numNodes, _vertices.size(), _faces.size(), _lists.size(), (uint)(_palettes.size() / 256));
	return true;
}

} // End of namespace Vangogh
