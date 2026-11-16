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

#include "vangogh/movietable.h"

namespace Vangogh {

static const MovieEntry kMovieTable[] = {
	{ "A01_032a", true, 112, 29, 528, 451 },
	{ "A01_032b", true, 528, 0, 91, 450 },
	{ "A01_032c", true, 481, 0, 154, 479 },
	{ "A01_032d", true, 543, 0, 58, 459 },
	{ "A01_032e", true, 523, 0, 101, 479 },
	{ "A01_032f", true, 492, 0, 146, 479 },
	{ "A01_032l", true, 112, 29, 528, 451 },
	{ "A03_012a", false, 112, 29, 479, 387 },
	{ "A03_012b", false, 112, 29, 479, 387 },
	{ "A03_012c", false, 112, 29, 479, 387 },
	{ "A03_012d", false, 112, 29, 479, 387 },
	{ "A03_012e", false, 112, 29, 479, 387 },
	{ "A03_012f", false, 112, 29, 479, 387 },
	{ "A03_012g", false, 112, 29, 479, 387 },
	{ "A03_012h", false, 112, 29, 479, 387 },
	{ "A03_013", false, 112, 29, 479, 387 },
	{ "A03_042a", true, 112, 29, 479, 381 },
	{ "A03_042b", true, 112, 29, 479, 381 },
	{ "A03_042c", true, 112, 29, 479, 381 },
	{ "A03_042d", true, 112, 29, 479, 381 },
	{ "A13_051", false, 158, 29, 325, 400 },
	{ "A13_08", false, 161, 29, 317, 400 },
	{ "A13_052a", true, 254, 29, 122, 365 },
	{ "A13_052b", true, 198, 75, 252, 319 },
	{ "A13_052c", true, 198, 75, 252, 319 },
	{ "A13_052d", true, 198, 75, 252, 319 },
	{ "A13_052e", true, 158, 29, 325, 400 },
	{ "A03_06a", false, 112, 29, 479, 399 },
	{ "A14_02", false, 112, 29, 479, 387 },
	{ "A01_02", false, 112, 29, 479, 260 },
	{ "A03_021", false, 112, 29, 479, 382 },
	{ "A03_022", false, 112, 29, 479, 382 },
	{ "A14_031a", true, 0, 29, 475, 451 },
	{ "A14_031b", true, 0, 29, 640, 451 },
	{ "A14_031c", true, 0, 29, 640, 451 },
	{ "A04_02", false, 112, 29, 479, 241 },
	{ "A04_03", false, 160, 29, 321, 400 },
	{ "A13_011", false, 112, 29, 479, 382 },
	{ "A13_012", false, 112, 29, 479, 382 },
	{ "A03_023a", true, 112, 29, 528, 382 },
	{ "A03_023b", true, 112, 29, 528, 382 },
	{ "A03_023c", true, 112, 29, 528, 382 },
	{ "A03_023d", true, 112, 29, 528, 382 },
	{ "A03_023e", true, 112, 29, 528, 382 },
	{ "A03_023f", true, 112, 29, 528, 382 },
	{ "A03_023g", true, 112, 29, 528, 382 },
	{ "A03_023h", true, 112, 29, 528, 382 },
	{ "A03_023i", true, 112, 29, 528, 382 },
	{ "A03_023j", true, 112, 29, 528, 382 },
	{ "A03_023k", true, 112, 29, 528, 382 },
	{ "A03_023l", true, 112, 29, 528, 382 },
	{ "A03_05a", false, 158, 29, 322, 400 },
	{ "A03_05b", false, 158, 29, 322, 400 },
	{ "A03_05c", true, 158, 29, 322, 400 },
	{ "A14_01a", false, 153, 29, 335, 400 },
	{ "A13_061a", true, 156, 29, 328, 400 },
	{ "A13_061b", true, 491, 0, 149, 426 },
	{ "A14_02a", true, 569, 0, 65, 400 },
	{ "A13_07", false, 164, 29, 312, 400 },
	{ "A13_02", false, 112, 29, 479, 301 },
	{ "A11_01", false, 112, 29, 479, 368 },
	{ "A13_04", false, 112, 29, 479, 383 },
	{ "A03_03", false, 112, 29, 479, 376 },
	{ "A13_09", false, 159, 29, 323, 400 },
	{ "A14_032a", false, 0, 29, 640, 451 },
	{ "A14_032b", true, 0, 0, 640, 480 },
	{ "A13_062", false, 156, 29, 328, 400 },
	{ "A04_012", false, 112, 29, 479, 241 },
};

const MovieEntry *findMovie(const Common::String &name) {
	for (uint i = 0; i < ARRAYSIZE(kMovieTable); i++) {
		if (name.equalsIgnoreCase(kMovieTable[i].name))
			return &kMovieTable[i];
	}
	return nullptr;
}

const MovieEntry *movieByIndex(uint index) {
	return index < ARRAYSIZE(kMovieTable) ? &kMovieTable[index] : nullptr;
}

} // End of namespace Vangogh
