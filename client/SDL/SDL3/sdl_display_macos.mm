/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - macOS display geometry
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

#include <cmath>

#import <AppKit/AppKit.h>

#include "sdl_display_macos.hpp"

float sdl_display_top_inset(const SDL_Rect& bounds)
{
	@autoreleasepool
	{
		/* SDL display bounds are CGDisplayBounds, which NSScreen does not use. */
		for (NSScreen* screen in [NSScreen screens])
		{
			NSNumber* number = screen.deviceDescription[@"NSScreenNumber"];
			if (!number)
				continue;
			const CGRect rect = CGDisplayBounds(number.unsignedIntValue);
			if ((std::lround(rect.origin.x) != bounds.x) ||
			    (std::lround(rect.origin.y) != bounds.y))
				continue;
			if (@available(macOS 12.0, *))
				return static_cast<float>(screen.safeAreaInsets.top);
			return 0.0f;
		}
	}
	return 0.0f;
}
