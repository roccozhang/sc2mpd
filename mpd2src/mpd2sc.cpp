#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Net/Core/DvDevice.h>
#include <OpenHome/Net/Core/OhNet.h>
#include <OpenHome/Net/Core/CpDevice.h>
#include <OpenHome/Net/Core/CpDeviceUpnp.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/OptionParser.h>
#include <OpenHome/Private/Debug.h>
#include <OpenHome/Os.h>
#include <OpenHome/Private/Env.h>

#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "OhmSender.h"

#include "log.h"
#include "icon.h"
#include "audioreader.h"
#include "openaudio.h"
#include "base64.hxx"

using namespace std;

using namespace OpenHome;
using namespace OpenHome::Net;
using namespace OpenHome::TestFramework;
using namespace OpenHome::Av;

class PcmSender {

public:
    static const TUint kPeriodMs = 10;
    static const TUint kSpeedNormal = 100;
    static const TUint kSpeedMin = 75;
    static const TUint kSpeedMax = 150;
    static const TUint kMaxPacketBytes = 4096;
        
public:
    PcmSender(Environment& aEnv, OhmSender* aSender,
              OhmSenderDriver* aDriver, const Brx& aUri,
              AudioReader* audio, bool paced);
    bool Start();
    void Pause();
    void Restart();
    ~PcmSender();
    void busyRdWr();
        
private:
    void CalculatePacketBytes();
    void TimerExpired();
        
private:
    Environment& iEnv;
    OhmSender* iSender;
    OhmSenderDriver* iDriver;
    Bws<OhmSender::kMaxTrackUriBytes> iUri;
    AudioReader *m_audio;
    Timer iTimer;
    Mutex iMutex;
    TBool iPaused;
    TUint iSpeed;           // percent, 100%=normal
    TUint iIndex;           // byte offset read position in source data
    TUint iPacketBytes;     // how many bytes of audio in each packet
    TUint iPacketFrames;    // how many audio frames in each packet
    TUint iPacketTime;      // how much audio time in each packet, uS
    TUint64 iLastTimeUs;    // last time stamp from system
    TInt32 iTimeOffsetUs;   // running offset in usec from ideal time
    //  <0 means sender is behind
    //  >0 means sender is ahead
    TBool iVerbose;
    TBool iPaced;
};

PcmSender::PcmSender(Environment& aEnv, OhmSender* aSender,
                     OhmSenderDriver* aDriver,
                     const Brx& aUri, AudioReader* audio, bool paced)
    : iEnv(aEnv)
    , iSender(aSender)
    , iDriver(aDriver)
    , iUri(aUri)
    , m_audio(audio)
    , iTimer(aEnv, MakeFunctor(*this, &PcmSender::TimerExpired), "PcmSender")
    , iMutex("WAVP")
    , iPaused(false)
    , iSpeed(kSpeedNormal)
    , iIndex(0)
    , iLastTimeUs(0)
    , iTimeOffsetUs(0)
    , iVerbose(false)
    , iPaced(paced)
      
{
    CalculatePacketBytes();
    LOGDEB("bytes per packet: " << iPacketBytes << endl);
    LOGDEB("frames per packet: " << iPacketFrames << endl);
    LOGDEB("usec per packet:   "<< iPacketTime << endl);
}

// We return true if the main thread should pause.
bool PcmSender::Start()
{
    iDriver->SetAudioFormat(m_audio->sampleRate(), m_audio->byteRate() * 8,
                            m_audio->numChannels(),
                            m_audio->bitsPerSample(), true, Brn("WAV"));
    iSender->SetEnabled(true);

    iSender->SetTrack(iUri, Brx::Empty(), m_audio->sampleCount(), 0);
    iSender->SetMetatext(Brn("PcmSender repeated play"));

    // It seems that both hijacking the main thread and using the
    // timer with a short timeout (see TimerExpired()) work.
    // Don't know what's best. The timer approach is closer to the original
    // code and leaves the main thread free for control ops if needed.
    // Otoh, if no data appears on the fifo, the timer thread is stuck in read
    // and the upnp side stops working (no sender advertised).
    // Maybe the best approach would be to start a separate thread and
    // use busyreading. Using the main thread for now.
    static const bool optionbusy = true;
    if (iPaced || !optionbusy) {
        LOGDEB("PcmSender::Start: using timers\n");
        iTimer.FireIn(kPeriodMs);
        return true;
    } else {
        LOGDEB("PcmSender::Start: block on reading only\n");
        busyRdWr();
        return false;
    }
}

