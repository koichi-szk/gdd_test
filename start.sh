#!/bin/bash
cd $HOME/gdd_test
. ./conf.sh
cd $TEST_HOME
pgmode pg12_gdd
pg_ctl status -D $DBDIR >& /dev/null
if [ $? == 0 ]; then
	pg_ctl stop -D $DBDIR
fi
pg_ctl start -D $DBDIR
