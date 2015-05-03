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
 */
#include "config.h"

#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <iostream>
#include <queue>

#include <microhttpd.h>

#include "log.h"
#include "rcvqueue.h"
#include "wav.h"

using namespace std;

/** 
   Implement web server to export the audio stream. I had to jump
   through many hoops to get this to work with mpd (probably because
   of audiofile/sndfile restrictions). None of these are necessary
   with VLC which is quite happy if we just serve any wav header
   directly followed by audio data. 

   The trick to get this to work with libsndfile was apparently to
   send a Content-Size header. If we don't, the lib tries to seek
   inside the file (which I tried to emulate by accepting ranges), but
   fails to parse it anyway, which is quite probably a bug in
   libsndfile (does not see files with unknown size often...).
*/

// Only accept HTTP connections from localhost
#define ACCEPT_LOCALONLY 1

static const int dataformat_wav = 1;

#ifndef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

// The queue for audio blocks out of SongCast
static queue<AudioMessage*> dataqueue;
static PTMutexInit dataqueueLock;
static pthread_cond_t dataqueueWaitCond = PTHREAD_COND_INITIALIZER;

// Bogus data size for our streams. Total size is databytes+44 (header)
const unsigned int databytes = 2 * 1000 * 1000 * 1000;

// Used this while trying to emulate ranges, did not do the trick
struct ReadContext {
    ReadContext(long long o = 0) : baseoffset(o) {}
    long long baseoffset;
};

static const char *ValueKindToCp(enum MHD_ValueKind kind)
{
    switch (kind) {
    case MHD_RESPONSE_HEADER_KIND: return "Response header";
    case MHD_HEADER_KIND: return "HTTP header";
    case MHD_COOKIE_KIND: return "Cookies";
    case MHD_POSTDATA_KIND: return "POST data";
    case MHD_GET_ARGUMENT_KIND: return "GET (URI) arguments";
    case MHD_FOOTER_KIND: return "HTTP footer";
    default: return "Unknown";
    }
}

#ifdef PRINT_KEYS
static int print_out_key (void *cls, enum MHD_ValueKind kind, 
                          const char *key, const char *value)
{
    LOGDEB(ValueKindToCp(kind) << ": " << key << " -> " << value << endl);
    return MHD_YES;
}
#endif /* PRINT_KEYS */

// This gets called by microhttpd when it needs data.
static ssize_t
data_generator(void *cls, uint64_t pos, char *buf, size_t max)
{
    ReadContext *rc = (ReadContext *)cls;
    //LOGDEB("data_generator: baseoffs " <<
    //        rc->baseoffset << " pos " << pos << " max " << max << endl);
    if (rc->baseoffset + pos >= databytes + 44) {
        LOGDEB("data_generator: returning EOS" << endl);
        return MHD_CONTENT_READER_END_OF_STREAM;
    }

    PTMutexLocker lock(dataqueueLock);
    // We only do the offset-fixing thing once per called because it's
    // based on the (unchanging) input parameters.
    bool offsetfixed = false;
    size_t bytes = 0;
    while (bytes < max) {
        while (dataqueue.empty()) {
            //LOGDEB("data_generator: waiting for buffer" << endl);
            pthread_cond_wait(&dataqueueWaitCond, lock.getMutex());
            if (!dataqueue.empty() && dataqueue.front()->m_buf == 0) {
                // special buf, to be processed ?
                LOGINF("data_generator: deleting empty buf" << endl);
                delete dataqueue.front();
                dataqueue.pop();
            }
        }

        AudioMessage *m = dataqueue.front();

        // After initial ops to read the header, our client usually
        // restarts reading the stream. We can't rewind, so we
        // simulate rewind by discarding any partial buffer, to be
        // sure that we start aligned with the sample (we could also
        // use the sample size and channel count to adjust the offset,
        // but this is simpler). This is a bit of a hack, because it
        // won't work if the client does not behave as we expect
        // (seeks to a position not exactly after the header). The
        // proper solution would be to compare the buffer position and
        // read offsets modulos against chansXsamplesize and adjust
        // the offset (back or forward dep on avail data) so they are
        // the same.
        if (rc->baseoffset + pos == 44 && 
            m->m_curoffs != 0 && !offsetfixed) {
            //LOGDEB1("data_generator: FIXING OFFSET" << endl);
            m->m_curoffs = m->m_bytes;
            offsetfixed = true;
        }
        if (dataformat_wav && rc->baseoffset == 0 && pos == 0 && bytes == 0) {
            LOGINF("data_generator: first buf" << endl);
            // Using buf+bytes in case we ever insert icy before the audio
            int sz = makewavheader(buf+bytes, max, 
                                   m->m_freq, m->m_bits, m->m_chans, databytes);
            bytes += sz;
        }

        size_t newbytes = MIN(max - bytes, m->m_bytes - m->m_curoffs);
        memcpy(buf + bytes, m->m_buf + m->m_curoffs, newbytes);
        m->m_curoffs += newbytes;
        bytes += newbytes;
        if (m->m_curoffs == m->m_bytes) {
            delete dataqueue.front();
            dataqueue.pop();
        }
    }
    //LOGDEB("data_generator: returning " << bytes << " bytes" << endl);
    return bytes;
}

