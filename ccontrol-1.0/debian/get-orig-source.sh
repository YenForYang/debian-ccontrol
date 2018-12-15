#!/bin/sh
set -e
export TAR_OPTIONS='--owner root --group root --mode a+rX'
export GZIP='-9n'
pwd=$(pwd)
version="$1"
if [ -z "$version" ]
then
    printf 'Usage: %s <version>\n' "$0"
    exit 1
fi
cd "$(dirname "$0")/../"
tmpdir=$(mktemp -t -d get-orig-source.XXXXXX)
set -x
uscan --noconf --force-download --rename --download-version="$version" --destdir="$tmpdir"
cd "$tmpdir"
tar -xjf ccontrol_*.orig.tar.bz2
rm *.tar.bz2
# Remove unwanted files:
rm -rf ccontrol-*/debian
rm -rf ccontrol-*/.hg*
mv ccontrol-*/ "ccontrol-$version.orig"
tar -czf "$pwd/ccontrol_$version.orig.tar.gz" ccontrol-*.orig/
set +x
cd ..
rm -rf "$tmpdir"

# vim:ts=4 sw=4 et
