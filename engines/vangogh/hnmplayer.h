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

#ifndef VANGOGH_HNMPLAYER_H
#define VANGOGH_HNMPLAYER_H

#include "common/path.h"
#include "common/scummsys.h"

namespace Graphics {
struct Surface;
}

namespace Video {
class VideoDecoder;
}

namespace Vangogh {

/**
 * Thin, non-blocking wrapper around Video::HNMDecoder, factored out of
 * VangoghEngine::playVideo() so the same HNM6+APC playback plumbing (see
 * hnm-apc-compat.md in the reversing notes -- embedded APC audio needs no
 * engine-specific decoding work, Video::HNMDecoder handles it once
 * setSoundType()/loadFile()/start() are called, in that order) can drive
 * both a one-shot, skippable cutscene (playVideo()) and a looping scene
 * backdrop (Scene) without duplicating decoder setup/tear-down.
 *
 * Unlike playVideo()'s old inline loop, HNMPlayer does not block or poll
 * events itself: callers own the per-frame timing/event loop and call
 * decodeNextFrame() once per tick, exactly like they'd call decodeNextFrame()
 * on the raw Video::VideoDecoder -- this class only hides construction/
 * teardown and the needsUpdate()-gated decode step.
 */
class HNMPlayer {
public:
	HNMPlayer();
	~HNMPlayer();

	HNMPlayer(const HNMPlayer &) = delete;
	HNMPlayer &operator=(const HNMPlayer &) = delete;

	/**
	 * Opens @p path as an HNM6 movie, decoding to the current screen's
	 * pixel format (see g_system->getScreenFormat()). @p loop selects
	 * Video::HNMDecoder's own looping mode: the video track then reports
	 * getFrameCount() == 0 and endOfVideo() never becomes true, restarting
	 * from the beginning of the file on its own once exhausted -- see
	 * video/hnm_decoder.h/.cpp. Any previously loaded movie is closed
	 * first. Returns false, after a warning(), if the file can't be
	 * opened.
	 */
	bool load(const Common::Path &path, bool loop);

	/** Frees the underlying decoder, as if freshly constructed. */
	void close();

	bool isLoaded() const { return _decoder != nullptr; }

	/**
	 * Decodes and presents the next frame if -- and only if -- one is due
	 * (Video::VideoDecoder::needsUpdate()); currentSurface() is updated in
	 * that case. Returns true exactly when a new frame was decoded, so
	 * callers know whether there's anything new to blit this tick. A no-op
	 * returning false if nothing is loaded.
	 */
	bool decodeNextFrame();

	/** The most recently decoded frame, or nullptr before the first decodeNextFrame(). */
	const Graphics::Surface *currentSurface() const { return _currentSurface; }

	/** True if nothing is loaded, or the loaded (non-looping) video has run out of frames. */
	bool endOfVideo() const;

	uint16 width() const;
	uint16 height() const;

	/** 0-based index of the most recently decoded frame, or -1 before the first decodeNextFrame(). */
	int getCurFrame() const;

	/** Total frame count, or 0 for a looping video (see load()) or if nothing is loaded. */
	uint32 getFrameCount() const;

private:
	Video::VideoDecoder *_decoder;
	const Graphics::Surface *_currentSurface;
};

} // End of namespace Vangogh

#endif // VANGOGH_HNMPLAYER_H
