/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef FREERDP_CHANNEL_AINPUT_H
#define FREERDP_CHANNEL_AINPUT_H

#include <freerdp/api.h>
#include <freerdp/dvc.h>
#include <freerdp/types.h>

#define AINPUT_CHANNEL_NAME "ainput"
#define AINPUT_DVC_CHANNEL_NAME "FreeRDP::Advanced::Input"

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum
	{
		MSG_AINPUT_VERSION = 0x01,
		MSG_AINPUT_MOUSE = 0x02
	} eAInputMsgType;

	typedef enum
	{
		AINPUT_FLAGS_WHEEL = 0x0001,
		AINPUT_FLAGS_MOVE = 0x0004,
		AINPUT_FLAGS_DOWN = 0x0008,

		AINPUT_FLAGS_REL = 0x0010,
		AINPUT_FLAGS_HAVE_REL = 0x0020,

		/* Pointer Flags */
		AINPUT_FLAGS_BUTTON1 = 0x1000, /* left */
		AINPUT_FLAGS_BUTTON2 = 0x2000, /* right */
		AINPUT_FLAGS_BUTTON3 = 0x4000, /* middle */

		/* Extended Pointer Flags */
		AINPUT_XFLAGS_BUTTON1 = 0x0100,
		AINPUT_XFLAGS_BUTTON2 = 0x0200
	} AInputEventFlags;

/* Scroll gesture extension, above the 32 bits of AInputEventFlags. With AINPUT_FLAGS_WHEEL, a
 * server that knows it can replay a trackpad gesture with its phases; others ignore the bits. */
#define AINPUT_FLAGS_SCROLL_EXT UINT64_C(0x100000000)
/* Continuous deltas from a trackpad or high-resolution wheel rather than wheel notches. */
#define AINPUT_FLAGS_SCROLL_PRECISE UINT64_C(0x200000000)
#define AINPUT_FLAGS_SCROLL_PHASE_SHIFT 36
#define AINPUT_FLAGS_SCROLL_MOMENTUM_SHIFT 40

	typedef enum
	{
		AINPUT_SCROLL_PHASE_NONE = 0,
		AINPUT_SCROLL_PHASE_BEGAN = 1,
		AINPUT_SCROLL_PHASE_CHANGED = 2,
		AINPUT_SCROLL_PHASE_ENDED = 3,
		AINPUT_SCROLL_PHASE_CANCELLED = 4,
		AINPUT_SCROLL_PHASE_MAY_BEGIN = 5
	} AInputScrollPhase;

	typedef enum
	{
		AINPUT_SCROLL_MOMENTUM_NONE = 0,
		AINPUT_SCROLL_MOMENTUM_BEGAN = 1,
		AINPUT_SCROLL_MOMENTUM_CHANGED = 2,
		AINPUT_SCROLL_MOMENTUM_ENDED = 3
	} AInputScrollMomentum;

	typedef struct ainput_client_context AInputClientContext;

#define AINPUT_VERSION_MAJOR 1
#define AINPUT_VERSION_MINOR 0

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_AINPUT_H */
