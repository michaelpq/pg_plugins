#!/bin/sh
# Build a minimal docker image using Linux Alpine.
# This has been taken from the original Docker facility of Alpine and
# modified slightly for the needs of ARM:
# https://github.com/gliderlabs/docker-alpine
# This script needs to be run with root permissions,

set -e

# Help message
usage() {
	printf >&2 '%s: [-r release] [-m mirror] [-s]\n' "$0"
	exit 1
}

# Create temporary paths when running this script, those are cleaned up
# automatically when this script exists.
tmp() {
	TMP=$(mktemp -d /tmp/alpine-docker-XXXXXXXXXX)
	ROOTFS=$(mktemp -d /tmp/alpine-docker-rootfs-XXXXXXXXXX)
	trap "rm -rf $TMP $ROOTFS" EXIT TERM INT
}

# Get the version number of this build. Note that apk-tools-static is not
# available in the testing layout of Alpine packages.
apkv() {
	curl -s $REPO/$ARCH/APKINDEX.tar.gz | tar -Oxz |
		grep -a '^P:apk-tools-static$' -A1 | tail -n1 | cut -d: -f2
}

# Get apk command from the source repo to perform operations
getapk() {
	curl -s $REPO/$ARCH/apk-tools-static-$(apkv).apk |
		tar -xz -C $TMP sbin/apk.static
}

# Create a minimalistic base image that will be used for the import phase
# with docker.
mkbase() {
	$TMP/sbin/apk.static --repository $REPO --update-cache --allow-untrusted \
						 --root $ROOTFS --initdb add alpine-base
}

# Update list of repositories to the one defined here.
conf() {
	printf '%s\n' $REPO > $ROOTFS/etc/apk/repositories
}

# Import the root file system into a docker image.
pack() {
	local id
	id=$(tar --numeric-owner -C $ROOTFS -c . | docker import - alpine-$ARCH_TAG:$REL)

	# Tag the build just done
	docker tag $id alpine-$ARCH_TAG:latest
}

# Save the root file system into a tarball
save() {
	[ $SAVE -eq 1 ] || return

	tar --numeric-owner -C $ROOTFS -c . | xz > rootfs.tar.xz
}

ARCH=$(uname -m)
while getopts "hr:m:s" opt; do
	case $opt in
	    r)
		    REL=$OPTARG
			;;
		m)
			MIRROR=$OPTARG
			;;
		s)
			SAVE=1
			;;
		a)
			ARCH=$OPTARG
			;;
		*)
			usage
			;;
	esac
done

# Some initialization variables
REL=${REL:-edge}
MIRROR=${MIRROR:-http://nl.alpinelinux.org/alpine}
SAVE=${SAVE:-0}
REPO=$MIRROR/$REL/main
ARCH=$(uname -m)
ARCH_TAG=$ARCH

# ARM architectures are using a different alias for Raspberry PI
# builds in Linux Alpine deliverables.
if [ $ARCH == 'armv6l' -o $ARCH == 'armv7l' ]; then
    ARCH=armhf
fi

# Now process the whole thing.
tmp
getapk
mkbase
conf
pack
save