// Parse range header. 
static void parseRanges(const string& ranges, vector<pair<int,int> >& oranges)
{
    oranges.clear();
    string::size_type pos = ranges.find("bytes=");
    if (pos == string::npos) {
        return;
    }
    pos += 6;
    bool done = false;
    while(!done) {
        string::size_type dash = ranges.find('-', pos);
        string::size_type comma = ranges.find(',', pos);
        string firstPart = dash != string::npos ? 
            ranges.substr(pos, dash-pos) : "";
        int start = firstPart.empty() ? 0 : atoi(firstPart.c_str());
        string secondPart = dash != string::npos ? 
            ranges.substr(dash+1, comma != string::npos ? 
                          comma-dash-1 : string::npos) : "";
        int fin = secondPart.empty() ? -1 : atoi(firstPart.c_str());
        pair<int,int> nrange(start,fin);
        oranges.push_back(nrange);
        if (comma != string::npos) {
            pos = comma + 1;
        }
        done = comma == string::npos;
    }
}

static void ContentReaderFreeCallback(void *cls)
{
    ReadContext *rc = (ReadContext*)cls;
    delete rc;
}

static int answer_to_connection(void *cls, struct MHD_Connection *connection, 
                                const char *url, 
                                const char *method, const char *version, 
                                const char *upload_data, 
                                size_t *upload_data_size, void **con_cls)
{
    LOGDEB("answer_to_connection: url " << url << " method " << method << 
           " version " << version << endl);

#ifdef PRINT_KEYS
    MHD_get_connection_values(connection, MHD_HEADER_KIND, &print_out_key, 0);
#endif

    static int aptr;
    if (&aptr != *con_cls) {
        /* do not respond on first call */
        *con_cls = &aptr;
        return MHD_YES;
    }

    const char* rangeh = MHD_lookup_connection_value(connection, 
                                                    MHD_HEADER_KIND, "range");
    vector<pair<int,int> > ranges;
    if (rangeh) {
        parseRanges(rangeh, ranges);
    }

    long long size = MHD_SIZE_UNKNOWN;
    ReadContext *rc = new ReadContext();
    if (ranges.size()) {
        if (ranges[0].second != -1) {
            size = ranges[0].second - ranges[0].first + 1;
        }
        rc->baseoffset = ranges[0].first;
        //LOGDEB("httpgate: answer: got " << ranges.size() << 
        //     " ranges: range[0]: offs " << rc->baseoffset <<
        //     " sz " << size << endl);
    }

    // the block size is flatly ignored by libcurl. 5280 is 440x3
    // (thats the songcast msg size in 24 bits mode. Any random value
    // would probably work the same
    struct MHD_Response *response = 
        MHD_create_response_from_callback(size, 5292, &data_generator, 
                                          rc, ContentReaderFreeCallback);
    if (response == NULL) {
        LOGERR("httpgate: answer: could not create response" << endl);
        return MHD_NO;
    }
    if (dataformat_wav) {
        MHD_add_response_header (response, "Content-Type", "audio/x-wav");
    } else {
        // Could not get this to work with mpd??
        MHD_add_response_header (response, "Content-Type", 
                                 "audio/x-mpd-cdda-pcm-reverse");
    }
    char cl[30];
    sprintf(cl, "%d", databytes+44);
    MHD_add_response_header (response, "Content-Length", cl);
    MHD_add_response_header (response, "Accept-Ranges", "bytes");
    //MHD_add_response_header (response, "icy-metaint", "32768");

    int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

static int accept_policy(void *, const struct sockaddr* sa,
                         socklen_t addrlen)
{
    static struct sockaddr_in localaddr;
    static bool init = false;
    if (!init) {
        inet_pton(AF_INET, "127.0.0.1", &localaddr.sin_addr);
        init = true;
    }

    if (!sa || sa->sa_family != AF_INET) {
        LOGERR("httpgate:accept_policy: not AF_INET" << endl);
        return MHD_NO;
    }

#if ACCEPT_LOCALONLY != 0
    const struct sockaddr_in *sain = (const struct sockaddr_in*)sa;
    if (sain->sin_addr.s_addr != localaddr.sin_addr.s_addr) {
        LOGERR("httpgate:accept_policy: not localhost" << endl);
        return MHD_NO;
    }
#endif
    return MHD_YES;
}


static void *audioEater(void *cls)
{
    AudioEater::Context *ctxt = (AudioEater::Context*)cls;

    LOGDEB("audioEater: queue " << ctxt->queue << " HTTP port " << ctxt->port 
           << endl);

    struct MHD_Daemon *daemon = 
        MHD_start_daemon(
            MHD_USE_THREAD_PER_CONNECTION,
            //MHD_USE_SELECT_INTERNALLY, 
            ctxt->port, 
            /* Accept policy callback and arg */
            accept_policy, NULL, 
            /* handler and arg */
            &answer_to_connection, NULL, 
            MHD_OPTION_END);
    if (NULL == daemon) {
        ctxt->queue->workerExit();
        return (void *)0;
    }

    WorkQueue<AudioMessage*> *queue = ctxt->queue;
    delete ctxt;
    while (true) {
        AudioMessage *tsk = 0;
        size_t qsz;
        if (!queue->take(&tsk, &qsz)) {
            MHD_stop_daemon (daemon);
            queue->workerExit();
            return (void*)1;
        }
        PTMutexLocker lock(dataqueueLock);

        /* limit size of queuing. If there is a client but it is not
           eating blocks fast enough, there will be skips */
        while (dataqueue.size() > 2) {
            LOGDEB("audioEater: discarding buffer !" << endl);
            delete dataqueue.front();
            dataqueue.pop();
        }

        dataqueue.push(tsk);
        pthread_cond_broadcast(&dataqueueWaitCond);
        pthread_yield();
    }
}

AudioEater httpAudioEater(AudioEater::BO_LSB, &audioEater);
