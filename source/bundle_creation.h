/* COPYING ******************************************************************
For copyright and licensing terms, see the file named COPYING.
// **************************************************************************
*/

#if !defined(INCLUDE_BUNDLE_CREATON_H)
#define INCLUDE_BUNDLE_CREATON_H

#include <string>

class FileDescriptorOwner;

void
create_link (
	const char * prog,
	const std::string & name,
	const FileDescriptorOwner & bundle_dir_fd,
	const std::string & target,
	const std::string & link
) ;
void
create_links (
	const char * prog,
	const std::string & bund,
	const bool is_target,
	const bool etc_bundle,
	const FileDescriptorOwner & bundle_dir_fd,
	const std::string & names,
	const std::string & subdir
) ;
void
make_mount_interdependencies (
	const char * prog,
	const std::string & name,
	const bool etc_bundle,
	const bool prevent_root_link,
	const FileDescriptorOwner & bundle_dir_fd,
	std::string where
) ;
static inline
bool
is_root(
	const char * p
) {
	return '/' == p[0] && '\0' == p[1];
}

#endif
