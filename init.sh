#!/bin/bash
cd $HOME/gdd_test

. ./conf.sh

mkdir -p $BUILD_LOGDIR
LOGFILE=$(logfile_gen.sh $INIT_LOGFILE_BODY $BUILD_LOGDIR)
touch $LOGFILE


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
echo "createdb koichi" |& tee -a $LOGFILE
createdb koichi |& tee -a $LOGFILE
echo "psql -f $TESTSRC/gdd_test.sql" |& tee -a $LOGFILE
psql -f $TESTSRC/gdd_test.sql |& tee -a $LOGFILE


HOSTS=(ubuntu00 ubuntu01 ubuntu02 ubuntu03 ubuntu04)
for h in "${HOSTS[@]}"; do
	ssh $h << EOF
		cd $TEST_HOME
		pgmode pg12_gdd
		pg_ctl status -w -D $DBDIR
		if [ $? == 0 ]; then
			pg_ctl stop -D $DBDIR
		fi
		rm -rf $DBDIR
		initdb $DBDIR
		cp pg_hba.conf $DBDIR
		cat postgresql.conf.addition >> $DBDIR/postgresql.conf
		pg_ctl start -D $DBDIR
		createdb koichi
EOF
	psql -h $h -f $TESTSRC/gdd_test.sql
done

