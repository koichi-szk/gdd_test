#!/bin/bash
cd $HOME/gdd_test

. ./conf.sh

mkdir -p $BUILD_LOGDIR
LOGFILE=$(logfile_gen.sh $INIT_LOGFILE_BODY $BUILD_LOGDIR)
touch $LOGFILE

echo "===============<< $(hostname) >>===================="

cd $TEST_HOME
pgmode pg13_gdd
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

TMP=/tmp/$$
TMP_BASE=$(basename $TMP)

HOSTS=(ubuntu00 ubuntu01 ubuntu02 ubuntu03 ubuntu04)
for h in "${HOSTS[@]}"; do
	echo "=============<< $h >>===================="
	ssh $h << EOF > /dev/null 2>&1
		cd $TEST_HOME
		pgmode pg13_gdd
		pg_ctl status -w -D $DBDIR > $TMP 2>&1
		if [ $? == 0 ]; then
			pg_ctl stop -D $DBDIR >> $TMP 2>&1
		fi
		rm -rf $DBDIR
		initdb $DBDIR >> $TMP
		cp pg_hba.conf $DBDIR
		cat postgresql.conf.addition >> $DBDIR/postgresql.conf
		pg_ctl start -D $DBDIR >> $TMP 2>&1 &
		wait
		createdb koichi >> $TMP 2>&1
EOF
	psql -h $h -f $TESTSRC/gdd_test.sql
	scp $h:$TMP /tmp/$TMP_BASE
	cat /tmp/$TMP_BASE
	rm /tmp/$TMP_BASE
	ssh $h "rm $TMP" > /dev/null 2>&1
done
psql << EOF
DROP TABLE IF EXISTS t1; 
CREATE TABLE t1 (a int, b int);
INSERT INTO t1 values (0, 0), (1, 1); 
DROP TABLE IF EXISTS t2; 
CREATE TABLE t2 (a int, b int);
INSERT INTO t2 values (0, 0), (1, 1); 
\q
EOF

for h in "${HOSTS[@]}"; do
	psql -h $h << EOF
DROP TABLE IF EXISTS t1; 
CREATE TABLE t1 (a int, b int);
INSERT INTO t1 values (0, 0), (1, 1); 
DROP TABLE IF EXISTS t2; 
CREATE TABLE t2 (a int, b int);
INSERT INTO t2 values (0, 0), (1, 1); 
\q
EOF
done
