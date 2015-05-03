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
#include <sys/types.h>

#include <iostream>
#include <queue>
#include <alsa/asoundlib.h>

#include <samplerate.h>

#include "log.h"
#include "rcvqueue.h"

using namespace std;

#ifndef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

// The queue for audio blocks ready for alsa
static const unsigned int qs = 200;
static const unsigned int qt = qs/2;
// the 40 value should be computed from the alsa buffer size. It's
// there becausee we have a jump on the first alsa write (alsa buffer
// is empty).
static const unsigned int qit = qs/2 + 40;

static WorkQueue<AudioMessage*> alsaqueue("alsaqueue", qs);
static snd_pcm_t *pcm;

static void *alsawriter(void *p)
{
    if (!alsaqueue.waitminsz(qit)) {
        LOGERR("alsawriter: waitminsz failed\n");
        return (void *)1;
    }
    while (true) {
        AudioMessage *tsk = 0;
        size_t qsz;
        if (!alsaqueue.take(&tsk, &qsz)) {
            // TBD: reset alsa?
            alsaqueue.workerExit();
            return (void*)1;
        }

        // Bufs 
        snd_pcm_uframes_t frames = 
            tsk->m_bytes / (tsk->m_chans * (tsk->m_bits/8));
        snd_pcm_sframes_t ret =  snd_pcm_writei(pcm, tsk->m_buf, frames);
        if (ret != int(frames)) {
            LOGERR("snd-cm_writei(" << frames <<" frames) failed: ret: " <<
                   ret << endl);
            if (ret < 0)
                snd_pcm_prepare(pcm);
//            return (void *)1;
        }
    }
}

