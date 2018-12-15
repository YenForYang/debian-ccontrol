#! /bin/bash -e

TESTDIR=/tmp/ccontrol-test

# We embed test info in comments
get_args()
{
    sed -n 's/^# *ARGS *\(.*\)/\1/p' $1
}

get_argv0()
{
    sed -n 's/^# *NAME *\(.*\)/\1/p' $1
}

# EXIT is optional.
get_exit_status()
{
    STATUS=`sed -n 's/^# *EXIT  *\(.*\)/\1/p' $1`
    if [ x"$STATUS" = x ]; then echo 0; else echo $STATUS; fi
}

get_output()
{
    sed -n 's/^# *EXEC *\(.*\)/\1/p' $1
}

get_exit_code()
{
    CODE=`sed -n 's/^# *EXITCODE *\(.*\)/\1/p' $1`
    if [ x"$CODE" = x ]; then echo 0; else echo $CODE; fi
}

get_fail_distcc()
{
    if grep -q FAILDISTCC $1; then echo 1; else echo 0; fi
}
    
run_test()
{
    ln -sfn `pwd`/$1 $TESTDIR/.ccontrol/config
    HOME=$TESTDIR ARGV0=`get_argv0 $1` EXITCODE=`get_exit_code $1` FAILDISTCC=`get_fail_distcc $1` $VALGRIND ./ccontrol_test `get_args $1` > $TESTDIR/output 2>&1
    RET=$?
    if [ $RET -ne `get_exit_status $1` ]; then
	cat $TESTDIR/output
	echo $1 failed: unexpected exit status $RET
	return 1
    fi
    case "`sed 's/ *$//' $TESTDIR/output`" in
	`get_output $1`) : ;;
	*)
	    cat $TESTDIR/output
	    echo $1 failed: unexpected output
	    return 1
	    ;;
    esac
    return 0
}

rm -rf $TESTDIR
mkdir $TESTDIR
mkdir $TESTDIR/.ccontrol
VALGRIND=`which valgrind`
case "$1" in
    --valgrind=*)
	VALGRIND=`echo $1 | cut -d= -f2-`; shift;;
esac

if [ -n "$VALGRIND" ]; then
    VALGRIND="$VALGRIND --suppressions=testsuite/vg-suppressions -q"
fi

# Clean any *real* ccontrol env vars out of environment (make check)
unset CCONTROL_LOCK
unset CCONTROL_NO_PARALLEL

MATCH=${1:-"*"}
for f in testsuite/[0-9]*.test; do
    case `basename $f` in $MATCH) RUN=1;; esac
    [ -n "$RUN" ] || continue

    if run_test $f; then
	echo -n .
    else
	exit 1
    fi
done
echo
