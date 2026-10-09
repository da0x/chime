#!/bin/sh
# chime's tests: what it runs, how it exits and what it says, with no sound played.
# Copyright (C) 2026 Daher Alfawares
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   tests/run.sh build/chime

set -u
chime=${1:?usage: tests/run.sh path/to/chime}
failures=0

# Runs chime with a PATH that has no sound player or notifier, so nothing is played.
quiet() { env PATH=/nonexistent "$chime" "$@"; }

expect() {
	description=$1 wanted_status=$2 wanted_words=$3
	shift 3
	said=$(quiet "$@" 2>&1)
	status=$?
	if [ "$status" -ne "$wanted_status" ]; then
		echo "FAIL $description: exit $status, wanted $wanted_status"
		failures=$((failures + 1))
	elif [ -n "$wanted_words" ] && ! printf '%s' "$said" | grep -q -- "$wanted_words"; then
		echo "FAIL $description: said \"$said\", wanted \"$wanted_words\""
		failures=$((failures + 1))
	else
		echo "ok   $description"
	fi
}

expect "a command that succeeds"          0   "finished in"          /bin/true
expect "a command that fails"             1   "failed with 1"        /bin/false
expect "the command's own status"         3   "failed with 3"        /bin/sh -c "exit 3"
expect "a command that isn't there"       127 "couldn't run"         chime-no-such-command
expect "a command stopped by a signal"    143 "stopped by SIGTERM"   /bin/sh -c 'kill -TERM $$'
expect "the job named"                    0   "/bin/true finished"   --name Job /bin/true
expect "a command after --"               0   "finished"             -- /bin/true
expect "chiming now"                      0   ""
expect "chiming an error now"             0   ""                     --error --message Failed
expect "an option it doesn't know"        2   "no option --bogus"    --bogus
expect "its version"                      0   "^chime [0-9]"         --version
expect "its help"                         0   "usage: chime"         --help

[ "$failures" -eq 0 ] && echo "all passed" || echo "$failures failed"
[ "$failures" -eq 0 ]
