#!/bin/bash

CLR_OPT=n
OPT_FLAG=n
while getopts "co" OPT; do
	case $OPT in
		"c")
			# If this flag is specified, .config will be invoked prior to the build
			CLR_OPT=y;;
		"o")
			# If this is not specified, CC optimizer will be set to -O0, which is
			# usful to test with gdb (default).
			# If this is specified, CC optimizer will be set to the default (-O2).
			# This is for regular use without gdb.
			OPT_FLAG=y;;
		*)
			echo "Invalid option."
			exit 1;;
	esac
done
		
cd /home/koichi/gdd_test

# Common configuration
. ./conf.sh

mkdir -p $BUILD_LOGDIR
# To generate log file, Koichi's local shell script is used.
LOGFILE=$(logfile_gen.sh $BUILD_LOGFILE_BODY $BUILD_LOGDIR)
touch $LOGFILE

# CC optimization level
if [ "$OPT_FLAG" == "n" ]; then
	export COPT='-O0 -g3'
else
	export COPT=""
fi

# Build base postgresql
cd $SRCDIR
echo "--------------- Postgres Configure ---------------------------------------" | tee -a $LOGFILE
#if [ $CLR_OPT == "y" ]; then
#	echo "./configure --prefix=$TARGET --enable-debug --with-perl --with-python --with-openssl" |& tee -a $LOGFILE
#	./configure --prefix=$TARGET --enable-debug --with-perl --with-python --with-openssl |& tee -a $LOGFILE
#fi
if [ $CLR_OPT == "y" ]; then
	echo "./configure --prefix=$TARGET --enable-debug --with-perl --with-python " |& tee -a $LOGFILE
	./configure --prefix=$TARGET --enable-debug --with-perl --with-python |& tee -a $LOGFILE
fi
echo "--------------- Posatgres Build ------------------------------------------" | tee -a $LOGFILE
if [ "$CLR_OPT" == "y" ];then
	echo "make clean" |& tee -a $LOGFILE
	make clean |& tee -a $LOGFILE
fi
echo "make -j 8" |& tee -a $LOGFILE
make -j 8 |& tee -a $LOGFILE
echo "--------------- Posatgres Make Check -------------------------------------" | tee -a $LOGFILE
echo "make check" |& tee -a $LOGFILE
make check |& tee -a $LOGFILE
echo "--------------- Posatgres Install ----------------------------------------" | tee -a $LOGFILE
echo "make install" |& tee -a $LOGFILE
make install |& tee -a $LOGFILE
echo "--------------- Run gtags and htags --------------------------------------" | tee -a $LOGFILE
cd src
echo "gtags" |& tee -a $LOGFILE
gtags
echo "htags -n -h --tabs 4" |& tee -a $LOGFILE
htags -n -h --tabs 4
cp -r HTML $HTAG_DIR/$TARGET_TAG
rm -r HTML

# Test tool build
echo "--------------- Test tool build ------------------------------------------" | tee -a $LOGFILE
echo "cd $TEST_HOME" |& tee -a $LOGFILE
cd $TEST_HOME
echo "cd $TESTSRC" |& tee -a $LOGFILE
cd $TESTSRC
if [ "$CLR_OPT" == "y" ];then
	echo "make clean" |& tee -a $LOGFILE
	make clean |& tee -a $LOGFILE
fi
echo "make" |& tee -a $LOGFILE
make |& tee -a $LOGFILE
echo "--------------- Test tool install ----------------------------------------" | tee -a $LOGFILE
echo "make install" |& tee -a $LOGFILE
make install |& tee -a $LOGFILE
cd $TEST_HOME
echo "cc -o sql sql.c"
cc -o sql sql.c
echo "--------------- Scenario Generator build ------------------------------------------" | tee -a $LOGFILE
echo "cd $TEST_HOME" |& tee -a $LOGFILE
cd $TEST_HOME
echo "cd $GENERATOR_SRC" |& tee -a $LOGFILE
cd $GENERATOR_SRC
if [ "$CLR_OPT" == "y" ];then
	echo "make clean" |& tee -a $LOGFILE
	make clean |& tee -a $LOGFILE
fi
echo "make" |& tee -a $LOGFILE
make |& tee -a $LOGFILE
echo "make install" |& tee -a $LOGFILE
make install |& tee -a $LOGFILE
