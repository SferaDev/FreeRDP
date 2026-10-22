#ifndef _WIN32
#include <errno.h>

#include "pollset.h"
#include <winpr/handle.h>
#include <winpr/sysinfo.h>
#include <winpr/assert.h>
#include "../log.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#if defined(__APPLE__) && defined(WINPR_HAVE_POLL_H)
#include <sys/select.h>
#endif

#define TAG WINPR_TAG("sync.pollset")

#ifdef WINPR_HAVE_POLL_H
static INT16 handle_mode_to_pollevent(ULONG mode)
{
	INT16 event = 0;

	if (mode & WINPR_FD_READ)
		event |= POLLIN;

	if (mode & WINPR_FD_WRITE)
		event |= POLLOUT;

	return event;
}
#endif

BOOL pollset_init(WINPR_POLL_SET* set, size_t nhandles)
{
	WINPR_ASSERT(set);
#ifdef WINPR_HAVE_POLL_H
	if (nhandles > MAXIMUM_WAIT_OBJECTS)
	{
		set->isStatic = FALSE;
		set->pollset = calloc(nhandles, sizeof(*set->pollset));
		if (!set->pollset)
			return FALSE;
	}
	else
	{
		set->pollset = set->staticSet;
		set->isStatic = TRUE;
	}
#else
	set->fdIndex = calloc(nhandles, sizeof(*set->fdIndex));
	if (!set->fdIndex)
		return FALSE;

	FD_ZERO(&set->rset_base);
	FD_ZERO(&set->rset);
	FD_ZERO(&set->wset_base);
	FD_ZERO(&set->wset);
	set->maxFd = 0;
	set->nread = set->nwrite = 0;
#endif

	set->size = nhandles;
	set->fillIndex = 0;
	return TRUE;
}

void pollset_uninit(WINPR_POLL_SET* set)
{
	WINPR_ASSERT(set);
#ifdef WINPR_HAVE_POLL_H
	if (!set->isStatic)
		free(set->pollset);
#else
	free(set->fdIndex);
#endif
}

void pollset_reset(WINPR_POLL_SET* set)
{
	WINPR_ASSERT(set);
#ifndef WINPR_HAVE_POLL_H
	FD_ZERO(&set->rset_base);
	FD_ZERO(&set->wset_base);
	set->maxFd = 0;
	set->nread = set->nwrite = 0;
#endif
	set->fillIndex = 0;
}

BOOL pollset_add(WINPR_POLL_SET* set, int fd, ULONG mode)
{
	WINPR_ASSERT(set);
#ifdef WINPR_HAVE_POLL_H
	struct pollfd* item = nullptr;
	if (set->fillIndex == set->size)
		return FALSE;

	item = &set->pollset[set->fillIndex];
	item->fd = fd;
	item->revents = 0;
	item->events = handle_mode_to_pollevent(mode);
#else
	FdIndex* fdIndex = &set->fdIndex[set->fillIndex];
	if (mode & WINPR_FD_READ)
	{
		FD_SET(fd, &set->rset_base);
		set->nread++;
	}

	if (mode & WINPR_FD_WRITE)
	{
		FD_SET(fd, &set->wset_base);
		set->nwrite++;
	}

	if (fd > set->maxFd)
		set->maxFd = fd;

	fdIndex->fd = fd;
	fdIndex->mode = mode;
#endif
	set->fillIndex++;
	return TRUE;
}

#if defined(__APPLE__) && defined(WINPR_HAVE_POLL_H)
/* On macOS 27 a zero-timeout poll() on an fd that is not ready costs 80-200 us,
 * against well under 1 us for select(). Zero-timeout waits run several times per
 * received PDU, which capped the client at about 650 PDUs/s. */
static int pollset_select_now(WINPR_POLL_SET* set)
{
	fd_set rset;
	fd_set wset;
	int maxFd = -1;

	FD_ZERO(&rset);
	FD_ZERO(&wset);
	for (size_t i = 0; i < set->fillIndex; i++)
	{
		const struct pollfd* item = &set->pollset[i];
		if ((item->fd < 0) || (item->fd >= FD_SETSIZE))
			return -2;
		if (item->events & POLLIN)
			FD_SET(item->fd, &rset);
		if (item->events & POLLOUT)
			FD_SET(item->fd, &wset);
		if (item->fd > maxFd)
			maxFd = item->fd;
	}

	struct timeval zero = { 0 };
	int ret = 0;
	do
	{
		ret = select(maxFd + 1, &rset, &wset, nullptr, &zero);
	} while ((ret < 0) && (errno == EINTR));
	if (ret < 0)
		return -2;

	for (size_t i = 0; i < set->fillIndex; i++)
	{
		struct pollfd* item = &set->pollset[i];
		item->revents = 0;
		if ((item->events & POLLIN) && FD_ISSET(item->fd, &rset))
			item->revents |= POLLIN;
		if ((item->events & POLLOUT) && FD_ISSET(item->fd, &wset))
			item->revents |= POLLOUT;
	}
	return ret;
}
#endif

