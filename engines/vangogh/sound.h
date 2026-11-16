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

#ifndef VANGOGH_SOUND_H
#define VANGOGH_SOUND_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/str.h"

namespace Vangogh {

/**
 * The original Snd_* layer (assets.md sec.1): static WAV effects, plus one
 * streamed voice ('!' APC) or streamed WAV ('_') at a time. Handles are
 * 1-based; 0 means "failed to load".
 */
class Sound {
public:
	Sound();
	~Sound();

	/**
	 * Name prefix selects the class: plain = static WAV sfx, '!' = APC voice
	 * stream, '_' = streamed WAV. Files live in data/sound/<name>.wav|apc.
	 * Loading the same name twice returns the same handle. Returns 0 on error.
	 */
	uint16 load(const Common::String &name);

	/** Starts (or restarts) a handle. Starting a stream stops the previous one. */
	void play(uint16 handle, bool loop);
	void stop(uint16 handle);
	void stopAll();
	bool isPlaying(uint16 handle) const;

	/** Original global volume 0..100 (Snd_SetVolumePercent: (p-100)*50 hundredths of dB). */
	void setVolumePercent(int percent);

	/** Length in milliseconds of the sound behind a handle (0 if unknown). */
	uint32 durationMs(uint16 handle) const;

private:
	enum Kind { kStatic, kApc, kWavStream };
	struct Entry {
		Common::String name;
		Kind kind;
		byte *data;       // kStatic only: whole WAV file
		uint32 size;
		uint32 lengthMs;
		Audio::SoundHandle handle;
	};

	enum { kMaxHandles = 512 };

	Audio::Mixer *_mixer;
	Common::Array<Entry> _entries;
	int _volume;       // mixer channel volume 0..255
	uint16 _stream;    // handle of the current voice/'_' stream, 0 = none

	void applyVolume(Entry &e);
};

} // End of namespace Vangogh

#endif // VANGOGH_SOUND_H
