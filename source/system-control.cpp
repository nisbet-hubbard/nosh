/* COPYING ******************************************************************
For copyright and licensing terms, see the file named COPYING.
// **************************************************************************
*/

#include <vector>
#include <map>
#include <set>
#include <cstddef>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <cerrno>
#include <new>
#include <memory>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include "utils.h"
#include "fdutils.h"
#include "service-manager-client.h"
#include "service-manager.h"
#include "common-manager.h"
#include "popt.h"

/* Global data **************************************************************
// **************************************************************************
*/

bool local_session_mode(false);

/* Utilities ****************************************************************
// **************************************************************************
*/

static 
const char * const 
target_bundle_prefixes[3] = {
	"/run/system-manager/targets/", 
	"/etc/system-manager/targets/", 
	"/var/system-manager/targets/"
}, * const
service_bundle_prefixes[5] = {
	"/run/sv/", 
	"/etc/sv/", 
	"/var/local/sv/",
	"/var/sv/",
	"/service/"
};

int
open_bundle_directory (
	const char * arg,
	std::string & path,
	std::string & name,
	std::string & suffix
) {
	if (const char * slash = std::strchr(arg, '/')) {
		path = std::string(arg, slash + 1);
		name = std::string(slash + 1);
		suffix = std::string();
		return open_dir_at(AT_FDCWD, (path + name + "/").c_str());
	}

	const std::string a(arg);
	if (!local_session_mode) {
		bool scan_for_target(false), scan_for_service(false);
		if (ends_in(a, ".target", name)) {
			scan_for_target = true;
		} else
		if (ends_in(a, ".service", name)) {
			scan_for_service = true;
		} else
		if (ends_in(a, ".socket", name)) {
			scan_for_service = true;
		} else
		{
			name = a;
			scan_for_target = scan_for_service = true;
		}
		if (scan_for_target) {
			suffix = ".target";
			for ( const char * const * q(target_bundle_prefixes); q < target_bundle_prefixes + sizeof target_bundle_prefixes/sizeof *target_bundle_prefixes; ++q) {
				path = *q;
				const int bundle_dir_fd(open_dir_at(AT_FDCWD, (path + name + "/").c_str()));
				if (0 <= bundle_dir_fd) return bundle_dir_fd;
			}
		}
		if (scan_for_service) {
			suffix = ".service";
			for ( const char * const * q(service_bundle_prefixes); q < service_bundle_prefixes + sizeof service_bundle_prefixes/sizeof *service_bundle_prefixes; ++q) {
				path = *q;
				const int bundle_dir_fd(open_dir_at(AT_FDCWD, (path + name + "/").c_str()));
				if (0 <= bundle_dir_fd) return bundle_dir_fd;
			}
		}
	}

	path = std::string();
	name = a;
	suffix = std::string();
	return open_dir_at(AT_FDCWD, (path + name + "/").c_str());
}

/* The system-control command ***********************************************
// **************************************************************************
*/

void
system_control ( 
	const char * & next_prog,
	std::vector<const char *> & args
) {
	const char * prog(basename_of(args[0]));
	try {
		// These compatibility options make command completion in the Z and Bourne Again shells slightly smoother.
		// They also prevent install/uninstall scripts in RPM packages from breaking.
		bool full(false), no_legend(false), no_pager(false), no_reload(false), quiet(false);
		popt::bool_definition full_option('\0', "full", "Compatibility option.  Ignored.", full);
		popt::bool_definition no_legend_option('\0', "no-legend", "Compatibility option.  Ignored.", no_legend);
		popt::bool_definition no_pager_option('\0', "no-pager", "Compatibility option.  Ignored.", no_pager);
		popt::bool_definition no_reload_option('\0', "no-reload", "Compatibility option.  Ignored.", no_reload);
		popt::bool_definition quiet_option('\0', "quite", "Compatibility option.  Ignored.", quiet);
		popt::bool_definition user_option('u', "user", "Communicate with the per-user manager.", local_session_mode);
		popt::definition * top_table[] = {
			&user_option,
			&full_option,
			&no_legend_option,
			&no_pager_option
		};
		popt::top_table_definition main_option(sizeof top_table/sizeof *top_table, top_table, "Main options", 
				"halt|reboot|poweroff|"
				"emergency|rescue|normal|init|sysinit|"
				"start|stop|try-restart|enable|disable|preset|reset|unload-when-stopped|"
				"is-active|is-loaded|"
				"cat|show|status|show-json|"
				"convert-systemd-units|convert-systemd-presets|"
				"convert-ttys-presets|convert-rcconf-presets|convert-fstab-services|"
				"nagios-check-services|load-kernel-module|unload-kernel-module|"
				"version"
				" args..."
		);

		std::vector<const char *> new_args;
		popt::arg_processor<const char **> p(args.data() + 1, args.data() + args.size(), prog, main_option, new_args);
		p.process(true /* strictly options before arguments */);
		args = new_args;
		next_prog = arg0_of(args);
		if (p.stopped()) throw EXIT_SUCCESS;
	} catch (const popt::error & e) {
		std::fprintf(stderr, "%s: FATAL: %s: %s\n", prog, e.arg, e.msg);
		throw EXIT_FAILURE;
	}

	if (args.empty()) {
		std::fprintf(stderr, "%s: FATAL: %s\n", prog, "Missing command name.");
		throw EXIT_FAILURE;
	}

	// Effectively, all subcommands are implemented by chaining to builtins of the same name.
}
