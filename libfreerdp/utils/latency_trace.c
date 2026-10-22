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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/environment.h>

#include <freerdp/utils/latency_trace.h>

#define LATENCY_SAMPLES 8192
#define LATENCY_REPORT_INTERVAL_NS (2ull * 1000ull * 1000ull * 1000ull)

typedef struct
{
	UINT32 us[LATENCY_SAMPLES];
	size_t count;
	UINT64 dropped;
} LatencyStage;

static INIT_ONCE initOnce = INIT_ONCE_STATIC_INIT;
static BOOL enabled = FALSE;
static CRITICAL_SECTION lock;
static LatencyStage* stages = NULL;
static UINT64 windowStart = 0;

static const char* stage_name(FreeRDPLatencyStage stage)
{
	switch (stage)
	{
		case FREERDP_LATENCY_GFX_FRAME:
			return "gfx.frame";
		case FREERDP_LATENCY_GFX_DECODE:
			return "gfx.decode";
		case FREERDP_LATENCY_GFX_END_FRAME:
			return "gfx.endframe";
		case FREERDP_LATENCY_GFX_ACK:
			return "gfx.start_to_ack";
		case FREERDP_LATENCY_UI_QUEUE:
			return "ui.queue";
		case FREERDP_LATENCY_UI_DRAW:
			return "ui.draw";
		case FREERDP_LATENCY_UI_PRESENT:
			return "ui.present";
		case FREERDP_LATENCY_UI_PACED:
			return "ui.paced";
		case FREERDP_LATENCY_UI_SIGNAL_TO_PRESENT:
			return "ui.signal_to_present";
		case FREERDP_LATENCY_INPUT:
			return "input.event_to_pdu";
		default:
			return "?";
	}
}

static BOOL CALLBACK latency_init(PINIT_ONCE once, PVOID param, PVOID* context)
{
	WINPR_UNUSED(once);
	WINPR_UNUSED(param);
	WINPR_UNUSED(context);

	char* env = GetEnvAlloc("FREERDP_LATENCY_TRACE");
	const BOOL on = env && (strcmp(env, "0") != 0) && (env[0] != '\0');
	free(env);
	if (!on)
		return TRUE;

	stages = calloc(FREERDP_LATENCY_STAGE_COUNT, sizeof(LatencyStage));
	if (!stages)
		return TRUE;
	if (!InitializeCriticalSectionAndSpinCount(&lock, 4000))
	{
		free(stages);
		stages = NULL;
		return TRUE;
	}
	windowStart = winpr_GetTickCount64NS();
	enabled = TRUE;
	(void)fprintf(stderr, "[latency] tracing enabled, summaries every 2 s (ms)\n");
	return TRUE;
}

BOOL freerdp_latency_trace_enabled(void)
{
	(void)InitOnceExecuteOnce(&initOnce, latency_init, NULL, NULL);
	return enabled;
}

UINT64 freerdp_latency_trace_now(void)
{
	return winpr_GetTickCount64NS();
}

static int cmp_u32(const void* a, const void* b)
{
	const UINT32 x = *(const UINT32*)a;
	const UINT32 y = *(const UINT32*)b;
	return (x > y) - (x < y);
}

static void report_locked(UINT64 now)
{
	const double secs = (double)(now - windowStart) / 1e9;
	(void)fprintf(stderr, "[latency] --- %.1f s window ---\n", secs);
	for (size_t i = 0; i < FREERDP_LATENCY_STAGE_COUNT; i++)
	{
		LatencyStage* st = &stages[i];
		if (st->count == 0)
			continue;
		qsort(st->us, st->count, sizeof(UINT32), cmp_u32);
		const UINT32 p50 = st->us[st->count / 2];
		const UINT32 p95 = st->us[(st->count * 95) / 100];
		const UINT32 max = st->us[st->count - 1];
		(void)fprintf(stderr,
		              "[latency] %-22s n=%-5zu %6.1f/s  p50=%7.2f  p95=%7.2f  max=%7.2f%s\n",
		              stage_name((FreeRDPLatencyStage)i), st->count, (double)st->count / secs,
		              p50 / 1000.0, p95 / 1000.0, max / 1000.0,
		              st->dropped ? "  (samples dropped)" : "");
		st->count = 0;
		st->dropped = 0;
	}
	(void)fflush(stderr);
	windowStart = now;
}

void freerdp_latency_trace_record(FreeRDPLatencyStage stage, UINT64 durationNs)
{
	if (!freerdp_latency_trace_enabled() || (stage >= FREERDP_LATENCY_STAGE_COUNT))
		return;

	const UINT64 us = durationNs / 1000ull;
	const UINT64 now = winpr_GetTickCount64NS();

	EnterCriticalSection(&lock);
	LatencyStage* st = &stages[stage];
	if (st->count < LATENCY_SAMPLES)
		st->us[st->count++] = (UINT32)MIN(us, UINT32_MAX);
	else
		st->dropped++;
	if (now - windowStart >= LATENCY_REPORT_INTERVAL_NS)
		report_locked(now);
	LeaveCriticalSection(&lock);
}

void freerdp_latency_trace_since(FreeRDPLatencyStage stage, UINT64 startNs)
{
	if ((startNs == 0) || !freerdp_latency_trace_enabled())
		return;
	const UINT64 now = winpr_GetTickCount64NS();
	if (now >= startNs)
		freerdp_latency_trace_record(stage, now - startNs);
}
