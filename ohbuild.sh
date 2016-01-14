#!/bin/sh

# Helper script for building the openhome libs prior to building
# sc2mpd and mpd2sc.
#
# Read about Openhome ohNet here: http://www.openhome.org/wiki/OhNet
#
# The source code we process is:
#    Copyright 2011-14, Linn Products Ltd. All rights reserved.
# See the license files under the different subdirs. In a nutshell: BSD
#
# This is far from foolproof or knowledgeable, but it seems to get me
# usable (static) libs.
# There are 3 modes of operation:
#   -c: clone, adjust, trim the source directories and produce a tar file
#   -b: clone, adjust the source dirs and build
#   -t: extract tar file and build.
#
# When cloning, we checkout a known ok version (it's more or less
# random, based on the last date I tried this, sometimes more recent
# versions don't build versions don't build), then build the different
# dirs.
#
# When producing the tar file, we get rid of the .git directory and a
# bunch of other things to reduce the size


fatal()
{
    echo $*; exit 1
}

usage()
{
    echo "Usage:"
    echo "ohbuild.sh -c <topdir> : clone and adjust openhome directories"
    echo " from the git repositories, and produce tar file in /tmp"
    echo "ohbuild.sh -t <tarfile> <topdir> : extract tar file in top dir and"
    echo "  build openhome in there"
    echo "ohbuild.sh -b <topdir> : clone and build, no cleaning up of unused"
    echo "  files, no tar file"
    echo "ohbuild.sh <topdir> : just build, don't change the tree"
    exit 1
}

opt_t=0
opt_c=0
opt_b=0
tarfile=''

opts=`getopt -n ohbuild.sh -o t:cb -- "$@"`
eval set -- "$opts"

while true ; do
    case "$1" in
        -t) opt_t=1; tarfile=$2 ; shift 2 ;;
        -b) opt_b=1; shift ;;
        -c) opt_c=1; shift ;;
        --) shift ; break ;;
        *) echo "Internal error!" ; exit 1 ;;
    esac
done

echo opt_t $opt_t
echo opt_c $opt_c
echo opt_b $opt_b
echo tarfile $tarfile

test $# -eq 1 || usage
topdir=$1
echo topdir $topdir

# Only one of -tcb
tot=`expr $opt_t + $opt_c + $opt_b`
test $tot -le 1 || usage

arch=
debug=

test -d $topdir || mkdir $topdir || fatal "Can't create $topdir"

cd $topdir || exit 1
# Make topdir an absolute path
topdir=`pwd`

otherfiles=`echo *.*`
test "$otherfiles"  != '*.*' && fatal topdir should not contain files

clone_oh()
{
    echo "Cloning OpenHome from git repos into $topdir"
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


    cd $topdir/ohNet
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
index 7c0dae8..6f51a86 100644
--- a/Makefile
+++ b/Makefile
@@ -88,6 +88,9 @@ else
     ifeq ($(gcc_machine),arm-linux-gnueabihf)
       detected_openhome_architecture = armhf
     endif
+    ifeq ($(gcc_machine),armv6l-unknown-linux-gnueabihf)
+      detected_openhome_architecture = armhf
+    endif
     ifneq (,$(findstring i686,$(gcc_machine)))
       detected_openhome_architecture = x86
     endif
@@ -412,6 +415,7 @@ ifeq ($(uset4), yes)
 build_targets = $(build_targets_base) tt
 else
 build_targets = $(build_targets_base)
+t4 = echo
 endif
 default : all
 
EOF

    cd  $topdir/ohNetGenerated
    # Jul 30 2015
    git checkout 92294ce514dbe38fa569fce8b58588f40bf09cdb || exit 1
    git checkout Makefile
    patch -p1 <<EOF
diff --git a/Makefile b/Makefile
index a7b84e3..5175a6f 100644
--- a/Makefile
+++ b/Makefile
@@ -79,6 +79,9 @@ else
     ifeq ($(gcc_machine),arm-linux-gnueabihf)
       detected_openhome_architecture = armhf
     endif
+    ifeq ($(gcc_machine),armv6l-unknown-linux-gnueabihf)
+      detected_openhome_architecture = armhf
+    endif
     ifneq (,$(findstring i686,$(gcc_machine)))
       detected_openhome_architecture = x86
     endif
@@ -359,6 +362,7 @@ ifeq ($(uset4), yes)
 build_targets = $(build_targets_base) tt
 else
 build_targets = $(build_targets_base)
+t4 = echo
 endif
 default : all
 
EOF

    cd  $topdir/ohdevtools
    # Dec 9 2015
    git checkout 77f4f971e2c62e84a7eab2a1bcf4aa3dc3590840 || exit 1
   
    cd  $topdir/ohTopology
    # Mar 17 2015
    git checkout 18f004621a7b0dc3add6ddfeec781bd3878ae42e || exit 1

    cd  $topdir/ohSongcast
    # Aug 19 2015
    git checkout fe9b8a80080118f3bff9b44328975d10bc2c230b || exit 1
}

