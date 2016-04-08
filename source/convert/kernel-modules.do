#!/bin/sh -e
## **************************************************************************
## For copyright and licensing terms, see the file named COPYING.
## **************************************************************************
#
# Convert the loaded kernel modules list external configuration formats.
# This is invoked by all.do .
#

# This is the systemd system, with several .d files full of module list files.
read_dir() { 
	for d 
	do 
		test \! -d "$d" || ( 
			cd "$d" && find -maxdepth 1 -name '*.conf' -a \( -type l -o -type f \)
		) | 
		while read -r i 
		do 
			echo "$d" "$i" 
		done 
	done 
}
list_modules_linux() { 
	read_dir /etc/modules-load.d/ /lib/modules-load.d/ /usr/lib/modules-load.d/ /usr/local/lib/modules-load.d/ | 
	awk '{ if (!x[$2]++) print $1$2"\n"; }' | 
	xargs grep -h -- '^[^;#]' 
	echo autofs
	echo ipv6
	echo unix
	true
}

# This is the BSD system, with settings in /etc/rc.conf{,.local}
read_rc() { clearenv read-conf rc.conf "`which printenv`" "$1" || true ; }
list_modules_bsd() { 
	( 
		read_rc kld_list || true
# ibcs2 is in the FreeBSD 10 rc.d scripts, but is not actually present as kernel modules any more.
#		read_rc ibcs2_loaders | sed -e 's:^:ibcs2_:' || true 
	) | 
	fmt -w 1
	true
}

list_modules() { case "`uname`" in Linux) list_modules_linux ;; *BSD) list_modules_bsd ;; esac ; }

redo-ifchange rc.conf

case "`uname`" in
*BSD)
	# This is the list of "known" BSD kernel modules for which service bundles are pre-supplied.
	# Note that we cannot just disable kmod@* and cyclog@kmod@* because of VirtualBox and others.
	for n in \
		fuse \
		geom_uzip \
		linux \
		svr4 \
		sysvmsg \
		sysvsem \
		sysvshm \
		;
	do
		system-control disable "kmod@$n"
	done
	for n in \
		ipfw_nat \
		libiconv \
		libmchain \
		linsysfs \
		msdosfs_iconv \
		sem \
		acpi_video \
		bwi_v3_ucode \
		bwn_v4_ucode \
		cuse4bsd \
		ext2fs \
		fdescfs \
		if_bwi \
		if_bwn \
		iwn1000fw \
		iwn4965fw \
		iwn5000fw \
		iwn5150fw \
		iwn6000fw \
		iwn6000g2afw \
		iwn6000g2bfw \
		iwn6050fw \
		mmc \
		mmcsd \
		ng_ubt \
		ntfs \
		ntfs_iconv \
		pefs \
		reiserfs \
		runfw \
		scd \
		smbfs \
		udf \
		udf_iconv \
		xfs \
		;
	do
		system-control disable "cyclog@kmod@$n"
		system-control disable "kmod@$n"
	done
	;;
esac

list_modules |
while read -r n
do
	# Note that the way that we are setting up prefixes allows variables such as ntfs_enable in /etc/rc.conf{,.local} .
	case "$n" in
	autofs)		;;	## log-less on BSD too?
	fuse)		;;	## log-less on BSD too?
	geom_uzip)	;;
	linux)		;;
	svr4)		;;
	sysvmsg)	;;
	sysvsem)	;;
	sysvshm)	;;
	*)
		system-control preset --prefix "cyclog@kmod@" -- "$n"
		;;
	esac
	system-control preset --prefix "kmod@" -- "$n"
	if system-control is-enabled "kmod@$n"
	then
		echo >> "$3" on "$n"
	else
		echo >> "$3" off "$n"
	fi
done

true
