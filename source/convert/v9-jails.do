#!/bin/sh -e
## **************************************************************************
## For copyright and licensing terms, see the file named COPYING.
## **************************************************************************
#
# Convert the FreeBSD 9 jails list external configuration formats.
# This is invoked by general-services.do .
#

# These get us *only* the configuration variables, safely.
read_rc() { if type sysrc >/dev/null 2>&1 ; then sysrc -i -n "$1" ; else clearenv read-conf -oknofile /etc/defaults/rc.conf read-conf -oknofile /etc/rc.conf read-conf -oknofile /etc/rc.conf.local `which printenv` "$1" ; fi }
dump() { clearenv read-conf -oknofile /etc/defaults/rc.conf read-conf -oknofile /etc/rc.conf read-conf -oknofile /etc/rc.conf.local `which printenv` ; }
list_jails() { read_rc jail_list || true ; }
get_config() { read_rc jail_"$1"_"$2" || read_rc jail_"$2" || true ; }

if_yes() { case "$1" in [Yy][Ee][Ss]|[Tt][Rr][Uu][Ee]) echo "$2" ;; esac ; }

for i in /etc/defaults/rc.conf /etc/rc.conf.local /etc/rc.conf
do
	if test -e "$i"
	then
		redo-ifchange "$i"
	else
		redo-ifcreate "$i"
	fi
done

redo-ifchange "v9-jail@.service" "v9-jailed@.service"

r="/var/local/sv"
e="--no-systemd-quirks --escape-instance --bundle-root"

for i in `list_jails`
do
	test -z "$i" && continue

	jail_service="v9-jail@$i"
	jailed_service="v9-jailed@$i"

	system-control convert-systemd-units $e "$r/" ./"${jail_service}.service"
	system-control convert-systemd-units $e "$r/" ./"${jailed_service}.service"

	mkdir -p -m 0755 "$r/${jail_service}/service/env"
	mkdir -p -m 0755 "$r/${jailed_service}/service/env"

	system-control preset -- "${jail_service}"
	system-control preset -- "${jailed_service}"

	dump |
	awk -F = "/jail_${i}_/{l=length(\"jail_${i}_\"); print substr(\$1,l+1,length(\$1)-l);}" |
	while read -r n
	do
		v="`get_config \"$i\" \"$n\"`"

		case "$n" in
		ip)		m="ip4";;
		rootdir)	m="root";;
		procfs_enable)	m="`if_yes \"$v\" mount_procfs`";;
		devfs_enable)	m="`if_yes \"$v\" mount_devfs`";;
		fdescfs_enable)	m="`if_yes \"$v\" mount_fdescfs`";;
		mount_enable)	m="`if_yes \"$v\" allow_mount`";;
		sysvipc_enable)	m="`if_yes \"$v\" allow_sysvipc`";;
		*)		m="$n";;
		esac
		test -n "$m" || continue

		system-control set-service-env -- "${jail_service}" "$m" "$v"
	done

	dump |
	awk -F = "/jail_${i}_exec_/{l=length(\"jail_${i}_exec_\"); print substr(\$1,l+1,length(\$1)-l);}" |
	while read -r n
	do
		system-control set-service-env -- "${jail_service}" "$n" "`get_config \"$i\" \"exec_$n\"`"
	done

	if system-control is-enabled "${jail_service}"
	then
		echo >> "$3" on "${jail_service}"
	else
		echo >> "$3" off "${jail_service}"
	fi
	system-control print-service-env "${jail_service}" >> "$3"
	if system-control is-enabled "${jailed_service}"
	then
		echo >> "$3" on "${jailed_service}"
	else
		echo >> "$3" off "${jailed_service}"
	fi
	system-control print-service-env "${jailed_service}" >> "$3"
done
