#!/bin/bash
cd $HOME/gdd_test
. ./conf.sh
cd $TEST_HOME
pgmode pg13_gdd
pg_ctl status -D $DBDIR
if [ $? == 0 ]; then
	pg_ctl stop -D $DBDIR
fi
rm -rf $DBDIR

TMP=/tmp/$$
TMP_BODY=$(basename $TMP)

HOSTS=(ubuntu00 ubuntu01 ubuntu02 ubuntu03 ubuntu04)
for h in "${HOSTS[@]}"; do
	echo "===========<< $h >>===================="
	ssh $h << EOF > /dev/null 2>&1
		cd $TEST_HOME
		pgmode pg13_gdd
		if [ $? == 0 ]; then
			pg_ctl stop -D $DBDIR
		fi
		rm -r $DBDIR
EOF
done
