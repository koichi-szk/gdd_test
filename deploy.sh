#!/bin/bash
. ./conf.sh

PG_INSTALLHOME=/home/koichi
PG_INSTALLDIR=pg14_gdd
GDD_TESTHOME=/home/koichi
GDD_TESTDIR=gdd_test

PG_SRC=$PG_INSTALLHOME/$PG_INSTALLDIR
PG_DEST=$PG_INSTALLHOME

TEST_SRC=$GDD_TESTHOME/$GDD_TESTDIR
TEST_DEST=$GDD_TESTHOME

for h in "${HOSTS[@]}"; do
rsync -av $HOME/bin $h:
rsync -av $PG_SRC $h:$PG_DEST
rsync -av $TEST_SRC $h:$GDD_DEST
ssh $h mkdir -p /hdd2/koichi/postgres-folk/postgres/src
rsync -a /hdd2/koichi/postgres-folk/postgres/src $h:/hdd2/koichi/postgres-folk/postgres
done
