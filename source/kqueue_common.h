/* COPYING ******************************************************************
For copyright and licensing terms, see the file named COPYING.
// **************************************************************************
*/

#include <stdint.h>
#if defined(__LINUX__) || defined(__linux__)
#include "kqueue_linux.h"
#else
#include <sys/event.h>
#endif

/// \brief An inline function that replicates EV_SET.
/// This does not evaluate its arguments more than once.
/// On OpenBSD, the macro does; FreeBSD/PC-BSD uses a temporary in the macro to avoid doing so.
extern inline
void
set_event (
	struct kevent * const ev,
	uintptr_t ident,
	short filter,
	unsigned short flags,
	unsigned int fflags,
	intptr_t data,
	void *udata
) {
	EV_SET(ev, ident, filter, flags, fflags, data, udata);
}
