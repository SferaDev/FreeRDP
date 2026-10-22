/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Latency trace
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

#ifndef FREERDP_UTILS_LATENCY_TRACE_H
#define FREERDP_UTILS_LATENCY_TRACE_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** Stages timed when FREERDP_LATENCY_TRACE=1; summaries go to stderr every 2 seconds. */
	typedef enum
	{
		FREERDP_LATENCY_GFX_FRAME,       /** StartFrame received to EndFrame received */
		FREERDP_LATENCY_GFX_DECODE,      /** one surface command */
		FREERDP_LATENCY_GFX_END_FRAME,   /** EndFrame: copy to the primary buffer, signal the UI */
		FREERDP_LATENCY_GFX_ACK,         /** StartFrame received to FrameAcknowledge queued */
		FREERDP_LATENCY_UI_QUEUE,        /** damage signaled to the UI thread picking it up */
		FREERDP_LATENCY_UI_DRAW,         /** UI thread drawing all windows once */
		FREERDP_LATENCY_UI_PRESENT,      /** one window's draw and present call */
		FREERDP_LATENCY_UI_PACED,        /** present deferred to the window's next refresh */
		FREERDP_LATENCY_UI_SIGNAL_TO_PRESENT, /** damage signaled to presented */
		FREERDP_LATENCY_INPUT,           /** OS input event timestamp to input PDU written */
		FREERDP_LATENCY_STAGE_COUNT
	} FreeRDPLatencyStage;

	FREERDP_API BOOL freerdp_latency_trace_enabled(void);

	/** Monotonic nanoseconds, comparable across threads. */
	FREERDP_API UINT64 freerdp_latency_trace_now(void);

	FREERDP_API void freerdp_latency_trace_record(FreeRDPLatencyStage stage, UINT64 durationNs);

	/** Records now - startNs when tracing is enabled and startNs is set. */
	FREERDP_API void freerdp_latency_trace_since(FreeRDPLatencyStage stage, UINT64 startNs);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_LATENCY_TRACE_H */