make_tarfile()
{
    cd $topdir || exit 1
    
    # Make space: get rid of the .git and other not useful data, then
    # produce a tar file for reproduction
    for dir in ohNet ohNetGenerated ohdevtools ohTopology ohSongcast;do
        test -d $dir || fatal no "'$dir'" in "'$topdir'"
        rm -rf $topdir/$dir/.git
    done
    rm -rf $topdir/ohNet/thirdparty
    rm -rf $topdir/ohNetGenerated/OpenHome/Net/Bindings/Cs
    rm -rf $topdir/ohNetGenerated/OpenHome/Net/Bindings/Java
    rm -rf $topdir/ohNetGenerated/OpenHome/Net/Bindings/Js
    rm -rf $topdir/ohNetGenerated/OpenHome/Net/T4/
    rm -rf $topdir/ohSongcast/Docs/
    rm -rf $topdir/ohSongcast/ohSongcast/Mac
    rm -rf $topdir/ohSongcast/ohSongcast/Windows
    rm -rf $topdir/ohTopology/waf
    rm -rf $topdir/ohdevtools/nuget
    
    dt=`date +%Y%m%d`
    tar czf $tarfile/tmp/openhome-sc2-${dt}.tar.gz .
}

build_ohNet()
{
    dir=ohNet
    echo;echo building $dir
    cd  $topdir/$dir || exit 1

    make native_only=yes || exit 1

    cd ..
}

build_ohNetGenerated()
{
    dir=ohNetGenerated
    echo;echo building $dir
    cd  $topdir/$dir || exit 1

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

    # Create bogus files for unused Makefile dependencies which we don't
    # carry in the tar file.
    mkdir -p OpenHome/Net/Service/ OpenHome/Net/T4/Templates/
    touch OpenHome/Net/Service/Services.xml \
          OpenHome/Net/T4/Templates/UpnpMakeT4.tt \
          OpenHome/Net/T4/Templates/CpUpnpMakeProxies.tt \
          OpenHome/Net/T4/Templates/DvUpnpMakeDevices.tt

    make native_only=yes

    # Copy the includes from here to the ohNet dir where ohTopology
    # will want them
    tar cf - Build/Include | (cd $topdir/ohNet/;tar xvf -) || exit 1
}

build_ohdevtools()
{
    dir=ohdevtools
    echo;echo building $dir
    cd  $topdir/$dir || exit 1

    # Nothing to build
}


# It appears that nothing compiled in topology is needed for Receivers
# or Senders, only managers of those. Some of the include files are
# needed (or at least used) though.
build_ohTopology()
{
    dir=ohTopology
    echo;echo building $dir
    cd  $topdir/$dir || exit 1

    #./go fetch --all --clean 
    #./waf configure --ohnet=../ohNet --dest-platform=Linux-x86

    # The build fails because of mono issues (trying to generate
    # include files from templates, this is probably fixable as the e
    # actual includes may exist somewhere).
    #./waf build

    mkdir -p build/Include/OpenHome/Av
    cp -p OpenHome/Av/*.h build/Include/OpenHome/Av/
}

build_ohSongcast()
{
    dir=ohSongcast
    echo;echo building $dir
    cd  $topdir/$dir || exit 1

    make release=1 Receiver WavSender
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

buildall()
{
    echo "Building all in $topdir"
    build_ohNet
    build_ohNetGenerated
    build_ohdevtools
    build_ohTopology
    build_ohSongcast
}

if test $opt_c -ne 0; then
    test -d $topdir/ohNet && fatal target dir should be initially empty \
                                   for producing a tar distribution
    clone_oh || fatal clone failed
    make_tarfile || fatal make_tarfile failed
    exit 0
fi

# Extract tar, or clone git repos
if test $opt_t -eq 1; then
    echo "Extracting tarfile in $topdir"
    cd $topdir || exit 1
    tar xf $tarfile
elif test $opt_b -eq 1; then
    clone_oh
fi

buildall
