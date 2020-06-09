#!/bin/bash
. ./conf.sh
cd $PWD
pgmode pg12_gdd
pg_ctl status -D $DBDIR >& /dev/null
if [ $? == 0 ]; then
	pg_ctl stop -D $DBDIR
fi
