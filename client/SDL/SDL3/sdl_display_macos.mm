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

#include <algorithm>
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
			{
				const CGFloat safe = screen.safeAreaInsets.top;
				if (safe <= 0.0)
					return 0.0f;
				/* Full screen windows start below the menu bar, which is a point taller than the
				 * housing: 949 of 982 points on a 14" MacBook Pro, not 950. */
				const CGFloat menu = NSMaxY(screen.frame) - NSMaxY(screen.visibleFrame);
				return static_cast<float>(std::max(safe, menu));
			}
			return 0.0f;
		}
	}
	return 0.0f;
}
