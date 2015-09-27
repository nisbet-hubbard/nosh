/* COPYING ******************************************************************
For copyright and licensing terms, see the file named COPYING.
// **************************************************************************
*/

#include <vector>
#include <map>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <ostream>
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>
#include "utils.h"
#include "listen.h"

/* Support functions ********************************************************
// **************************************************************************
*/

namespace {
struct Client {
	Client() : off(0) {}

	char buffer[384];
	struct initreq {
		enum { MAGIC = 0x03091969 };
		enum { RUNLVL = 1 };
		int magic;
		int cmd;
		int runlevel;
	};
	std::size_t off;
} ;
}

/* Main function ************************************************************
// **************************************************************************
*/

// This must have static storage duration as we are using it in args.
static std::string runlevel_option;

void
initctl_read (
	const char * & next_prog,
	std::vector<const char *> & args
) {
	const char * prog(basename_of(args[0]));

	const unsigned listen_fds(query_listen_fds());
	if (1U > listen_fds) {
		const int error(errno);
		std::fprintf(stderr, "%s: FATAL: %s: %s\n", prog, "LISTEN_FDS", std::strerror(error));
		throw EXIT_FAILURE;
	}

#if !defined(__LINUX__) && !defined(__linux__)
	struct sigaction sa;
	sa.sa_flags=0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler=SIG_IGN;
	sigaction(SIGHUP,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTSTP,&sa,NULL);
	sigaction(SIGALRM,&sa,NULL);
	sigaction(SIGPIPE,&sa,NULL);
	sigaction(SIGQUIT,&sa,NULL);
#endif

	const int queue(kqueue());
	if (0 > queue) {
		const int error(errno);
		std::fprintf(stderr, "%s: FATAL: %s: %s\n", prog, "kqueue", std::strerror(error));
		throw EXIT_FAILURE;
	}

	{
		std::vector<struct kevent> p(listen_fds + 7);
		for (unsigned i(0U); i < listen_fds; ++i)
			EV_SET(&p[i], LISTEN_SOCKET_FILENO + i, EVFILT_READ, EV_ADD, 0, 0, 0);
		EV_SET(&p[listen_fds + 0], SIGHUP, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
		EV_SET(&p[listen_fds + 1], SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
		EV_SET(&p[listen_fds + 2], SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
		EV_SET(&p[listen_fds + 3], SIGTSTP, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
		EV_SET(&p[listen_fds + 4], SIGALRM, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
		EV_SET(&p[listen_fds + 5], SIGPIPE, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
		EV_SET(&p[listen_fds + 6], SIGQUIT, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
		if (0 > kevent(queue, p.data(), listen_fds + 7, 0, 0, 0)) {
			const int error(errno);
			std::fprintf(stderr, "%s: FATAL: %s: %s\n", prog, "kevent", std::strerror(error));
			throw EXIT_FAILURE;
		}
	}

	std::vector<Client> clients(listen_fds);
	bool in_shutdown(false);
	for (;;) {
		try {
			if (in_shutdown) break;
			struct kevent e;
			if (0 > kevent(queue, 0, 0, &e, 1, 0)) {
				const int error(errno);
				if (EINTR == error) continue;
				std::fprintf(stderr, "%s: FATAL: %s: %s\n", prog, "kevent", std::strerror(error));
				throw EXIT_FAILURE;
			}
			switch (e.filter) {
				case EVFILT_READ:
				{
					const int fd(e.ident);
					if (LISTEN_SOCKET_FILENO > fd || LISTEN_SOCKET_FILENO + static_cast<int>(listen_fds) <= fd) {
						std::fprintf(stderr, "%s: DEBUG: read event ident %lu\n", prog, e.ident);
						break;
					}
					Client & c(clients[fd]);
					const int n(read(fd, c.buffer + c.off, sizeof c.buffer - c.off));
					if (0 > n) {
						const int error(errno);
						std::fprintf(stderr, "%s: ERROR: %s: %s\n", prog, "read", std::strerror(error));
						break;
					}
					if (0 == n)
						// FIXME: This incorrectly assumes one input fd.
						break;
					c.off += n;
					if (c.off < sizeof c.buffer)
						break;
					c.off = 0;
					const Client::initreq & r(*reinterpret_cast<const Client::initreq *>(c.buffer));
					if (r.MAGIC != r.magic) {
						std::fprintf(stderr, "%s: ERROR: %s\n", prog, "bad magic number in request");
						break;
					}
					if (r.cmd != r.RUNLVL) {
						std::fprintf(stderr, "%s: ERROR: %d: %s\n", prog, r.cmd, "unsupported command in request");
						break;
					}
					if (!std::isprint(r.runlevel)) {
						std::fprintf(stderr, "%s: ERROR: %d: %s\n", prog, r.runlevel, "unsupported run level in request");
						break;
					}
					const int pid(fork());
					if (0 > pid) {
						const int error(errno);
						std::fprintf(stderr, "%s: ERROR: %s: %s\n", prog, "fork", std::strerror(error));
						break;
					}
					if (0 != pid)
						break;
					const char option[3] = { '-', static_cast<char>(r.runlevel), '\0' };
					runlevel_option = option;
					args.clear();
					args.insert(args.end(), "telinit");
					args.insert(args.end(), runlevel_option.c_str());
					args.insert(args.end(), 0);
					next_prog = arg0_of(args);
					return;
					break;
				}
				case EVFILT_SIGNAL:
					switch (e.ident) {
						case SIGHUP:
						case SIGTERM:
						case SIGINT:
						case SIGPIPE:
						case SIGQUIT:
							in_shutdown = true;
							break;
						case SIGTSTP:
							std::fprintf(stderr, "%s: INFO: %s\n", prog, "Paused.");
							raise(SIGSTOP);
							std::fprintf(stderr, "%s: INFO: %s\n", prog, "Continued.");
							break;
						case SIGALRM:
						default:
							std::fprintf(stderr, "%s: DEBUG: signal event ident %lu fflags %x\n", prog, e.ident, e.fflags);
							break;
					}
					break;
				default:
					std::fprintf(stderr, "%s: DEBUG: event filter %hd ident %lu fflags %x\n", prog, e.filter, e.ident, e.fflags);
					break;
			}
		} catch (const std::exception & e) {
			std::fprintf(stderr, "%s: ERROR: exception: %s\n", prog, e.what());
		}
	}
	throw EXIT_SUCCESS;
}
