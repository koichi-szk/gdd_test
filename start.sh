#!/bin/bash
cd $HOME/gdd_test
. ./conf.sh
cd $TEST_HOME
pgmode pg13_gdd
pg_ctl status -D $DBDIR >& /dev/null
if [ $? == 0 ]; then
	pg_ctl stop -D $DBDIR
fi
TMP=/tmp/$$
pg_ctl start -D $DBDIR > $TMP 2>&1 &
wait
cat $TMP
rm $TMP

TMP_BASE=$(basename $TMP)

HOSTS=(ubuntu00 ubuntu01 ubuntu02 ubuntu03 ubuntu04)
for h in "${HOSTS[@]}"; do
	echo "=========<< $h >>========================"
	ssh $h << EOF > /dev/null 2>&1
		cd $TEST_HOME
		pgmode pg13_gdd
		pg_ctl start -D $DBDIR > $TMP 2>&1 &
		wait
EOF
scp $h:$TMP /tmp/$TMP_BASE
cat /tmp/$TMP_BASE
rm /tmp/$TMP_BASE
ssh $h "rm $TMP" > /dev/null 2>&1
done
