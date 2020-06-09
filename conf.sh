#!/bin/bash
TEST_HOME=/home/koichi/gdd_test
DBDIR=pg12_gdd_database
DBPATH=$PWD/$DBDIR
pgmode pg12_gdd
SRCDIR=/hdd2/koichi/postgres-folk/postgres
TARGET=/home/koichi/pg12_gdd
TESTSRC=./sql_functions
BUILD_LOGDIR=/var/log/koichi/gdd_test_build
BUILD_LOGFILE_BODY=bdd_build
HTAG_DIR=/hdd2/koichi/postgres_htag
TARGET_TAG=PG12_GDD
