/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - macOS pasteboard helpers
 *
 * Copyright 2026 Alexis Rico <sferadev@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/* NSPasteboard has no change notification, and SDL's Cocoa backend only compares the change
 * count when one of its windows becomes key. A copy made while the session window keeps the
 * focus (another process, Universal Clipboard, a clipboard manager) never reaches SDL, and
 * SDL's cached mime types keep describing what we last put there ourselves. */
namespace sdl::clip::macos
{
	/* The general pasteboard's change count, or the named pasteboard's. */
	[[nodiscard]] long changeCount(const char* pasteboard = nullptr);

	/* The mime types on the pasteboard, as SDL lists them: each type's preferred mime type
	 * followed by the type identifier itself. */
	[[nodiscard]] std::vector<std::string> mimeTypes(const char* pasteboard = nullptr);

	/* Content a password manager marked as not to be shared (nspasteboard.org). */
	[[nodiscard]] bool isConcealed(const std::vector<std::string>& mimeTypes);

	/* Calls notify on the main thread whenever the general pasteboard's change count moves,
	 * checked every interval while the run loop runs (SDL pumps it while waiting for events). */
	[[nodiscard]] bool startMonitor(double intervalSeconds, void (*notify)(void*), void* userdata);
	void stopMonitor();

	/* Decodes any image ImageIO reads (PNG, TIFF, JPEG, HEIC...) into a CF_DIB, or a CF_DIBV5
	 * when v5 is set. Empty on failure. */
	[[nodiscard]] std::vector<uint8_t> imageToDib(const void* data, size_t size, bool v5);

	/* Builds a 32 bpp bottom-up CF_DIB or CF_DIBV5 from top-down BGRA rows with straight alpha.
	 * Empty if the size does not fit the headers. */
	[[nodiscard]] std::vector<uint8_t> bgraToDib(const uint8_t* bgra, size_t width, size_t height,
	                                             size_t stride, bool v5);
} // namespace sdl::clip::macos
