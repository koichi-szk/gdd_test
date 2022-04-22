#!/bin/bash
#
# Copyright (c) 2022, Koichi Suzuki
#
cd $HOME/gdd_test
. ./conf.sh
cd $TEST_HOME
pgmode pg14_gdd
pg_ctl status -D $DBDIR
if [ $? == 0 ]; then
	pg_ctl stop -D $DBDIR
fi
rm -rf $DBDIR

TMP=/tmp/$$
TMP_BODY=$(basename $TMP)

for h in "${HOSTS[@]}"; do
	echo "===========<< $h >>===================="
	ssh $h << EOF > /dev/null 2>&1
		cd $TEST_HOME
		pgmode pg14_gdd
		if [ $? == 0 ]; then
			pg_ctl stop -D $DBDIR
		fi
		rm -r $DBDIR
EOF
done