void PcmSender::Pause()
{
    iMutex.Wait();

    if (iPaused) {
        iPaused = false;
        iLastTimeUs = 0;
        iTimeOffsetUs = 0;
        iTimer.FireIn(kPeriodMs);
    } else {
        iPaused = true;
    }
        
    iMutex.Signal();
}

void PcmSender::Restart()
{
    iMutex.Wait();
    iIndex = 0;
    iMutex.Signal();
}

void PcmSender::CalculatePacketBytes()
{
    // in order to let wavsender change the playback rate,
    // we keep constant it's idea of how much audio time is in each packet,
    // but vary the amount of data that is actually sent

    // calculate the amount of time in each packet
    TUint norm_bytes = (m_audio->sampleRate() * m_audio->bytesPerFrame() *
                        kPeriodMs) / 1000;
    if (norm_bytes > kMaxPacketBytes) {
        norm_bytes = kMaxPacketBytes;
    }
    TUint norm_packet_samples = norm_bytes / m_audio->bytesPerFrame();
    iPacketTime = (norm_packet_samples*1000000/(m_audio->sampleRate()/10) + 5)/10;

    // calculate the adjusted speed packet size
    TUint bytes = (norm_bytes * iSpeed) / 100;
    if (bytes > kMaxPacketBytes) {
        bytes = kMaxPacketBytes;
    }
    iPacketFrames = bytes / m_audio->bytesPerFrame();
    iPacketBytes = iPacketFrames * m_audio->bytesPerFrame();
}

void PcmSender::busyRdWr()
{
    LOGDEB("PcmSender:busyRdWr: packetbytes " << iPacketBytes << endl);
    while (true) {
        const unsigned char *cp = m_audio->data((size_t)iPacketBytes);
        if (cp == 0) {
            return;
        }
        iDriver->SendAudio(cp, iPacketBytes);
    }
}

void PcmSender::TimerExpired()
{
    iMutex.Wait();
        
    if (!iPaused) {
        TUint64 now = OsTimeInUs(iEnv.OsCtx());

        const unsigned char *cp = m_audio->data((size_t)iPacketBytes);
        if (cp == 0) {
            static bool sigsent = false;
            if (!sigsent) {
                LOGDEB("PcmSender::TimerExpired: killing myself\n");
                kill(getpid(), SIGUSR1);
                sigsent = true;
            }
        } else {
            iDriver->SendAudio(cp, iPacketBytes);
        }

        if (!iPaced) {
            // Means we're doing blocking reads on the source, and
            // it's setting the pace.  I'd like to actually use 0 here
            // (ala qt processEvents()), but this appears to busyloop
            // and not let the sender do its thing.  Anyway, as long
            // as we can read from the fifo in much less than (period-2),
            // which should always be true, we should be ok.
            // I can see not much difference between doing this or
            // hijacking the main thread for busy read/write
            iTimer.FireIn(2);
        } else {
            // skip the first packet, and any time the clock value wraps
            if (iLastTimeUs && iLastTimeUs < now) {

                // will contain the new time out in ms
                TUint new_timer_ms = kPeriodMs;

                // the difference in usec from where we should be
                TInt32 diff = (TInt32)(now - iLastTimeUs) - iPacketTime;

                // increment running offset
                iTimeOffsetUs -= diff;

                // determine new timer value based upon current offset from ideal
                if (iTimeOffsetUs < -1000) {
                    // we are late
                    TInt32 time_offset_ms = iTimeOffsetUs/1000;
                    if (time_offset_ms < 1-(TInt32)kPeriodMs) {
                        // in case callback is severely late, we can only
                        // catch up so much
                        new_timer_ms = 1;
                    } else {
                        new_timer_ms = kPeriodMs + time_offset_ms;
                    }
                } else if (iTimeOffsetUs > 1000) {
                    // we are early
                    new_timer_ms = kPeriodMs+1;
                } else {
                    // we are about on time
                    new_timer_ms = kPeriodMs;
                }

                // set timer
                iTimer.FireIn(new_timer_ms);

                // logging
                if (iVerbose) {
                    if (iTimeOffsetUs >= 1000)
                        printf ("tnow:%d tlast:%d actual:%4d diff:%4d offset:%5d timer:%d\n", (TUint)now, (TUint)iLastTimeUs, (TUint)(now-iLastTimeUs), diff, iTimeOffsetUs, new_timer_ms);
                    else
                        printf ("tnow:%d tlast:%d actual:%4d diff:%4d offset:%4d timer:%d\n", (TUint)now, (TUint)iLastTimeUs, (TUint)(now-iLastTimeUs), diff, iTimeOffsetUs, new_timer_ms);
                }
            } else {
                iTimer.FireIn(kPeriodMs);
            }
            iLastTimeUs = now;
        }
    }
        
    iMutex.Signal();
}

