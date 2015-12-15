#!/bin/sh

# Helper script for building the openhome libs prior to building
# scmpdcli.  This is far from foolproof or knowledgeable, but it seems
# to get me usable (static) libs. We first clone the git repositories,
# checkout a known ok version (it's more or less random, based on the
# 1st date I tried this, but some further versions don't build), then
# build th edifferent dirs.
# You should create a top empty dir first, then "sh ohbuild.sh mytopdir"

fatal()
{
    echo $*; exit 1
}
usage()
{
    fatal "Usage: ohbuild.sh <topdir>"
}
test $# = 1 || usage
topdir=$1

cd $topdir || exit 1
topdir=`pwd`


otherfiles=`echo *.*`
test "$otherfiles"  != '*.*' && fatal not in top dir

arch=
debug=

clone_oh()
{
    cd $topdir
    for rep in \
        https://github.com/openhome/ohNet.git \
        https://github.com/openhome/ohdevtools.git \
        https://github.com/openhome/ohNetGenerated.git \
        https://github.com/openhome/ohTopology.git \
        https://github.com/openhome/ohSongcast.git \
        ; do
        dir=`echo $rep | sed -e 's+https://github.com/openhome/++' \
            -e 's/\.git$//'`
        echo $dir
        test ! -d $dir && git clone $rep
    done
}

build_ohNet()
{
    cd $topdir
    dir=ohNet
    echo building $dir
    cd  $dir
    # Nov 30 2015. Commits from early december broke ohNetGenerated
    git checkout 0e22566e3da0eb9b40f4183e76254c6f2a3c2909 || exit 1

    git checkout Makefile
    # Note: the 'make: o: Command not found' errors originate in
    # common.mak and are due to variable t4 being undefined. t4 is
    # normally defined as 'mono' in T4Linux.mak, included in Makefile
    # only if 'uset4' is set (which it is not by default). Common.mak
    # should heed uset4, but it does not. This does not seem to have
    # consequences, and the errors are suppressed by defining t4 as 'echo'
    patch -p1 <<EOF
diff --git a/Makefile b/Makefile
index 7c0dae8..ddc4477 100644
--- a/Makefile
+++ b/Makefile
@@ -412,6 +412,7 @@ ifeq (\$(uset4), yes)
 build_targets = \$(build_targets_base) tt
 else
 build_targets = \$(build_targets_base)
+t4 = echo
 endif
 default : all
 
EOF

    make native_only=yes || exit 1

    cd ..
}

build_ohNetGenerated()
{
    dir=ohNetGenerated
    echo building $dir
    cd  $dir
    # Jul 30 2015
    git checkout 92294ce514dbe38fa569fce8b58588f40bf09cdb || exit 1
    git checkout Makefile
    patch -p1 <<EOF
diff --git a/Makefile b/Makefile
index a7b84e3..9c335f8 100644
--- a/Makefile
+++ b/Makefile
@@ -359,6 +359,7 @@ ifeq (\$(uset4), yes)
 build_targets = \$(build_targets_base) tt
 else
 build_targets = \$(build_targets_base)
+t4 = echo
 endif
 default : all

EOF

    # e.g. Linux-x64, Linux-armhf
    arch=`basename $topdir/ohNet/Build/Bundles/ohNet-*-*.tar.gz | \
        sed -e s/ohNet-//  -e s/-[A-Z][a-z][a-z]*\.tar\.gz$//`
    # e.g. Debug, Release
    debug=`basename $topdir/ohNet/Build/Bundles/ohNet-*-*.tar.gz | \
        sed -e s/.*-//  -e s/\.tar\.gz$//`

    sd=dependencies/${arch}
    mkdir -p "$sd"
    (cd $sd;
        tar xvzf $topdir/ohNet/Build/Bundles/ohNet-${arch}-${debug}.tar.gz
    ) || exit 1


    make native_only=yes

    # Copy the includes from here to the ohNet dir where ohTopology
    # will want them
    tar cf - Build/Include | (cd $topdir/ohNet/;tar xvf -) || exit 1

    cd ..
}

build_ohdevtools()
{
    cd $topdir
    dir=ohdevtools
    echo building $dir
    cd  $dir

    # Dec 9 2015
    git checkout 77f4f971e2c62e84a7eab2a1bcf4aa3dc3590840 || exit 1
    # Nothing to build
    cd ..
}


# It appears that nothing compiled in topology is needed for Receivers
# or Senders, only managers of those. Some of the include files are
# needed (or at least used) though.
build_ohTopology()
{
    cd $topdir
    dir=ohTopology
    echo building $dir
    cd  $dir

    # Mar 17 2015
    git checkout 18f004621a7b0dc3add6ddfeec781bd3878ae42e || exit 1
	
    #./go fetch --all --clean 
    #./waf configure --ohnet=../ohNet --dest-platform=Linux-x86

    # The build fails because of mono issues (trying to generate
    # include files from templates, this is probably fixable as the e
    # actual includes may exist somewhere).
    #./waf build

    mkdir -p build/Include/OpenHome/Av
    cp -p OpenHome/Av/*.h build/Include/OpenHome/Av/

    cd ..
}

build_ohSongcast()
{
    cd $topdir
    dir=ohSongcast
    echo building $dir
    cd  $dir

    # Aug 19 2015
    git checkout fe9b8a80080118f3bff9b44328975d10bc2c230b || exit 1

    make release=1 Receiver
    # make release=1 WavSender
}

official_way()
{
    # from README, actually Does not work, for reference. Issues probably have
    # something to do with lacking mono or wrong version
    cd ohNet
    make ohNetCore proxies devices TestFramework
    cd ../ohNetGenerated
    ./go fetch --all
    make
    cd ../ohNetmon
    ./go fetch --all
    ./waf configure --ohnet=../ohNet
    ./waf build
    cd ../ohTopology
    ./go fetch --all
    ./waf configure --ohnet=../ohNet
    ./waf build
    cd ../ohSongcast
    make release=1
}

clone_oh

build_ohNet
build_ohNetGenerated
build_ohdevtools
build_ohTopology
build_ohSongcast
