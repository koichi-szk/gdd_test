#!/bin/bash

CLR_OPT=n
while getopts "c" OPT; do
	case $OPT in
		"c")
			CLR_OPT=y;;
		*)
			echo "Invalid option."
			exit 1;;
	esac
done
		
cd /home/koichi/gdd_test
. ./conf.sh
mkdir -p $BUILD_LOGDIR
LOGFILE=$(logfile_gen.sh $BUILD_LOGFILE_BODY $BUILD_LOGDIR)
touch $LOGFILE
cd $SRCDIR
if [ "$CLR_OPT" == "y" ]; then
	echo "--------------- Postgres Configure ---------------------------------------" | tee -a $LOGFILE
	echo "./configure --prefix=$TARGET --enable-debug --with-perl --with-pythonu --with-openssl" |& tee -a $LOGFILE
	./configure --prefix=$TARGET --enable-debug --with-perl --with-pythonu --with-openssl |& tee -a $LOGFILE
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
cd $TESTSRC
echo "--------------- Test tool build ------------------------------------------" | tee -a $LOGFILE
if [ "$CLR_OPT" == "y" ];then
	make clean |& tee -a $LOGFILE
fi
make |& tee -a $LOGFILE
echo "--------------- Test tool install ----------------------------------------" | tee -a $LOGFILE
make install |& tee -a $LOGFILE
