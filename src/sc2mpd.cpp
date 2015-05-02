/*
Copyright 2011-14, Linn Products Ltd. All rights reserved.

Unless otherwise stated, all code in this project is licensed under the 2-clause
(Simplified) BSD license.  See BsdLicense.txt for details.

*/
/* Copyright (C) 2014 J.F.Dockes
 *	 This program is free software; you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2 of the License, or
 *	 (at your option) any later version.
 *
 *	 This program is distributed in the hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	 GNU General Public License for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the
 *	 Free Software Foundation, Inc.,
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Modified from ohSongcast/Receiver/Receiver.cpp
 */
#include "config.h"

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/OptionParser.h>
#include <OpenHome/Private/Debug.h>
#include <OpenHome/Net/Core/OhNet.h>

#include "Debug.h"
#include "OhmReceiver.h"

#include "workqueue.h"
#include "rcvqueue.h"
#include "log.h"
#include "conftree.h"
#include "chrono.h"

#include <vector>
#include <stdio.h>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

WorkQueue<AudioMessage*> audioqueue("audioqueue", 2);

#ifdef _WIN32

#pragma warning(disable:4355) // use of 'this' in ctor lists safe in this case

#define CDECL __cdecl

int mygetch()
{
    return (_getch());
}

#else

#define CDECL

#include <termios.h>
#include <unistd.h>

