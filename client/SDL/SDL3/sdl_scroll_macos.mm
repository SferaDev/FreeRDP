/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - native macOS scrolling
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

#include <freerdp/config.h>

#import <AppKit/AppKit.h>

#include <freerdp/channels/ainput.h>

#include "sdl_scroll_macos.hpp"
#include "sdl_context.hpp"
#include "sdl_prefs.hpp"
#include "sdl_touch.hpp"

namespace
{
	id monitor = nil;
	/* A gesture or its momentum that began over a session window; its events keep going to the
	 * server when the pointer leaves, so the server always sees it end. */
	bool gestureOpen = false;

	UINT64 phaseBits(NSEventPhase phase)
	{
		switch (phase)
		{
			case NSEventPhaseBegan:
				return AINPUT_SCROLL_PHASE_BEGAN;
			case NSEventPhaseStationary:
			case NSEventPhaseChanged:
				return AINPUT_SCROLL_PHASE_CHANGED;
			case NSEventPhaseEnded:
				return AINPUT_SCROLL_PHASE_ENDED;
			case NSEventPhaseCancelled:
				return AINPUT_SCROLL_PHASE_CANCELLED;
			case NSEventPhaseMayBegin:
				return AINPUT_SCROLL_PHASE_MAY_BEGIN;
			default:
				return AINPUT_SCROLL_PHASE_NONE;
		}
	}

	UINT64 momentumBits(NSEventPhase phase)
	{
		switch (phase)
		{
			case NSEventPhaseBegan:
				return AINPUT_SCROLL_MOMENTUM_BEGAN;
			case NSEventPhaseStationary:
			case NSEventPhaseChanged:
				return AINPUT_SCROLL_MOMENTUM_CHANGED;
			case NSEventPhaseEnded:
			case NSEventPhaseCancelled:
				return AINPUT_SCROLL_MOMENTUM_ENDED;
			default:
				return AINPUT_SCROLL_MOMENTUM_NONE;
		}
	}

	bool overSession(SdlContext* sdl, NSWindow* window)
	{
		if (!window)
			return false;

		int count = 0;
		SDL_Window** windows = SDL_GetWindows(&count);
		if (!windows)
			return false;

		bool found = false;
		for (int i = 0; i < count; i++)
		{
			const SDL_PropertiesID props = SDL_GetWindowProperties(windows[i]);
			if (SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr) ==
			    (__bridge void*)window)
			{
				found = sdl->isSessionWindow(SDL_GetWindowID(windows[i]));
				break;
			}
		}
		SDL_free(windows);
		return found;
	}

	void forward(SdlContext* sdl, NSEvent* event)
	{
		const UINT64 phase = phaseBits(event.phase);
		const UINT64 momentum = momentumBits(event.momentumPhase);
		const bool gesture =
		    (phase != AINPUT_SCROLL_PHASE_NONE) || (momentum != AINPUT_SCROLL_MOMENTUM_NONE);

		if (!overSession(sdl, event.window) && !(gesture && gestureOpen))
			return;

		if ((phase == AINPUT_SCROLL_PHASE_ENDED) || (phase == AINPUT_SCROLL_PHASE_CANCELLED) ||
		    (momentum == AINPUT_SCROLL_MOMENTUM_ENDED))
			gestureOpen = false;
		else if (gesture)
			gestureOpen = true;

		/* Precise deltas are points, as a local scroll view applies them; the rest are lines.
		 * A macOS server posts 100 pixels per 120-unit notch, so 1.2 units keep a point a point. */
		const bool precise = event.hasPreciseScrollingDeltas;
		static const double speed = SdlPref::instance()->get_int("ScrollSpeedPercent", 100) / 100.0;
		const double unit = (precise ? 1.2 : 120.0) * speed;
		double x = -(precise ? event.scrollingDeltaX : event.deltaX) * unit;
		double y = (precise ? event.scrollingDeltaY : event.deltaY) * unit;
		if (event.directionInvertedFromDevice &&
		    !SdlPref::instance()->get_bool("UseLocalMouseScrollDirection"))
		{
			x = -x;
			y = -y;
		}

		if (!gesture && (x == 0.0) && (y == 0.0))
			return;

		UINT64 ext = AINPUT_FLAGS_SCROLL_EXT | (phase << AINPUT_FLAGS_SCROLL_PHASE_SHIFT) |
		             (momentum << AINPUT_FLAGS_SCROLL_MOMENTUM_SHIFT);
		if (precise)
			ext |= AINPUT_FLAGS_SCROLL_PRECISE;

		if (!SdlTouch::sendScroll(sdl, x, y, ext))
			WLog_Print(sdl->getWLog(), WLOG_WARN, "failed to send a scroll event");
	}
} // namespace

bool sdl_scroll_monitor_install(SdlContext* sdl)
{
	if (monitor)
		return true;

	monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
	                                                 handler:^NSEvent*(NSEvent* event) {
		                                                 forward(sdl, event);
		                                                 return event;
	                                                 }];
	return monitor != nil;
}

void sdl_scroll_monitor_uninstall()
{
	if (!monitor)
		return;
	[NSEvent removeMonitor:monitor];
	monitor = nil;
	gestureOpen = false;
}

bool sdl_scroll_monitor_active()
{
	return monitor != nil;
}