static bool alsa_init(AudioMessage *tsk)
{
    snd_pcm_hw_params_t *hw_params;
    int err;
//    static const string dev("plughw:CARD=PCH,DEV=0");
    static const string dev("hw:2,0");
    const char *cmd = "";
    unsigned int actual_rate = tsk->m_freq;
    int dir=0;

    if ((err = snd_pcm_open(&pcm, dev.c_str(), 
                            SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        LOGERR("alsa_init: snd_pcm_open " << dev << " " << 
               snd_strerror(err) << endl);
        return false;;
    }
		   
    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        LOGERR("alsa_init: snd_pcm_hw_params_malloc " << 
               snd_strerror(err) << endl);
        snd_pcm_close(pcm);
        return false;
    }

    cmd = "snd_pcm_hw_params_any";
    if ((err = snd_pcm_hw_params_any(pcm, hw_params)) < 0) {
        goto error;
    }
	
    cmd = "snd_pcm_hw_params_set_access";
    if ((err = 
         snd_pcm_hw_params_set_access(pcm, hw_params, 
                                      SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        goto error;
    }

    cmd = "snd_pcm_hw_params_set_format";
    if ((err = 
         snd_pcm_hw_params_set_format(pcm, hw_params, 
                                      SND_PCM_FORMAT_S16_LE)) < 0) {
        goto error;
    }
    cmd = "snd_pcm_hw_params_set_rate_near";
    if ((err = snd_pcm_hw_params_set_rate_near(pcm, hw_params, 
                                               &actual_rate, &dir)) < 0) {
        goto error;
    }

    cmd = "snd_pcm_hw_params_set_channels";
    if ((err = snd_pcm_hw_params_set_channels(pcm, hw_params, 
                                              tsk->m_chans)) < 0) {
        goto error;
    }

    cmd = "snd_pcm_hw_params";
    if ((err = snd_pcm_hw_params(pcm, hw_params)) < 0) {
        goto error;
    }
	
    snd_pcm_hw_params_free(hw_params);
    return true;

error:
    snd_pcm_hw_params_free(hw_params);
    LOGERR("alsa_init: " << cmd << " " << snd_strerror(err) << endl);
    return false;;
}

static void *audioEater(void *cls)
{
    AudioEater::Context *ctxt = (AudioEater::Context*)cls;

    LOGDEB("alsaEater: queue " << ctxt->queue << endl);

    WorkQueue<AudioMessage*> *queue = ctxt->queue;
    delete ctxt;

    bool qinit = false;
    int src_error = 0;
    SRC_STATE *src_state = 0;
    SRC_DATA src_data;
    memset(&src_data, 0, sizeof(src_data));
    alsaqueue.start(1, alsawriter, 0);
    float samplerate_ratio = 1.0;

    while (true) {
        AudioMessage *tsk = 0;
        size_t qsz;
        if (!queue->take(&tsk, &qsz)) {
            // TBD: reset alsa?
            queue->workerExit();
            return (void*)1;
        }

        if (src_state == 0) {
            if (!alsa_init(tsk)) {
                queue->workerExit();
                return (void *)1;
            }
            // BEST_QUALITY yields approx 25% cpu on a core i7
            // 4770T. Obviously too much, actually might not be
            // sustainable.
            // MEDIUM_QUALITY is around 10%
            // FASTEST is 4-5%. Given that this is process-wide, probably
            // a couple % in fact.
            // To be re-evaluated on the pi...
            src_state = src_new(SRC_SINC_FASTEST, tsk->m_chans, &src_error);
        }

        if (qinit) {
            float qs = alsaqueue.qsize();
            float t = ((qt - qs) / qt);
            float adj = t * t  / 10;
            if (alsaqueue.qsize() < qt) {
                samplerate_ratio =  1.0 + adj;
                if (samplerate_ratio > 1.1)
                    samplerate_ratio = 1.1;
            } else {
                samplerate_ratio -= adj;
                if (samplerate_ratio < 0.9) 
                    samplerate_ratio = 0.9;
            }
        }

        unsigned int tot_samples = tsk->m_bytes / (tsk->m_bits/8);
        if ((unsigned int)src_data.input_frames < tot_samples / tsk->m_chans) {
            int bytes = tot_samples * sizeof(float);
            src_data.data_in = (float *)realloc(src_data.data_in, bytes);
            src_data.data_out = (float *)realloc(src_data.data_out, 2 * bytes);
            src_data.input_frames = tot_samples / tsk->m_chans;
            // Available space for output
            src_data.output_frames = 2 * src_data.input_frames;
        }
        src_data.src_ratio = samplerate_ratio;
        src_data.end_of_input = 0;
        
        switch (tsk->m_bits) {
        case 16: {
            const short *sp = (const short *)tsk->m_buf;
            for (unsigned int i = 0; i < tot_samples; i++) {
                src_data.data_in[i] = *sp++;
            }
            break;
        }
        case 24:
        case 32:
        default:
            abort();
        }
        int ret = src_process(src_state, &src_data);
        if (ret) {
            LOGERR("src_process: " << src_strerror(ret) << endl);
            continue;
        }
        {
            static int cnt;
            if (cnt++ == 100) {
                LOGDEB("samplerate: " 
                       " qsize " << alsaqueue.qsize() << 
                       " ratio " << samplerate_ratio <<
                       " in " << src_data.input_frames << 
                       " consumed " << src_data.input_frames_used << 
                       " out " << src_data.output_frames_gen << endl);
                cnt = 0;
            }
        }
        tot_samples =  src_data.output_frames_gen * tsk->m_chans;
        if (src_data.output_frames_gen > src_data.input_frames) {
            tsk->m_bytes = tot_samples * (tsk->m_bits / 8);
            tsk->m_buf = (char *)realloc(tsk->m_buf, tsk->m_bytes);
            if (!tsk->m_buf) 
                abort();
        }

        // Output is always 16 bits lsb first for now. We should
        // probably dither the lsb ?
        tsk->m_bits = 16;
        {
#ifdef WORDS_BIGENDIAN
            unsigned char *ocp = (unsigned char *)tsk->m_buf;
            short val;
            unsigned char *icp = (unsigned char *)&val;
            for (unsigned int i = 0; i < tot_samples; i++) {
                val = src_data.data_out[i];;
                *ocp++ = icp[1];
                *ocp++ = icp[0];
            }
            tsk->m_bytes = ocp - tsk->m_buf;
#else
            short *sp = (short *)tsk->m_buf;
            for (unsigned int i = 0; i < tot_samples; i++) {
                *sp++ = src_data.data_out[i];
            }
            tsk->m_bytes = (char *)sp - tsk->m_buf;
#endif
        }

        if (!alsaqueue.put(tsk)) {
            LOGERR("alsaEater: queue put failed\n");
            return (void *)1;
        }
        if (alsaqueue.qsize() >= qit)
            qinit = true;
    }
}

AudioEater alsaAudioEater(AudioEater::BO_HOST, &audioEater);
