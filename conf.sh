#!/bin/bash
TEST_HOME=$HOME/gdd_test
DBDIR=pg14_gdd_database
DBPATH=$TEST_HOME/$DBDIR
pgmode pg14_gdd
SRCDIR=/hdd2/koichi/postgres-folk/postgres
TARGET=$HOME/pg14_gdd
TESTSRC=./sql_functions
BUILD_LOGDIR=/var/log/koichi/gdd_test_build
BUILD_LOGFILE_BODY=gdd_build
INIT_LOGFILE_BODY=gdd_test_init
HTAG_DIR=/hdd2/koichi/postgres_htag
TARGET_TAG=PG14_GDD
GENERATOR_SRC=$TEST_HOME/general_scenario_generator
export COPT='-O0 -g3'
HOSTS=(ubuntu00 ubuntu01 ubuntu02 ubuntu03 ubuntu04)
