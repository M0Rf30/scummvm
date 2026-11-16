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


#include "vangogh/anim3da.h"
#include "vangogh/bfg.h"
#include "vangogh/model3d.h"

#include "common/endian.h"
#include "common/textconsole.h"
#include "common/util.h"

#include <math.h>

namespace Vangogh {

namespace {

const uint kRecordHeader = 0x14;	// type-4 object header before the body
const uint kChannelHeader = 20;
const uint kRotKeySize = 20, kPosKeySize = 16;

inline int32 readS32(const byte *p) {
	return (int32)READ_LE_UINT32(p);
}

}

void Anim3DA::clear() {
	_channels.clear();
	_rot.clear();
	_pos.clear();
	_duration = 0;
}

bool Anim3DA::load(const BFGFile &bfg, const char *recordName) {
	clear();
	Common::Array<byte> rec;
	if (!bfg.getRecord(recordName, rec))
		return false;
	if (rec.size() < kRecordHeader + 4 || READ_LE_UINT32(&rec[8]) != 4) {
		warning("Vangogh: %s is not a 3DA record", recordName);
		return false;
	}

	const byte *body = &rec[kRecordHeader];
	const uint bodySize = rec.size() - kRecordHeader;
	const uint32 n = READ_LE_UINT32(body);
	if (n == 0 || 4 + n * 4 > bodySize) {
		warning("Vangogh: %s: bad channel count %u", recordName, n);
		return false;
	}

	_channels.resize(n);
	for (uint32 i = 0; i < n; i++) {
		const uint32 off = READ_LE_UINT32(body + 4 + i * 4);
		if (off + kChannelHeader > bodySize) {
			warning("Vangogh: %s: channel %u is out of range", recordName, i);
			clear();
			return false;
		}
		const byte *ch = body + off;
		const uint32 nRot = READ_LE_UINT32(ch + 4), nPos = READ_LE_UINT32(ch + 8);
		const uint32 rotOff = READ_LE_UINT32(ch + 12), posOff = READ_LE_UINT32(ch + 16);
		if (rotOff + (uint64)nRot * kRotKeySize > bodySize || posOff + (uint64)nPos * kPosKeySize > bodySize) {
			warning("Vangogh: %s: channel %u keys are out of range", recordName, i);
			clear();
			return false;
		}
		if (i == 0)
			_duration = READ_LE_UINT32(ch);

		Channel &c = _channels[i];
		c.firstRot = _rot.size();
		c.numRot = nRot;
		for (uint32 k = 0; k < nRot; k++) {
			const byte *p = body + rotOff + k * kRotKeySize;
			RotKey key;
			key.time = readS32(p);
			for (int j = 0; j < 4; j++)
				key.q[j] = readS32(p + 4 + j * 4);
			_rot.push_back(key);
		}
		c.firstPos = _pos.size();
		c.numPos = nPos;
		for (uint32 k = 0; k < nPos; k++) {
			const byte *p = body + posOff + k * kPosKeySize;
			PosKey key;
			key.time = readS32(p);
			for (int j = 0; j < 3; j++)
				key.p[j] = readS32(p + 4 + j * 4);
			_pos.push_back(key);
		}
	}
	return true;
}

uint Anim3DA::numAnimated() const {
	uint n = 0;
	for (uint i = 0; i < _channels.size(); i++) {
		if (_channels[i].numRot >= 2 || _channels[i].numPos >= 2)
			n++;
	}
	return n;
}

bool Anim3DA::channelKeys(uint i, uint &numRot, uint &numPos) const {
	if (i >= _channels.size())
		return false;
	numRot = _channels[i].numRot;
	numPos = _channels[i].numPos;
	return true;
}

void Anim3DA::quatToMatrix(const float q[4], float m[9]) {
	// sub_441ec0: standard unit quaternion -> rotation matrix, row-major.
	const float x = q[0], y = q[1], z = q[2], w = q[3];
	m[0] = 1.0f - 2.0f * (y * y + z * z);
	m[1] = 2.0f * (x * y - w * z);
	m[2] = 2.0f * (x * z + w * y);
	m[3] = 2.0f * (x * y + w * z);
	m[4] = 1.0f - 2.0f * (x * x + z * z);
	m[5] = 2.0f * (y * z - w * x);
	m[6] = 2.0f * (x * z - w * y);
	m[7] = 2.0f * (y * z + w * x);
	m[8] = 1.0f - 2.0f * (x * x + y * y);
}

namespace {

/** sub_438310 key search: index of the first key with time >= t (clamped), in [1, n-1]. */
template<class Key>
uint findKey(const Key *keys, uint n, int t) {
	uint lo = 0, hi = n - 1;
	while (hi >= 2 && lo + 1 < hi) {
		const uint mid = (lo + hi) / 2;
		if (t <= keys[mid].time)
			hi = mid;
		else
			lo = mid;
	}
	return hi;
}

}

void Anim3DA::evalRot(const Channel &c, int t, float m[9]) const {
	const RotKey *keys = &_rot[c.firstRot];
	const uint hi = findKey(keys, c.numRot, t);
	float q[4];
	if (keys[hi].time <= t) {
		for (int j = 0; j < 4; j++)
			q[j] = keys[hi].q[j] / 32768.0f;
	} else {
		const RotKey &a = keys[hi - 1], &b = keys[hi];
		const int w = (int)((int64)(t - a.time) * 256 / (b.time - a.time));
		// sub_442180: slerp in 1.15 with the exe's special cases for (anti)parallel keys.
		const int32 dot = (int32)(((int64)a.q[0] * b.q[0] + (int64)a.q[1] * b.q[1] + (int64)a.q[2] * b.q[2] + (int64)a.q[3] * b.q[3]) >> 15);
		const float u = w / 256.0f;
		if (dot + 0x8000 < 0x15) {
			// Opposite hemispheres: swing through a quaternion at 90 degrees (w is not blended, as in the exe).
			const float qz[4] = { -(float)a.q[1], (float)a.q[0], -(float)a.q[3], (float)a.q[2] };
			const float c0 = fabsf(cosf(u * (float)M_PI)), c1 = sinf(u * (float)M_PI);
			for (int j = 0; j < 3; j++)
				q[j] = (a.q[j] * c0 + qz[j] * c1) / 32768.0f;
			q[3] = qz[3] / 32768.0f;
		} else {
			float wa, wb;
			if (0x8000 - dot < 0x15) {
				wa = (256 - w) / 256.0f;
				wb = u;
			} else {
				const float theta = acosf(dot / 32768.0f), s = sinf(theta);
				wa = sinf((1.0f - u) * theta) / s;
				wb = sinf(u * theta) / s;
			}
			for (int j = 0; j < 4; j++)
				q[j] = (a.q[j] * wa + b.q[j] * wb) / 32768.0f;
		}
	}
	quatToMatrix(q, m);
}

void Anim3DA::evalPos(const Channel &c, int t, int32 p[3]) const {
	const PosKey *keys = &_pos[c.firstPos];
	const uint hi = findKey(keys, c.numPos, t);
	if (keys[hi].time <= t) {
		for (int j = 0; j < 3; j++)
			p[j] = keys[hi].p[j];
		return;
	}
	const PosKey &a = keys[hi - 1], &b = keys[hi];
	const int w = (int)((int64)(t - a.time) * 256 / (b.time - a.time));
	for (int j = 0; j < 3; j++)
		p[j] = (int32)(((int64)b.p[j] * w + (int64)a.p[j] * (256 - w)) >> 8);
}

void Anim3DA::apply(Model3D &model, int t) const {
	const uint n = MIN<uint>(_channels.size(), model.numNodes());
	for (uint i = 0; i < n; i++) {
		const Channel &c = _channels[i];
		Model3D::Node &node = model.editNode(i);
		if (c.numRot >= 2)
			evalRot(c, t, node.rot);
		if (c.numPos >= 2)
			evalPos(c, t, node.pos);
	}
}

int Anim3DA::restPoseError(const Model3D &model, uint channel, float &rotDelta) const {
	rotDelta = 0.0f;
	if (channel >= _channels.size() || channel >= model.numNodes())
		return 0;
	const Channel &c = _channels[channel];
	const Model3D::Node &node = model.node(channel);
	int posDelta = 0;
	if (c.numPos >= 1) {
		const PosKey &k = _pos[c.firstPos];
		for (int j = 0; j < 3; j++)
			posDelta = MAX<int>(posDelta, ABS(k.p[j] - node.pos[j]));
	}
	if (c.numRot >= 1) {
		const RotKey &k = _rot[c.firstRot];
		float q[4], m[9];
		for (int j = 0; j < 4; j++)
			q[j] = k.q[j] / 32768.0f;
		quatToMatrix(q, m);
		for (int j = 0; j < 9; j++)
			rotDelta = MAX<float>(rotDelta, fabsf(m[j] - node.rot[j]));
	}
	return posDelta;
}

} // End of namespace Vangogh