int mygetch()
{
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

#endif


using namespace OpenHome;
using namespace OpenHome::Net;
using namespace OpenHome::TestFramework;
using namespace OpenHome::Av;


class OhmReceiverDriver : public IOhmReceiverDriver, public IOhmMsgProcessor {
public:
    OhmReceiverDriver(int port);

private:
    // IOhmReceiverDriver
    virtual void Add(OhmMsg& aMsg);
    virtual void Timestamp(OhmMsg& aMsg);
    virtual void Started();
    virtual void Connected();
    virtual void Playing();
    virtual void Disconnected();
    virtual void Stopped();

    // IOhmMsgProcessor
    virtual void Process(OhmMsgAudio& aMsg);
    virtual void Process(OhmMsgTrack& aMsg);
    virtual void Process(OhmMsgMetatext& aMsg);

private:
    // Debug, stats, etc while we get to understand the Songcast streams
    class Observer {
    public:
        TBool iReset;
        TUint iCount;
        TUint iFrame;
        int dumpfd;
        Chrono chron;
        Observer() : iReset(true), iCount(0), iFrame(0), dumpfd(-1) {
#if 0
            dumpfd = 
                open("/y/av/tmp/sc2dump", O_WRONLY|O_CREAT|O_TRUNC, 0666);
            if (dumpfd < 0) {
                LOGERR("OhmReceiverDriver::Open dump file failed\n");
            }
#endif
        }

        void reset() {
            iReset = true;
        }

        void process(OhmMsgAudio& aMsg);
    };
    Observer obs;
};

OhmReceiverDriver::OhmReceiverDriver(int port)
{
    AudioEaterContext *ctxt = new AudioEaterContext(port);
    audioqueue.start(1, &audioEater, ctxt);
}

void OhmReceiverDriver::Add(OhmMsg& aMsg)
{
    aMsg.Process(*this);
    aMsg.RemoveRef();
}

void OhmReceiverDriver::Timestamp(OhmMsg& /*aMsg*/)
{
}

void OhmReceiverDriver::Started()
{
    LOGDEB("=== STARTED ====\n");
}

void OhmReceiverDriver::Connected()
{
    obs.reset();
    LOGDEB("=== CONNECTED ====\n");
}

void OhmReceiverDriver::Playing()
{
    LOGDEB("=== PLAYING ====\n");
    printf("PLAYING\n");
    fflush(stdout);
}

void OhmReceiverDriver::Disconnected()
{
    LOGDEB("=== DISCONNECTED ====\n");
}

void OhmReceiverDriver::Stopped()
{
    LOGDEB("=== STOPPED ====\n");
}

void OhmReceiverDriver::Observer::process(OhmMsgAudio& aMsg)
{
    if (++iCount == 400 || aMsg.Halt()) {
        static unsigned long long last_timestamp;
        unsigned long long timestamp = aMsg.MediaTimestamp();
        LOGDEB("OhmRcvDrv::Process:audio: samplerate " << aMsg.SampleRate() <<
               " bitdepth " << aMsg.BitDepth() << " channels " <<
               aMsg.Channels() << " samples " << aMsg.Samples() << 
               " Halted ? " << aMsg.Halt() << endl);

        if (last_timestamp) {
            long long intervalus = 
                ((timestamp - last_timestamp) * 1000000) / (256*48000);
            long long atsus = 
                ((timestamp) * 1000000) / (256*48000);
            long long absus = chron.amicros() - 1430477861905884LL;
            LOGDEB("Computed-uS: " << intervalus  << 
                   " Elapsed-uS: " << chron.urestart() << 
                   " Timestamp-uS: " << atsus <<
                   " Abs-uS: " << absus << 
                   " Diff-mS " << (absus - atsus) / 1000 <<
                   endl);
        }
        last_timestamp = timestamp;

        if (!aMsg.Halt()) {
            unsigned int bytes = 
                aMsg.Samples() * (aMsg.BitDepth() / 8) * aMsg.Channels();

            if (bytes != aMsg.Audio().Bytes()) {
                LOGERR("OhmRcvDrv::Process:audio: computed bytes " << bytes << 
                       " !=  bufer's " << aMsg.Audio().Bytes() << endl);
                bytes = aMsg.Audio().Bytes();
            }
            const unsigned char *icp = 
                (const unsigned char *)aMsg.Audio().Ptr();
            bool silence = true;
            for (unsigned int i = 0; i < bytes; i++) {
                if (icp[i]) {
                    silence = false;
                    break;
                }
            }
            if (silence) {
                LOGDEB("OhmRcvDrv::Process:audio: silence buffer" << endl);
            }
            if (dumpfd >= 0) {
                if (write(dumpfd, icp, bytes) != int(bytes)) {
                    ;
                }
            }
        }

        iCount = 0;
    }

    if (iReset) {
        iFrame = aMsg.Frame();
        iReset = false;
    } else {
        if (aMsg.Frame() != iFrame + 1) {
            LOGINF("Missed frames between " << iFrame << " and " << 
                    aMsg.Frame());
        }
        iFrame = aMsg.Frame();
    }
}

void OhmReceiverDriver::Process(OhmMsgAudio& aMsg)
{

    if (aMsg.Audio().Bytes() == 0) {
        LOGDEB("OhmReceiverDriver::Process: empty message\n");
        return;
    }

    unsigned int bytes = aMsg.Audio().Bytes();
    char *buf = (char *)malloc(bytes);
    if (buf == 0) {
        LOGERR("OhmReceiverDriver::Process: can't allocate " << 
               bytes << " bytes\n");
        return;
    }

#ifdef WORDS_BIGENDIAN
    memcpy(buf, aMsg.Audio().Ptr(), bytes);
#else
    if (aMsg.BitDepth() == 16) {
        swab(aMsg.Audio().Ptr(), buf, bytes);
    } else if (aMsg.BitDepth() == 24) {
        unsigned char *ocp = (unsigned char *)buf;
        const unsigned char *icp = (const unsigned char *)aMsg.Audio().Ptr();
        const unsigned char *icp0 = icp;
        while (icp - icp0 <= int(bytes) - 3) {
            *ocp++ = icp[2];
            *ocp++ = icp[1];
            *ocp++ = *icp;
            icp += 3;
        }
    } else if (aMsg.BitDepth() == 32) {
        // Never seen this but whatever...
        unsigned char *ocp = (unsigned char *)buf;
        const unsigned char *icp = (const unsigned char *)aMsg.Audio().Ptr();
        const unsigned char *icp0 = icp;
        while (icp - icp0 <= int(bytes) - 4) {
            *ocp++ = icp[3];
            *ocp++ = icp[2];
            *ocp++ = icp[1];
            *ocp++ = *icp;
            icp += 4;
        }
    }
#endif

    AudioMessage *ap = new 
        AudioMessage(aMsg.BitDepth(), aMsg.Channels(), aMsg.Samples(),
                     aMsg.SampleRate(), buf);

    // There is nothing special we can do if put fails: no way to
    // return status. Should we just exit ?
    if (!audioqueue.put(ap, true)) {
    }
}

void OhmReceiverDriver::Process(OhmMsgTrack& aMsg)
{
    LOGDEB("OhmRcvDrv::Process:trk: TRACK SEQ " << aMsg.Sequence() << endl);
    Brhz uri(aMsg.Uri());
    LOGDEB("OhmRcvDrv::Process:trk: TRACK URI " << uri.CString() << endl);
    Brhz metadata(aMsg.Metadata());
    LOGDEB("OhmRcvDrv::Process:trk: TRACK METADATA " << metadata.CString() 
           << endl);
}

void OhmReceiverDriver::Process(OhmMsgMetatext& aMsg)
{
    LOGDEB("OhmRcvDrv::Process:meta: METATEXT SEQUENCE " <<  aMsg.Sequence() 
           << endl);
    Brhz metatext(aMsg.Metatext());
    LOGDEB("OhmRcvDrv::Process:meta: METATEXT " << metatext.CString() << endl);
}

int CDECL main(int aArgc, char* aArgv[])
{
    string logfilename;
    int loglevel(Logger::LLINF);

    OptionParser parser;

    OptionUint optionAdapter("-a", "--adapter", 0, 
                             "[adapter] index of network adapter to use");
    parser.AddOption(&optionAdapter);

    OptionUint optionTtl("-t", "--ttl", 1, "[ttl] ttl");
    parser.AddOption(&optionTtl);

    OptionUint optionInteract("-i", "--interact", 0, "[interact] interactive");
    parser.AddOption(&optionInteract);

    OptionString optionUri("-u", "--uri", Brn("mpus://0.0.0.0:0"), 
                           "[uri] uri of the sender");
    parser.AddOption(&optionUri);

    OptionString optionConfig("-c", "--config", Brn("/etc/upmpdcli.conf"), 
                              "[config] upmpdcli configuration file path");
    parser.AddOption(&optionConfig);

    if (!parser.Parse(aArgc, aArgv)) {
        return (1);
    }

    InitialisationParams* initParams = InitialisationParams::Create();

    Library* lib = new Library(initParams);

    std::vector<NetworkAdapter*>* subnetList = lib->CreateSubnetList();
    TIpAddress subnet = (*subnetList)[optionAdapter.Value()]->Subnet();
    TIpAddress adapter = (*subnetList)[optionAdapter.Value()]->Address();
    Library::DestroySubnetList(subnetList);


    TUint ttl = optionTtl.Value();
    Brhz uri(optionUri.Value());

    string uconfigfile = (const char *)optionConfig.Value().Ptr();

    bool cfspecified = true;
    if (uconfigfile.empty()) {
        cfspecified = false;
        uconfigfile = "/etc/upmpdcli.conf";
    }
    ConfSimple config(uconfigfile.c_str(), 1, true);
    if (!config.ok()) {
        cerr << "Could not open config: " << uconfigfile << endl;
        if (cfspecified)
            return 1;
    }

    int port = 8768;
    string value;
    if (config.get("schttpport", value)) {
        port = atoi(value.c_str());
    }
    config.get("sclogfilename", logfilename);
    if (config.get("scloglevel", value))
        loglevel = atoi(value.c_str());
    if (Logger::getTheLog(logfilename) == 0) {
        cerr << "Can't initialize log" << endl;
        return 1;
    }
    Logger::getTheLog("")->setLogLevel(Logger::LogLevel(loglevel));

    LOGINF("scmpdcli: using subnet " << (subnet & 0xff) << "." << 
           ((subnet >> 8) & 0xff) << "." << ((subnet >> 16) & 0xff) << "." <<
           ((subnet >> 24) & 0xff) << endl);

    OhmReceiverDriver* driver = new OhmReceiverDriver(port);

    OhmReceiver* receiver = new OhmReceiver(lib->Env(), adapter, ttl, *driver);

    CpStack* cpStack = lib->StartCp(subnet);
    cpStack = cpStack; // avoid unused variable warning

    Debug::SetLevel(Debug::kMedia);

    if (optionInteract.Value()) {
        printf("q = quit\n");
        for (;;) {
            int key = mygetch();

            if (key == 'q') {
                printf("QUIT\n");
                break;
            } else if (key == 'p') {
                printf("PLAY %s\n", uri.CString());
                receiver->Play(uri);
            } else if (key == 's') {
                printf("STOP\n");
                receiver->Stop();
            }
        }
    } else {
        receiver->Play(uri);
        for (;;) {
            sleep(1000);
        }
    }

    delete(receiver);

    delete lib;

    if (optionInteract.Value())
        printf("\n");

    return (0);
}
