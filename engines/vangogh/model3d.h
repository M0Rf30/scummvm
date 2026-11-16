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

#ifndef VANGOGH_MODEL3D_H
#define VANGOGH_MODEL3D_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

namespace Vangogh {

class BFGFile;

/**
 * A scene's 3D model: the `.3DC` node tree plus the `.3DM` texture banks
 * of its materials, flattened into arrays that Renderer3D can walk without
 * further parsing. Format reference: vangogh-re notes/render3d.md sec.2-3
 * (PEINTRE.exe sub_43abb0 / sub_43a560 relocator).
 *
 * All vertices/UVs of all nodes live in two flat arrays; a vertex remembers
 * which node owns it (and therefore which node matrix transforms it), a
 * face remembers global indices only.
 */
class Model3D {
public:
	/** Face list span types (sub_44f3f0 dispatch == rasteriser span type). */
	enum ListType {
		kTextured = 3,
		kColorKeyed = -6,
		kBlended = -4,
		kFlat = 1
	};

	struct Node {
		int16 parent = -1;
		int16 firstChild = -1;
		int16 nextSibling = -1;
		int32 pos[3] = { 0, 0, 0 };
		float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };	///< row-major, v_parent = R * v_child
		char name[13] = { 0 };
		uint32 uvFirst = 0, uvCount = 0;	///< this node's UV records in uvs() (pairs)
	};

	struct Vertex {
		float x, y, z;	///< node-local
		uint16 node;	///< owner node
	};

	struct Face {
		uint32 v[3];	///< global vertex indices
		uint32 uv[3];	///< global UV indices (unused for flat lists)
	};

	struct FaceList {
		int32 type;
		uint16 node;
		int32 bank;		///< texture bank index, -1 if none (flat lists / missing .3DM)
		uint16 color;	///< RGB565, flat lists
		char key[17];	///< texture name (the material key, or the name given to retexture())
		uint32 firstFace;
		uint32 numFaces;
	};

	static const uint kTexSize = 256;

	/** Loads <scene>.3DC and every referenced .3DM out of @p bfg. */
	bool load(const BFGFile &bfg, const Common::String &scene);
	void clear();

	uint numNodes() const { return _nodes.size(); }
	const Node &node(uint i) const { return _nodes[i]; }
	/** Mutable node, for animation: the node pose is the only thing that changes after load (Anim3DA::apply). */
	Node &editNode(uint i) { return _nodes[i]; }
	/** Index of the node called @p name (case-insensitive), or -1. */
	int findNode(const char *name) const;
	/** World-space position of node @p i's origin (composes the parent chain of poses). */
	void worldPos(uint i, float out[3]) const;
	uint numVertices() const { return _vertices.size(); }
	const Vertex *vertices() const { return _vertices.begin(); }
	const float *uvs() const { return _uvs.begin(); }	///< interleaved u,v in texels
	const Face *faces() const { return _faces.begin(); }
	uint numLists() const { return _lists.size(); }
	const FaceList &list(uint i) const { return _lists[i]; }

	/**
	 * Registers the texture @p name backed by the 3DM record @p file (sub_43ded0), so retexture() can switch to it
	 * later without the archive. False if the record is missing.
	 */
	bool registerTexture(const BFGFile &bfg, const char *name, const char *file);
	/**
	 * sub_43c0b0: every face list of the model whose texture is called @p from now uses the registered texture
	 * @p to (and is called @p to afterwards). Returns the number of lists switched.
	 */
	uint retexture(const char *from, const char *to);
	/**
	 * Adds (@p du, @p dv) texels to every UV record of @p node (the exe's UV record walks, sub_43e2d0 and friends).
	 * With @p wrap the coordinates stay in [0, 256) (the 0x80ffffff mask of the fire and steam scrolls).
	 */
	void shiftNodeUV(uint node, float du, float dv, bool wrap);

	const uint16 *palette(int32 bank) const { return &_palettes[(uint)bank * 256]; }	///< shade row 16 (node shade 15)
	const byte *texture(int32 bank) const { return &_textures[(uint)bank * kTexSize * kTexSize]; }

private:
	bool parse(const Common::Array<byte> &rec, const BFGFile &bfg);
	int32 loadBank(const BFGFile &bfg, const char *file);

	struct RegisteredTexture {
		char name[17];
		int32 bank;
	};

	Common::Array<RegisteredTexture> _registered;
	Common::Array<Node> _nodes;
	Common::Array<Vertex> _vertices;
	Common::Array<float> _uvs;
	Common::Array<Face> _faces;
	Common::Array<FaceList> _lists;
	Common::Array<uint16> _palettes;
	Common::Array<byte> _textures;
};

} // End of namespace Vangogh

#endif // VANGOGH_MODEL3D_H
