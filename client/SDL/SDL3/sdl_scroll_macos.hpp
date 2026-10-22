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
#pragma once

class SdlContext;

/* SDL drops the gesture and momentum phases of macOS scroll events, which a macOS server needs
 * to scroll like a local trackpad. This reads the NSEvents directly and sends them in place of
 * SDL's wheel events. */
#if defined(__APPLE__)
bool sdl_scroll_monitor_install(SdlContext* sdl);
void sdl_scroll_monitor_uninstall();
bool sdl_scroll_monitor_active();
#else
static inline bool sdl_scroll_monitor_install(SdlContext*)
{
	return false;
}
static inline void sdl_scroll_monitor_uninstall()
{
}
static inline bool sdl_scroll_monitor_active()
{
	return false;
}
#endif
