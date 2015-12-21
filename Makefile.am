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

bin_PROGRAMS = sc2mpd mpd2sc

# Maybe these are needed for the oh headers ?
OHCCFLAGS = -D_GNU_SOURCE -D_REENTRANT -DDEFINE_LITTLE_ENDIAN \
          -DDEFINE_TRACE \
          -DDllImport="__attribute__ ((visibility(\"default\")))" \
          -DDllExport="__attribute__ ((visibility(\"default\")))" \
          -DDllExportClass="__attribute__ ((visibility(\"default\")))"

OTHEROSC2 = $(TOPSCO)/Ohm.o $(TOPSCO)/OhmMsg.o $(TOPSCO)/OhmSocket.o \
             $(TOPSCO)/OhmReceiver.o $(TOPSCO)/OhmProtocolMulticast.o \
             $(TOPSCO)/OhmProtocolUnicast.o \
             $(TOPOH)ohNetGenerated/$(OBJIPATH)DvAvOpenhomeOrgReceiver1.o

sc2mpd_LDADD = $(OTHEROSC2) $(TOPOH)ohNet/$(OBJIPATH)libohNetCore.a \
     $(TOPOH)ohNet/$(OBJIPATH)libTestFramework.a $(OTHERLIBS)

sc2mpd_SOURCES = \
     ohbuild.sh \
     sc2src/alsadirect.cpp \
     sc2src/chrono.cpp \
     sc2src/chrono.h \
     sc2src/conftree.cpp \
     sc2src/conftree.h \
     sc2src/httpgate.cpp \
     sc2src/log.cpp \
     sc2src/log.h \
     sc2src/ptmutex.h \
     sc2src/rcvqueue.h \
     sc2src/sc2mpd.cpp \
     sc2src/wav.cpp \
     sc2src/wav.h \
     sc2src/workqueue.h
     
OTHEROMP2 = $(TOPSCO)/Ohm.o $(TOPSCO)/OhmMsg.o $(TOPSCO)/OhmSocket.o \
             $(TOPSCO)/OhmSender.o \
             $(TOPOH)ohNetGenerated/$(OBJIPATH)DvAvOpenhomeOrgSender1.o

mpd2sc_CPPFLAGS = -I$(srcdir)/sc2src $(AM_CPPFLAGS) 
mpd2sc_LDADD = $(OTHEROMP2) $(TOPOH)ohNet/$(OBJIPATH)libohNetCore.a \
     $(TOPOH)ohNet/$(OBJIPATH)libTestFramework.a $(OTHERLIBS)

mpd2sc_SOURCES = \
     mpd2src/base64.cxx \
     mpd2src/fiforeader.cpp \
     mpd2src/openaudio.cpp \
     mpd2src/stringtotokens.cpp \
     mpd2src/mpd2sc.cpp \
     mpd2src/wavreader.cpp \
     sc2src/log.cpp
     
