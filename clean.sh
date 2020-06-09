#!/bin/bash
. ./conf.sh
cd $TEST_HOME
pgmode pg12_gdd
pg_ctl status -D $DBDIR
if [ $? == 0 ]; then
	pg_ctl stop -D $DBDIR
fi
rm -rf $DBDIR
