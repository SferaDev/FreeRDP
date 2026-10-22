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
#pragma once

#include <SDL3/SDL.h>

#if defined(__APPLE__)
/* The height in points of the strip a camera housing takes at the top of the display with the
 * given SDL bounds, which full screen windows stay below. */
float sdl_display_top_inset(const SDL_Rect& bounds);
#endif
