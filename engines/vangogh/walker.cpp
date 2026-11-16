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

// The script bot: what a player's hands do (steer with the arrows, point the mouse at something visible). It never
// touches the game state; collision, picking and every puzzle run through the normal frame.

#include "vangogh/world.h"

#include "common/util.h"

#include <math.h>

namespace Vangogh {

namespace {

const int kMaxGrid = 600;
const int kYawDead = 90;		// 4096 = 360 degrees
const int kPitchDead = 50;
const int kPitchMax = 500;		// the movement code's pitch limit

const float kAng = 4096.0f / (2.0f * (float)M_PI);

inline int signedAngle(int a) {
	return ((a + 2048) & 0xfff) - 2048;
}

}

bool World::nodeCentre(const char *name, float out[3]) const {
	const int n = _model.findNode(name);
	if (n < 0)
		return false;
	// World position = camera + V^T * (view-space position of the last render).
	float v[3];
	if (!_renderer.nodeViewCentroid(_model, n, v))
		_renderer.nodeViewPos(n, v);
	const float ang = 1.0f / kAng;
	const float a = (float)(_rotA & 0xfff) * ang, b = (float)(_rotB & 0xfff) * ang;
	const float sa = sinf(a), ca = cosf(a), sb = sinf(b), cb = cosf(b);
	const float V[9] = { cb, 0.0f, -sb, sa * sb, ca, cb * sa, sb * ca, -sa, cb * ca };
	for (int j = 0; j < 3; j++)
		out[j] = (float)_pos[j] + V[j] * v[0] + V[3 + j] * v[1] + V[6 + j] * v[2];
	return true;
}

bool World::planWalk(int tx, int tz, int radius, Common::Array<int16> &path, int minRadius) {
	path.clear();
	const CollisionMesh &mesh = _boxes[_box];
	if (!_boxLoaded[_box] || !mesh.size())
		return false;

	int32 lo[2] = { 0x7fffffff, 0x7fffffff }, hi[2] = { -0x7fffffff, -0x7fffffff };
	for (uint i = 0; i < mesh.size(); i++) {
		for (int k = 0; k < 3; k++) {
			lo[0] = MIN(lo[0], mesh.tri(i).v[k][0]); hi[0] = MAX(hi[0], mesh.tri(i).v[k][0]);
			lo[1] = MIN(lo[1], mesh.tri(i).v[k][2]); hi[1] = MAX(hi[1], mesh.tri(i).v[k][2]);
		}
	}
	// Small rooms (the chambre) are planned on a finer grid with a thinner sphere, or their gaps vanish between cells.
	const bool fine = (hi[0] - lo[0]) < 3000 && (hi[1] - lo[1]) < 3000;
	const int kGridStep = fine ? 50 : 100, kPlanRadius = fine ? 100 : 130;
	const int nx = (hi[0] - lo[0]) / kGridStep + 1, nz = (hi[1] - lo[1]) / kGridStep + 1;
	if (nx <= 0 || nz <= 0 || nx > kMaxGrid || nz > kMaxGrid)
		return false;

	const int cells = nx * nz;
	Common::Array<int32> parent(cells), cellY(cells);
	for (int i = 0; i < cells; i++)
		parent[i] = -2;
	Common::Array<uint32> queue;
	queue.reserve(cells);
	int sx = CLIP((_pos[0] - lo[0]) / kGridStep, 0, nx - 1), sz = CLIP((_pos[2] - lo[1]) / kGridStep, 0, nz - 1);
	int32 startY = _pos[1];
	{
		// A pose without floor under it (the pont arrival hangs just past the floor's edge): start from the nearest
		// cell that has floor, the first leg of the route crosses the gap.
		int32 fy;
		const int32 cam[3] = { lo[0] + sx * kGridStep, _pos[1], lo[1] + sz * kGridStep };
		if (!_collision.groundY(cam, _groundProbe, fy)) {
			bool found = false;
			for (int ring = 1; ring <= 8 && !found; ring++) {
				for (int dz = -ring; dz <= ring && !found; dz++) {
					for (int dx = -ring; dx <= ring && !found; dx++) {
						if (MAX(ABS(dx), ABS(dz)) != ring)
							continue;
						const int nxc = sx + dx, nzc = sz + dz;
						if (nxc < 0 || nzc < 0 || nxc >= nx || nzc >= nz)
							continue;
						const int32 c2[3] = { lo[0] + nxc * kGridStep, _pos[1], lo[1] + nzc * kGridStep };
						if (_collision.groundY(c2, _groundProbe, fy)) {
							sx = nxc;
							sz = nzc;
							startY = fy;
							found = true;
						}
					}
				}
			}
		} else {
			startY = fy;
		}
	}
	parent[sz * nx + sx] = -1;
	cellY[sz * nx + sx] = startY;
	queue.push_back(sz * nx + sx);
	int best = -1;
	const float r2 = (float)radius * (float)radius, m2 = (float)minRadius * (float)minRadius;
	for (uint head = 0; head < queue.size(); head++) {
		const int cx = queue[head] % nx, cz = queue[head] / nx;
		const float dx = (float)(lo[0] + cx * kGridStep - tx), dz = (float)(lo[1] + cz * kGridStep - tz);
		if (dx * dx + dz * dz <= r2 && dx * dx + dz * dz >= m2) {
			best = queue[head];
			break;		// breadth first: the first cell in range is the shortest route
		}
		static const int kDir[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		for (int d = 0; d < 4; d++) {
			const int mx = cx + kDir[d][0], mz = cz + kDir[d][1];
			if (mx < 0 || mz < 0 || mx >= nx || mz >= nz || parent[mz * nx + mx] != -2)
				continue;
			const int32 y = cellY[cz * nx + cx];
			const int32 to[3] = { lo[0] + mx * kGridStep, y, lo[1] + mz * kGridStep };
			int32 floorY;
			if (!_collision.groundY(to, _groundProbe, floorY))
				continue;
			int32 from[3] = { lo[0] + cx * kGridStep, y, lo[1] + cz * kGridStep };
			if (_collision.moveSphere(to, from, kPlanRadius))
				continue;
			parent[mz * nx + mx] = queue[head];
			cellY[mz * nx + mx] = floorY;
			queue.push_back(mz * nx + mx);
		}
	}
	if (best < 0)
		return false;

	// Cell chain start -> goal, then string-pulled: skip ahead while a straight sweep is clear.
	Common::Array<int32> chain;
	for (int c = best; c >= 0; c = parent[c])
		chain.insert_at(0, c);
	if (sx != CLIP((_pos[0] - lo[0]) / kGridStep, 0, nx - 1) || sz != CLIP((_pos[2] - lo[1]) / kGridStep, 0, nz - 1)) {
		path.push_back((int16)(lo[0] + sx * kGridStep));
		path.push_back((int16)(lo[1] + sz * kGridStep));
	}
	uint anchor = 0;
	while (anchor + 1 < chain.size()) {
		uint far = anchor + 1;
		for (uint j = chain.size() - 1; j > anchor + 1; j--) {
			const int32 from[3] = { lo[0] + (int)(chain[anchor] % nx) * kGridStep, cellY[chain[anchor]], lo[1] + (int)(chain[anchor] / nx) * kGridStep };
			const int32 to[3] = { lo[0] + (int)(chain[j] % nx) * kGridStep, cellY[chain[anchor]], lo[1] + (int)(chain[j] / nx) * kGridStep };
			int32 probe[3] = { from[0], from[1], from[2] };
			if (!_collision.moveSphere(to, probe, kPlanRadius)) {
				far = j;
				break;
			}
		}
		path.push_back((int16)(lo[0] + (int)(chain[far] % nx) * kGridStep));
		path.push_back((int16)(lo[1] + (int)(chain[far] / nx) * kGridStep));
		anchor = far;
	}
	return true;
}

bool World::steer(WorldInput &in, int tx, int tz, int tol) {
	in.up = in.down = in.left = in.right = in.pgUp = in.pgDown = in.alt = false;
	const float dx = (float)(tx - _pos[0]), dz = (float)(tz - _pos[2]);
	if (dx * dx + dz * dz <= (float)tol * (float)tol)
		return true;
	const int err = signedAngle((int)lroundf(atan2f(dx, dz) * kAng) - (_rotB & 0xfff));
	if (err > kYawDead)
		in.right = true;
	else if (err < -kYawDead)
		in.left = true;
	if (abs(err) < 400)
		in.up = true;
	return false;
}

bool World::turnTo(WorldInput &in, const float target[3]) {
	in.up = in.down = in.left = in.right = in.pgUp = in.pgDown = in.alt = false;
	const float d[3] = { target[0] - _pos[0], target[1] - _pos[1], target[2] - _pos[2] };
	const float dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
	if (dl < 1.0f)
		return true;
	const int wantYaw = (int)lroundf(atan2f(d[0], d[2]) * kAng);
	const int wantPitch = CLIP((int)lroundf(asinf(CLIP(-d[1] / dl, -1.0f, 1.0f)) * kAng), -kPitchMax, kPitchMax);
	const int eb = signedAngle(wantYaw - (_rotB & 0xfff));
	const int ea = wantPitch - _rotA;
	if (eb > kYawDead)
		in.right = true;
	else if (eb < -kYawDead)
		in.left = true;
	if (ea > kPitchDead)
		in.pgUp = true;
	else if (ea < -kPitchDead)
		in.pgDown = true;
	return !in.left && !in.right && !in.pgUp && !in.pgDown;
}

bool World::screenPointOf(const char *name, int &x, int &y) {
	const int n = _model.findNode(name);
	if (n < 0)
		return false;
	// The pointer must stay clear of the inventory bar and the retour corner.
	int bestD = 0x7fffffff;
	for (int step = 8; step >= 2 && bestD == 0x7fffffff; step /= 2) {
		for (int py = 10; py < 400; py += step) {
			for (int px = 4; px < 636; px += step) {
				const int d = (px - 320) * (px - 320) + (py - 240) * (py - 240);
				if (d >= bestD || _renderer.pick(_model, px, py, nullptr) != n)
					continue;
				bestD = d;
				x = px;
				y = py;
			}
		}
	}
	if (bestD != 0x7fffffff && _carry != kCarryNone) {
		// Carrying an object: the exe picks at the middle of the carried sprite, so the pointer sits up-left of the target.
		const UISprite &c = _ext[_carry - kNumCursors];
		x = MAX(0, x - c.width() / 2);
		y = MAX(0, y - c.height() / 2);
	}
	return bestD != 0x7fffffff;
}

} // End of namespace Vangogh
