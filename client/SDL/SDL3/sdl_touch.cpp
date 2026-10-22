/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP SDL touch/mouse input
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#include "sdl_touch.hpp"
#include "sdl_context.hpp"
#include "sdl_prefs.hpp"
#include "sdl_scroll_macos.hpp"

#include <algorithm>
#include <cmath>

#include <winpr/wtypes.h>
#include <winpr/assert.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/channels/ainput.h>

#include <SDL3/SDL.h>

[[nodiscard]] static bool send_mouse_wheel(SdlContext* sdl, UINT16 flags, INT32 avalue)
{
	WINPR_ASSERT(sdl);
	if (avalue < 0)
	{
		flags |= PTR_FLAGS_WHEEL_NEGATIVE;
		avalue = -avalue;
	}

	while (avalue > 0)
	{
		const UINT16 cval = (avalue > 0xFF) ? 0xFF : static_cast<UINT16>(avalue);
		UINT16 cflags = flags | cval;
		/* Convert negative values to 9bit twos complement */
		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			cflags = (flags & 0xFF00) | (0x100 - cval);
		if (!freerdp_client_send_wheel_event(sdl->common(), cflags))
			return FALSE;

		avalue -= cval;
	}
	return TRUE;
}

[[nodiscard]] static UINT32 sdl_scale_pressure(const float pressure)
{
	const float val = pressure * 0x400; /* [MS-RDPEI] 2.2.3.3.1.1 RDPINPUT_TOUCH_CONTACT */
	if (val < 0.0f)
		return 0;
	if (val > 0x400)
		return 0x400;
	return static_cast<UINT32>(val);
}

bool SdlTouch::touchUp(SdlContext* sdl, const SDL_TouchFingerEvent& ev)
{
	WINPR_ASSERT(sdl);

	return freerdp_client_handle_touch(sdl->common(), FREERDP_TOUCH_UP | FREERDP_TOUCH_HAS_PRESSURE,
	                                   static_cast<INT32>(ev.fingerID),
	                                   sdl_scale_pressure(ev.pressure), static_cast<Sint32>(ev.x),
	                                   static_cast<Sint32>(ev.y));
}

bool SdlTouch::touchCancel(SdlContext* sdl, const SDL_TouchFingerEvent& ev)
{
	WINPR_ASSERT(sdl);

	return freerdp_client_handle_touch(
	    sdl->common(), FREERDP_TOUCH_CANCEL | FREERDP_TOUCH_HAS_PRESSURE,
	    static_cast<INT32>(ev.fingerID), sdl_scale_pressure(ev.pressure), static_cast<Sint32>(ev.x),
	    static_cast<Sint32>(ev.y));
}

bool SdlTouch::touchDown(SdlContext* sdl, const SDL_TouchFingerEvent& ev)
{
	WINPR_ASSERT(sdl);

	return freerdp_client_handle_touch(
	    sdl->common(), FREERDP_TOUCH_DOWN | FREERDP_TOUCH_HAS_PRESSURE,
	    static_cast<INT32>(ev.fingerID), sdl_scale_pressure(ev.pressure), static_cast<Sint32>(ev.x),
	    static_cast<Sint32>(ev.y));
}

bool SdlTouch::touchMotion(SdlContext* sdl, const SDL_TouchFingerEvent& ev)
{
	WINPR_ASSERT(sdl);

	return freerdp_client_handle_touch(
	    sdl->common(), FREERDP_TOUCH_MOTION | FREERDP_TOUCH_HAS_PRESSURE,
	    static_cast<INT32>(ev.fingerID), sdl_scale_pressure(ev.pressure), static_cast<Sint32>(ev.x),
	    static_cast<Sint32>(ev.y));
}

bool SdlTouch::handleEvent(SdlContext* sdl, const SDL_MouseMotionEvent& ev)
{
	WINPR_ASSERT(sdl);

	if (!sdl->getInputChannelContext().mouse_focus(ev.windowID))
		return FALSE;

	const BOOL relative =
	    freerdp_client_use_relative_mouse_events(sdl->common()) && !sdl->hasCursor();
	auto x = static_cast<INT32>(relative ? ev.xrel : ev.x);
	auto y = static_cast<INT32>(relative ? ev.yrel : ev.y);
	return freerdp_client_send_button_event(sdl->common(), relative, PTR_FLAGS_MOVE, x, y);
}

