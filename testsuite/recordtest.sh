#!/bin/bash

DIR="`dirname \"$0\"`"
. "$DIR"/_common.sh


if [ $# -eq 0 ] ; then
	fail "Usage: recordtest.sh <dir with test> [<options>]"
fi

TEST="$PWD/tests/$1"

shift
cd_workdir

cp -r context context.save
rm -rf after
cd context || fail "Could not cd into context dir"
tdrecord -o ../recording $RECORDOPTS -e T3WINDOW_OPTS ../../../src/.objects/edit -C ../test.cfg "$@" || fail "!! Could not record test"
fixup_test ../recording
cd .. || fail "Could not change back to work dir"

rm context/libt3widgetlog.txt context/log.txt
mv context after
mv context.save context

[ -d "$TEST" ] && rm -rf "$TEST"
mkdir "$TEST"
cp -r * "$TEST"
exit 0
