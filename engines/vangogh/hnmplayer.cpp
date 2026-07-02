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

#include "vangogh/hnmplayer.h"

#include "audio/mixer.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "video/hnm_decoder.h"
#include "video/video_decoder.h"

namespace Vangogh {

HNMPlayer::HNMPlayer() : _decoder(nullptr), _currentSurface(nullptr) {
}

HNMPlayer::~HNMPlayer() {
	close();
}

void HNMPlayer::close() {
	delete _decoder;
	_decoder = nullptr;
	_currentSurface = nullptr;
}

bool HNMPlayer::load(const Common::Path &path, bool loop) {
	close();

	// HNM6 has no palette of its own; the codec draws via the given
	// PixelFormat's RGBToColor(), so matching the screen format (rather
	// than the header's informational bpp=16 field) is all that's needed.
	// Embedded APC audio, if any, is entirely handled by HNMDecoder's own
	// APCAudioTrack once setSoundType()/loadFile()/start() are called, in
	// that order -- mirrors engines/cryomni3d CryOmni3DEngine::playHNM().
	Video::VideoDecoder *decoder = new Video::HNMDecoder(g_system->getScreenFormat(), loop, nullptr);
	decoder->setSoundType(Audio::Mixer::kMusicSoundType);

	if (!decoder->loadFile(path)) {
		warning("Vangogh: could not open movie %s", path.toString().c_str());
		delete decoder;
		return false;
	}

	decoder->start();
	_decoder = decoder;
	_currentSurface = nullptr;
	return true;
}

bool HNMPlayer::decodeNextFrame() {
	if (!_decoder || !_decoder->needsUpdate())
		return false;

	const Graphics::Surface *frame = _decoder->decodeNextFrame();
	if (!frame)
		return false;

	_currentSurface = frame;
	return true;
}

bool HNMPlayer::endOfVideo() const {
	return !_decoder || _decoder->endOfVideo();
}

uint16 HNMPlayer::width() const {
	return _decoder ? _decoder->getWidth() : 0;
}

uint16 HNMPlayer::height() const {
	return _decoder ? _decoder->getHeight() : 0;
}

int HNMPlayer::getCurFrame() const {
	return _decoder ? _decoder->getCurFrame() : -1;
}

uint32 HNMPlayer::getFrameCount() const {
	return _decoder ? _decoder->getFrameCount() : 0;
}

} // End of namespace Vangogh