PcmSender::~PcmSender()
{
    iTimer.Cancel();
    delete (iSender);
    delete (iDriver);
}

// Sig catcher so that we can interrupt the pause() which will be waiting
// for playing to end
void sigcatcher(int)
{
    LOGDEB("sigcatcher\n");
}

int main(int aArgc, char* aArgv[])
{
    OptionParser parser;
    
    OptionString optionAudioParams("-A", "--audio", Brn(""), "[44100:16:2] audio params only if they can't be obtained from file. Conflicting values will cause error");
    parser.AddOption(&optionAudioParams);

    OptionUint optionAdapter("-a", "--adapter", 0, "[adapter] index of network adapter to use");
    parser.AddOption(&optionAdapter);

    OptionUint optionChannel("-c", "--channel", 0, "[0..65535] sender channel");
    parser.AddOption(&optionChannel);

    OptionBool optionDisabled("-d", "--disabled", "[disabled] start up disabled");
    parser.AddOption(&optionDisabled);

    OptionString optionFile("-f", "--file", Brn(""), "[file] wav file to send");
    parser.AddOption(&optionFile);
    
    OptionUint optionLatency("-l", "--latency", 100, "[latency] latency in ms");
    parser.AddOption(&optionLatency);

    OptionBool optionMulticast("-m", "--multicast", "[multicast] use multicast instead of unicast");
    parser.AddOption(&optionMulticast);

    OptionBool optionNeedPace("-p", "--pace", "Use internal timer to pace source. Implicit for regular files.");
    parser.AddOption(&optionNeedPace);
    
    OptionString optionName("-n", "--name", Brn("Openhome WavSender"), "[name] name of the sender");
    parser.AddOption(&optionName);

    OptionUint optionTtl("-t", "--ttl", 1, "[ttl] ttl");
    parser.AddOption(&optionTtl);

    OptionString optionUdn("-u", "--udn", Brn("12345678"), "[udn] udn for the upnp device");
    parser.AddOption(&optionUdn);

//    OptionBool optionPacketLogging("-z", "--logging", "[logging] toggle packet logging");
//    parser.AddOption(&optionPacketLogging);

    if (!parser.Parse(aArgc, aArgv)) {
        return (1);
    }

    InitialisationParams* initParams = InitialisationParams::Create();

    Library* lib = new Library(initParams);

    std::vector<NetworkAdapter*>* subnetList = lib->CreateSubnetList();
    LOGDEB("adapter list:\n");
    for (unsigned i=0; i<subnetList->size(); ++i) {
        TIpAddress addr = (*subnetList)[i]->Address();
        LOGDEB("  " << i << ": " << (addr&0xff) << "." <<
               ((addr>>8)&0xff) << "." << ((addr>>16)&0xff) << "." <<
               ((addr>>24)&0xff) << endl);
    }
    if (subnetList->size() <= optionAdapter.Value()) {
        LOGERR("ERROR: adapter " << optionAdapter.Value() << "doesn't exist\n");
        return (1);
    }

    TIpAddress subnet = (*subnetList)[optionAdapter.Value()]->Subnet();
    TIpAddress adapter = (*subnetList)[optionAdapter.Value()]->Address();
    Library::DestroySubnetList(subnetList);
    lib->SetCurrentSubnet(subnet);

    LOGDEB("using subnet " << (subnet&0xff) << "." << ((subnet>>8)&0xff) << "."
           << ((subnet>>16)&0xff) << "." <<  ((subnet>>24)&0xff) << endl);

    Brhz file(optionFile.Value());
    
    if (file.Bytes() == 0) {
        LOGERR("No input file specified\n");
        return (1);
    }
    
    Brhz udn(optionUdn.Value());
    Brhz name(optionName.Value());
    Brhz audioparams(optionAudioParams.Value());
    TUint channel = optionChannel.Value();
    TUint ttl = optionTtl.Value();
    TUint latency = optionLatency.Value();
    TBool multicast = optionMulticast.Value();
    TBool disabled = optionDisabled.Value();
    //TBool logging = optionPacketLogging.Value();
    TBool needpace = optionNeedPace.Value();

    AudioReader *audio = openAudio(file.CString(), audioparams.CString(),
                                   !needpace);
    if (!audio || !audio->open()) {
        cerr << "Audio file open failed" << endl;
        return 1;
    }
    needpace = !audio->isblocking();
    LOGDEB("sample rate:        " << audio->sampleRate() << endl);
    LOGDEB("sample size:        " << audio->bytesPerSample() << endl);
    LOGDEB("channels:           " << audio->numChannels() << endl);
    
    DvStack* dvStack = lib->StartDv();

    DvDeviceStandard* device = new DvDeviceStandard(*dvStack, udn);
    
    device->SetAttribute("Upnp.Domain", "av.openhome.org");
    device->SetAttribute("Upnp.Type", "Sender");
    device->SetAttribute("Upnp.Version", "1");
    device->SetAttribute("Upnp.FriendlyName", name.CString());
    device->SetAttribute("Upnp.Manufacturer", "Openhome");
    device->SetAttribute("Upnp.ManufacturerUrl", "http://www.openhome.org");
    device->SetAttribute("Upnp.ModelDescription", "Openhome WavSender");
    device->SetAttribute("Upnp.ModelName", "Openhome WavSender");
    device->SetAttribute("Upnp.ModelNumber", "1");
    device->SetAttribute("Upnp.ModelUrl", "http://www.openhome.org");
    device->SetAttribute("Upnp.SerialNumber", "");
    device->SetAttribute("Upnp.Upc", "");

    OhmSenderDriver* driver = new OhmSenderDriver(lib->Env());
    
    Brn icon(icon_png, icon_png_len);

    OhmSender* sender =
        new OhmSender(lib->Env(), *device, *driver, name, channel, adapter, ttl,
                      latency, multicast, !disabled, icon, Brn("image/png"), 0);
        
    PcmSender* pcmsender = new PcmSender(lib->Env(), sender, driver, file,
                                         audio, needpace);
    
    device->SetEnabled();

    const Brx& suri(sender->SenderUri());
    string uri((const char*)suri.Ptr(), suri.Bytes());
    const Brx& smeta(sender->SenderMetadata());
    string meta((const char*)smeta.Ptr(), smeta.Bytes());
    cout << "URI " << UPnPP::base64_encode(uri) <<
        " METADATA " << UPnPP::base64_encode(meta) << endl;
//    cout << "URI " << uri << " METADATA " << meta << endl;
    cout.flush();

    signal(SIGUSR1, sigcatcher);
    signal(SIGINT, sigcatcher);
    signal(SIGTERM, sigcatcher);
    if (pcmsender->Start()) {
        pause();
    }

    LOGDEB("Main: cleaning up\n");
    delete (pcmsender);

    delete (device);
    
    UpnpLibrary::Close();

    return (0);
}
