#!/bin/bash
TEST_HOME=/home/koichi/gdd_test
DBDIR=pg13_gdd_database
DBPATH=$PWD/$DBDIR
pgmode pg13_gdd
SRCDIR=/hdd2/koichi/postgres-folk/postgres
TARGET=/home/koichi/pg13_gdd
TESTSRC=./sql_functions
BUILD_LOGDIR=/var/log/koichi/gdd_test_build
BUILD_LOGFILE_BODY=gdd_build
INIT_LOGFILE_BODY=gdd_test_init
HTAG_DIR=/hdd2/koichi/postgres_htag
TARGET_TAG=PG13_GDD
GENERATOR_SRC=./general_scenario_generator
export COPT='-O0 -g3'
