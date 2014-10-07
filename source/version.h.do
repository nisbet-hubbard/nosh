#!/bin/sh -e
cwd="`cd .. && /bin/pwd`"
base_plus_version="`basename "${cwd}"`"
base="`echo "${base_plus_version}" | sed -e 's/-[[:digit:]][[:alnum:].]*$//'`"
if [ "${base}" != "${base_plus_version}" ]
then
	version="`echo "${base_plus_version}" | sed -e 's/^.*-\([[:digit:]][[:alnum:].]*\)$/\1/'`"
	echo > $3 '#define' 'VERSION' "\"${version}\""
else
	echo > $3 '#define' 'VERSION' '"unknown"'
fi
exit 0
