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

#include "vangogh/sound.h"
#include "audio/audiostream.h"
#include "audio/decoders/apc.h"
#include "audio/decoders/wave.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/memstream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include <math.h>

namespace Vangogh {

Sound::Sound() : _mixer(g_system->getMixer()), _volume(Audio::Mixer::kMaxChannelVolume), _stream(0) {
	_entries.reserve(kMaxHandles);
}

Sound::~Sound() {
	stopAll();
	for (uint i = 0; i < _entries.size(); i++)
		delete[] _entries[i].data;
}

static Common::String soundPath(const Common::String &base, const char *ext) {
	return Common::String::format("sound/%s.%s", base.c_str(), ext);
}

uint16 Sound::load(const Common::String &name) {
	if (name.empty())
		return 0;
	for (uint i = 0; i < _entries.size(); i++)
		if (_entries[i].name == name)
			return i + 1;
	if (_entries.size() >= kMaxHandles) {
		warning("Vangogh: sound handle table full");
		return 0;
	}

	Entry e;
	e.name = name;
	e.data = nullptr;
	e.size = 0;
	e.lengthMs = 0;
	e.kind = name[0] == '!' ? kApc : (name[0] == '_' ? kWavStream : kStatic);
	const Common::String base = e.kind == kStatic ? name : name.substr(1);

	Common::File f;
	if (!f.open(Common::Path(soundPath(base, e.kind == kApc ? "apc" : "wav"), '/'))) {
		warning("Vangogh: sound '%s' not found", name.c_str());
		return 0;
	}

	if (e.kind == kApc) {
		byte hdr[32];
		if (f.read(hdr, 32) != 32 || memcmp(hdr, "CRYO_APC1.20", 12) != 0) {
			warning("Vangogh: bad APC '%s'", name.c_str());
			return 0;
		}
		const uint32 samples = READ_LE_UINT32(hdr + 12), rate = READ_LE_UINT32(hdr + 16);
		e.lengthMs = rate ? (uint32)((uint64)samples * 1000 / rate) : 0;
	} else {
		const uint32 size = f.size();
		byte *buf = new byte[size];
		if (e.kind == kStatic) {
			f.seek(0);
			if (f.read(buf, size) != size) {
				delete[] buf;
				warning("Vangogh: short read on '%s'", name.c_str());
				return 0;
			}
			e.data = buf;
			e.size = size;
			buf = nullptr;
		}
		delete[] buf;
		f.seek(0);
		Audio::SeekableAudioStream *s = Audio::makeWAVStream(f.readStream(size), DisposeAfterUse::YES);
		if (!s) {
			delete[] e.data;
			warning("Vangogh: bad WAV '%s'", name.c_str());
			return 0;
		}
		e.lengthMs = s->getLength().msecs();
		delete s;
	}

	_entries.push_back(e);
	return _entries.size();
}

void Sound::applyVolume(Entry &e) {
	if (_mixer->isSoundHandleActive(e.handle))
		_mixer->setChannelVolume(e.handle, _volume);
}

void Sound::play(uint16 handle, bool loop) {
	if (handle == 0 || handle > _entries.size())
		return;
	Entry &e = _entries[handle - 1];

	if (e.kind != kStatic) {
		// One voice/stream at a time (Snd_StopStream)
		if (_stream)
			stop(_stream);
		_stream = handle;
	} else {
		_mixer->stopHandle(e.handle);
	}

	Audio::AudioStream *as = nullptr;
	Audio::Mixer::SoundType type = Audio::Mixer::kSFXSoundType;
	if (e.kind == kApc) {
		type = Audio::Mixer::kSpeechSoundType;
		Common::File f;
		if (!f.open(Common::Path(soundPath(e.name.substr(1), "apc"), '/')))
			return;
		const uint32 size = f.size();
		Common::SeekableReadStream *hdr = f.readStream(32);
		Audio::PacketizedAudioStream *ps = Audio::makeAPCStream(*hdr);
		delete hdr;
		if (!ps)
			return;
		ps->queuePacket(f.readStream(size - 32));
		ps->finish();
		as = ps;
	} else {
		Common::SeekableReadStream *rs;
		if (e.kind == kStatic) {
			rs = new Common::MemoryReadStream(e.data, e.size, DisposeAfterUse::NO);
		} else {
			type = Audio::Mixer::kSpeechSoundType;
			Common::File *f = new Common::File;
			if (!f->open(Common::Path(soundPath(e.name.substr(1), "wav"), '/'))) {
				delete f;
				return;
			}
			rs = f;
		}
		Audio::SeekableAudioStream *ws = Audio::makeWAVStream(rs, DisposeAfterUse::YES);
		if (!ws)
			return;
		as = loop ? Audio::makeLoopingAudioStream(ws, 0) : (Audio::AudioStream *)ws;
	}

	_mixer->playStream(type, &e.handle, as, -1, _volume);
	debug(1, "Vangogh: sound play '%s' (%u ms)%s", e.name.c_str(), e.lengthMs, loop ? " loop" : "");
}

void Sound::stop(uint16 handle) {
	if (handle == 0 || handle > _entries.size())
		return;
	_mixer->stopHandle(_entries[handle - 1].handle);
	if (_stream == handle)
		_stream = 0;
}

void Sound::stopAll() {
	for (uint i = 0; i < _entries.size(); i++)
		_mixer->stopHandle(_entries[i].handle);
	_stream = 0;
}

bool Sound::isPlaying(uint16 handle) const {
	if (handle == 0 || handle > _entries.size())
		return false;
	return _mixer->isSoundHandleActive(_entries[handle - 1].handle);
}

uint32 Sound::durationMs(uint16 handle) const {
	return (handle == 0 || handle > _entries.size()) ? 0 : _entries[handle - 1].lengthMs;
}

void Sound::setVolumePercent(int percent) {
	// DirectSound attenuation in hundredths of dB: (p-100)*50 -> -50 dB..0 dB
	percent = CLIP(percent, 0, 100);
	const float db = (percent - 100) * 50 / 100.0f;
	_volume = percent == 0 ? 0 : (int)(Audio::Mixer::kMaxChannelVolume * powf(10.0f, db / 20.0f) + 0.5f);
	for (uint i = 0; i < _entries.size(); i++)
		applyVolume(_entries[i]);
}

} // End of namespace Vangogh
