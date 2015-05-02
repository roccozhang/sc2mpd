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
#ifndef _RCVQUEUE_H_INCLUDED_
#define _RCVQUEUE_H_INCLUDED_

#include "workqueue.h"

/* 
 * The audio messages which get passed between the songcast receiver
 * and the http server part. We could probably use ohSongcast own audio 
 * messages, but I prefer to stop the custom type usage asap.
 */
class AudioMessage {
public:
    // If buf is not 0, it is a malloced buffer, and we take
    // ownership. The caller MUST NOT free it. Its size must be at
    // least (bits/8) * chans * samples
    AudioMessage(unsigned int bits, unsigned int channels, unsigned int samples,
                 unsigned int sampfreq, char *buf = 0) 
        : m_bits(bits), m_chans(channels), m_freq(sampfreq),
          m_bytes(buf ? (bits/8) * channels * samples : 0), m_buf(buf),
          m_curoffs(0) {
    }

    ~AudioMessage() {
        if (m_buf)
            free(m_buf);
    }

    unsigned int m_bits;
    unsigned int m_chans;
    unsigned int m_freq;
    unsigned int m_bytes;
    char *m_buf;
    unsigned int m_curoffs; /* Used by the http data emitter */
};

extern WorkQueue<AudioMessage*> audioqueue;

/** Worker routine for fetching bufs from the rcvqueue and making them
 *  available to HTTP. the param is actually an AudioEaterContext */
extern void *audioEater(void *);

struct AudioEaterContext {
    AudioEaterContext(int p = 0) : port(p) {}
    int port;
};

#endif /* _RCVQUEUE_H_INCLUDED_ */