bool SdlTouch::handleEvent(SdlContext* sdl, const SDL_MouseWheelEvent& ev)
{
	WINPR_ASSERT(sdl);

	if (sdl_scroll_monitor_active())
		return TRUE;

	const bool flipped = (ev.direction == SDL_MOUSEWHEEL_FLIPPED) &&
	                     !SdlPref::instance()->get_bool("UseLocalMouseScrollDirection");

	/* Trackpads and high-resolution wheels report pixel deltas that SDL scales to tenths of a
	 * line; only notched wheels send whole lines. A fraction marks a precise stream, which may
	 * still carry whole values, so keep treating it as precise for a moment. */
	static Uint64 lastPrecise = 0;
	const Uint64 now = SDL_GetTicks();
	if ((ev.x != SDL_truncf(ev.x)) || (ev.y != SDL_truncf(ev.y)))
		lastPrecise = now;
	const double unit = (lastPrecise != 0) && (now - lastPrecise < 500) ? 12.0 : 120.0;
	const double sign = flipped ? -1.0 : 1.0;

	return sendScroll(sdl, ev.x * sign * unit, ev.y * sign * unit, 0);
}

bool SdlTouch::sendScroll(SdlContext* sdl, double x, double y, UINT64 ext)
{
	WINPR_ASSERT(sdl);
	auto cctx = sdl->common();
	WINPR_ASSERT(cctx);

#if defined(CHANNEL_AINPUT_CLIENT)
	/* ainput carries 16.16 fixed point, so no fraction of a pixel is lost. */
	if (cctx->ainput)
	{
		if (freerdp_get_state(&cctx->context) != CONNECTION_STATE_ACTIVE)
			return TRUE;

		auto fixed = [](double v)
		{
			const double f = std::round(v * 0x10000);
			return static_cast<INT32>(std::clamp(f, static_cast<double>(INT32_MIN),
			                                     static_cast<double>(INT32_MAX)));
		};
		WINPR_ASSERT(cctx->ainput->AInputSendInputEvent);
		if (cctx->ainput->AInputSendInputEvent(cctx->ainput, AINPUT_FLAGS_WHEEL | ext, fixed(x),
		                                       fixed(y)) == CHANNEL_RC_OK)
			return TRUE;
	}
#endif

	/* Wheel PDUs carry whole units; keep the fractions for the next event. */
	static double restX = 0.0;
	static double restY = 0.0;
	auto whole = [](double& rest, double v)
	{
		if ((v != 0.0) && ((rest < 0.0) != (v < 0.0)))
			rest = 0.0;
		rest += v;
		const double w = std::trunc(rest);
		rest -= w;
		return static_cast<INT32>(w);
	};
	const INT32 wy = whole(restY, y);
	const INT32 wx = whole(restX, x);

	if ((wy != 0) && !send_mouse_wheel(sdl, PTR_FLAGS_WHEEL, wy))
		return FALSE;
	if ((wx != 0) && !send_mouse_wheel(sdl, PTR_FLAGS_HWHEEL, wx))
		return FALSE;
	return TRUE;
}

bool SdlTouch::handleEvent(SdlContext* sdl, const SDL_MouseButtonEvent& ev)
{
	UINT16 flags = 0;
	UINT16 xflags = 0;

	WINPR_ASSERT(sdl);

	if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
	{
		flags |= PTR_FLAGS_DOWN;
		xflags |= PTR_XFLAGS_DOWN;
	}

	switch (ev.button)
	{
		case 1:
			flags |= PTR_FLAGS_BUTTON1;
			break;
		case 2:
			flags |= PTR_FLAGS_BUTTON3;
			break;
		case 3:
			flags |= PTR_FLAGS_BUTTON2;
			break;
		case 4:
			xflags |= PTR_XFLAGS_BUTTON1;
			break;
		case 5:
			xflags |= PTR_XFLAGS_BUTTON2;
			break;
		default:
			break;
	}

	const BOOL relative =
	    freerdp_client_use_relative_mouse_events(sdl->common()) && !sdl->hasCursor();
	auto x = static_cast<INT32>(relative ? 0 : ev.x);
	auto y = static_cast<INT32>(relative ? 0 : ev.y);

	if ((flags & (~PTR_FLAGS_DOWN)) != 0)
		return freerdp_client_send_button_event(sdl->common(), relative, flags, x, y);
	else if ((xflags & (~PTR_XFLAGS_DOWN)) != 0)
		return freerdp_client_send_extended_button_event(sdl->common(), relative, xflags, x, y);
	else
		return TRUE;
}

bool SdlTouch::handleEvent(SdlContext* sdl, const SDL_TouchFingerEvent& ev)
{
	switch (ev.type)
	{
		case SDL_EVENT_FINGER_CANCELED:
			return SdlTouch::touchCancel(sdl, ev);
		case SDL_EVENT_FINGER_UP:
			return SdlTouch::touchUp(sdl, ev);
		case SDL_EVENT_FINGER_DOWN:
			return SdlTouch::touchDown(sdl, ev);
		case SDL_EVENT_FINGER_MOTION:
			return SdlTouch::touchMotion(sdl, ev);
		default:
			return false;
	}
}
