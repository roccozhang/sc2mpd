# dir over all OpenHome dirs
TOPOH=@TOPOH@/

OTHERLIBS=@OTHERLIBS@

OHBTYPE=Release

# Top ohSongcast dir
TOPSC = $(TOPOH)ohSongcast/
# Internal path to obj inside ohNet, ohSongcast etc.
OBJIPATH = Build/Obj/Posix/$(OHBTYPE)/
# ohSongcast other objects
TOPSCO=$(TOPSC)$(OBJIPATH)


AM_CPPFLAGS = -DDEBUG -g -Wall \
            -DDATADIR=\"${pkgdatadir}\" -DCONFIGDIR=\"${sysconfdir}\" \
            -DPACKAGE_VERSION=\"${PACKAGE_VERSION}\" \
            -I$(TOPOH)ohNet/Build/Include/ \
            -I$(TOPOH)ohNetGenerated/Build/Include/ \
            -I$(TOPOH)ohTopology/build/Include/  \
            -I$(TOPSC)

AM_CXXFLAGS = -std=c++0x

bin_PROGRAMS = sc2mpd

# Maybe these are needed for the oh headers ?
OHCCFLAGS = -D_GNU_SOURCE -D_REENTRANT -DDEFINE_LITTLE_ENDIAN \
          -DDEFINE_TRACE \
          -DDllImport="__attribute__ ((visibility(\"default\")))" \
          -DDllExport="__attribute__ ((visibility(\"default\")))" \
          -DDllExportClass="__attribute__ ((visibility(\"default\")))"

OTHEROBJECTS = $(TOPSCO)/Ohm.o $(TOPSCO)/OhmMsg.o $(TOPSCO)/OhmSocket.o \
             $(TOPSCO)/OhmReceiver.o $(TOPSCO)/OhmProtocolMulticast.o \
             $(TOPSCO)/OhmProtocolUnicast.o \
             $(TOPOH)ohNetGenerated/$(OBJIPATH)DvAvOpenhomeOrgReceiver1.o

sc2mpd_LDADD = $(OTHEROBJECTS) $(TOPOH)ohNet/$(OBJIPATH)libohNetCore.a \
     $(TOPOH)ohNet/$(OBJIPATH)libTestFramework.a $(OTHERLIBS)

sc2mpd_SOURCES = \
     ohbuild.sh \
     src/conftree.cpp \
     src/conftree.h \
     src/httpgate.cpp \
     src/log.cpp \
     src/log.h \
     src/ptmutex.h \
     src/rcvqueue.h \
     src/sc2mpd.cpp \
     src/wav.cpp \
     src/wav.h \
     src/workqueue.h
     
