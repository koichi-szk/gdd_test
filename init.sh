#!/bin/bash
. ./conf.sh
cd $TEST_HOME
pgmode pg12_gdd
pg_ctl status -D $DBDIR
if [ $? == 0 ]; then
	pg_ctl stop -D $DBDIR
fi
rm -rf $DBDIR
initdb $DBDIR
cp pg_hba.conf $DBDIR
cat postgresql.conf.addition >> $DBDIR/postgresql.conf
pg_ctl start -D $DBDIR
createdb koichi
psql -f $TESTSRC/gdd_test.sql