int pollset_poll(WINPR_POLL_SET* set, DWORD dwMilliseconds)
{
	WINPR_ASSERT(set);
	int ret = 0;
	UINT64 dueTime = 0;
	UINT64 now = 0;

	now = GetTickCount64();
	if (dwMilliseconds == INFINITE)
		dueTime = 0xFFFFFFFFFFFFFFFF;
	else
		dueTime = now + dwMilliseconds;

#ifdef WINPR_HAVE_POLL_H
	int timeout = 0;

#if defined(__APPLE__)
	if (dwMilliseconds == 0)
	{
		const int status = pollset_select_now(set);
		if (status != -2)
			return status;
	}
#endif

	do
	{
		if (dwMilliseconds == INFINITE)
			timeout = -1;
		else
			timeout = (int)(dueTime - now);

		ret = poll(set->pollset, WINPR_ASSERTING_INT_CAST(nfds_t, set->fillIndex), timeout);
		if (ret >= 0)
		{
#if defined(__EMSCRIPTEN__)
			/* If we have tried 10 times unsuccessfully we will yield in emscripten so pending event
			 * handlers might be run */
			if (ret == 0)
			{
				if (++set->yieldCounter > 10)
				{
					emscripten_sleep(0);
					set->yieldCounter = 0;
				}
			}
			else
				set->yieldCounter = 0;
#endif
			return ret;
		}

		if (errno != EINTR)
			return -1;

		now = GetTickCount64();
	} while (now < dueTime);

#else
	do
	{
		struct timeval staticTimeout;
		struct timeval* timeout;

		fd_set* rset = nullptr;
		fd_set* wset = nullptr;

		if (dwMilliseconds == INFINITE)
		{
			timeout = nullptr;
		}
		else
		{
			long waitTime = (long)(dueTime - now);

			timeout = &staticTimeout;
			timeout->tv_sec = waitTime / 1000;
			timeout->tv_usec = (waitTime % 1000) * 1000;
		}

		if (set->nread)
		{
			rset = &set->rset;
			memcpy(rset, &set->rset_base, sizeof(*rset));
		}

		if (set->nwrite)
		{
			wset = &set->wset;
			memcpy(wset, &set->wset_base, sizeof(*wset));
		}

		ret = select(set->maxFd + 1, rset, wset, nullptr, timeout);
		if (ret >= 0)
			return ret;

		if (errno != EINTR)
			return -1;

		now = GetTickCount64();

	} while (now < dueTime);

	FD_ZERO(&set->rset);
	FD_ZERO(&set->wset);
#endif

	return 0; /* timeout */
}

BOOL pollset_isSignaled(WINPR_POLL_SET* set, size_t idx)
{
	WINPR_ASSERT(set);

	if (idx > set->fillIndex)
	{
		WLog_ERR(TAG, "index=%" PRIuz " out of pollset(fillIndex=%" PRIuz ")", idx, set->fillIndex);
		return FALSE;
	}

#ifdef WINPR_HAVE_POLL_H
	return !!(set->pollset[idx].revents & set->pollset[idx].events);
#else
	FdIndex* fdIndex = &set->fdIndex[idx];
	if (fdIndex->fd < 0)
		return FALSE;

	if ((fdIndex->mode & WINPR_FD_READ) && FD_ISSET(fdIndex->fd, &set->rset))
		return TRUE;

	if ((fdIndex->mode & WINPR_FD_WRITE) && FD_ISSET(fdIndex->fd, &set->wset))
		return TRUE;

	return FALSE;
#endif
}

BOOL pollset_isReadSignaled(WINPR_POLL_SET* set, size_t idx)
{
	WINPR_ASSERT(set);

	if (idx > set->fillIndex)
	{
		WLog_ERR(TAG, "index=%" PRIuz " out of pollset(fillIndex=%" PRIuz ")", idx, set->fillIndex);
		return FALSE;
	}

#ifdef WINPR_HAVE_POLL_H
	return !!(set->pollset[idx].revents & POLLIN);
#else
	FdIndex* fdIndex = &set->fdIndex[idx];
	if (fdIndex->fd < 0)
		return FALSE;

	return FD_ISSET(fdIndex->fd, &set->rset);
#endif
}

BOOL pollset_isWriteSignaled(WINPR_POLL_SET* set, size_t idx)
{
	WINPR_ASSERT(set);

	if (idx > set->fillIndex)
	{
		WLog_ERR(TAG, "index=%" PRIuz " out of pollset(fillIndex=%" PRIuz ")", idx, set->fillIndex);
		return FALSE;
	}

#ifdef WINPR_HAVE_POLL_H
	return !!(set->pollset[idx].revents & POLLOUT);
#else
	FdIndex* fdIndex = &set->fdIndex[idx];
	if (fdIndex->fd < 0)
		return FALSE;

	return FD_ISSET(fdIndex->fd, &set->wset);
#endif
}

#endif
