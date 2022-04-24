# source gitver.sh, or
# source gitver.sh <path-to-your-include-dir> <your-version-h>
#!/bin/sh
 
if [ $# != 2 ]; then
    INC_DIR=.
    FILENAME=version.h
else
    INC_DIR=$1
    FILENAME=$2
fi
 
VER_FILE=$INC_DIR/$FILENAME
 
DATE=`date +"built: %Y-%m-%d %k:%M:%S"`
 
if [ -d .git ]; then
	GITLOCALVER=`git rev-list HEAD | wc -l | awk '{print $1}'`
	echo "Git Local version:" $GITLOCALVER
	GIT_VER=r$GITLOCALVER
	#GIT_VER="Git: $GIT_VER $(git rev-list HEAD -n 1 | cut -c 1-7)"
	GIT_VER="Git: $GIT_VER $(git describe --tags --always --dirty="-dev")"
	GIT_VERSION=$GIT_VER
	VB_HASGITVER=1
	echo "ALL GIT Version:" $GIT_VERSION
else
	VB_HASGITVER=0
fi
 
VB_HASSVNVER=0
if [ -d .svn ]; then
	VER16=`svn --version | grep "1\.6"`
	VER17=`svn --version | grep "1\.7"`
	VER18=`svn --version | grep "1\.8"`
 
	if [ "$VER16" != "" ]; then
		SVNLOCALVER=`svn info | cat -n | awk '{if($1==5)print $3}'`
		echo "1.6 version"
	fi
	if [ "$VER17" != "" ]; then
		SVNLOCALVER=`svn info | cat -n | awk '{if($1==6)print $3}'`
		echo "1.7 version"
	fi
	if [ "$VER18" != "" ]; then
		SVNLOCALVER=`svn info | cat -n | awk '{if($1==7)print $3}'`
		echo "1.8 version"
	fi
 
	echo "SVN Local Version:" $SVNLOCALVER
	SVN_VER=$SVNLOCALVER
	SVN_VER="SVN: v$SVN_VER"
	SVN_VERSION=$SVN_VER
	VB_HASSVNVER=1
	echo "ALL SVN Version:" $SVN_VERSION
fi
 
#version.h
if [ $VB_HASGITVER = 0 ] && [ $VB_HASSVNVER = 0 ]; then
	echo "No Version Control."
else
	echo "Generated:" $VER_FILE 
	echo "#ifndef VC_VERSION_H" > $VER_FILE
	echo "#define VC_VERSION_H" >> $VER_FILE
	echo "" >> $VER_FILE
 
	if [ $VB_HASGITVER = 1 ] && [ $VB_HASSVNVER = 0 ]; then
		echo "#define VERSION_NUMBER \"$GIT_VERSION $DATE\"" >> $VER_FILE
	fi
 
	if [ $VB_HASGITVER = 0 ] && [ $VB_HASSVNVER = 1 ]; then
		echo "#define VERSION_NUMBER \"$SVN_VERSION $DATE\"" >> $VER_FILE
	fi
	 
	if [ $VB_HASGITVER = 1 ] && [ $VB_HASSVNVER = 1 ]; then
		echo "#define VERSION_NUMBER \"$GIT_VERSION , $SVN_VERSION $DATE\"" >> $VER_FILE
	fi
 
	echo "" >> $VER_FILE
	echo "#endif" >> $VER_FILE
fi