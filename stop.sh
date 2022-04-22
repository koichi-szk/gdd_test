#!/bin/bash
#
# Copyright (c) 2022, Koichi Suzuki
#
cd $HOME/gdd_test
. ./conf.sh
cd $TEST_HOME
pgmode pg14_gdd
pg_ctl status -D $DBDIR >& /dev/null
if [ $? == 0 ]; then
	pg_ctl stop -D $DBDIR
else
	echo "Test database is not running."
fi
TMP=/tmp/$$
TMP_BODY=$(basename $TMP)

for h in "${HOSTS[@]}"; do
	echo "============<< $h >>===================="
	ssh $h << EOF > /dev/null 2>&1
		cd $TEST_HOME
		pgmode pg14_gdd
		pg_ctl stop -D $DBDIR > $TMP 2>&1 &
		wait
EOF
scp $h:$TMP /tmp/$TMP_BODY
cat /tmp/$TMP_BODY
ssh $h "rm $TMP" > /dev/null 2>&1
done
