/* Copyright (C) 2015 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#include "fiforeader.h"

#include <iostream>
#include <string>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "log.h"
#include "audioutil.h"

using namespace std;

FifoReader::FifoReader(const string& fn, int sampleRate, int bitsPerSample,
                       int chans, bool needswap, bool blocking)
    : m_fn(fn), m_needswap(needswap), m_blocking(blocking),
      m_fd(-1), m_tmpbuf(0), m_tmpbufsize(0)
{
    setAudioParams(sampleRate, bitsPerSample, chans);
}

FifoReader::~FifoReader()
{
    if (m_fd >= 0)
        ::close(m_fd);
    
    if (m_tmpbuf)
        free(m_tmpbuf);
}

bool FifoReader::open()
{
    LOGDEB("FifoReader::open: blocking: " << m_blocking << endl);
    
    int flags = m_blocking ? O_RDONLY : O_RDONLY|O_NONBLOCK;
    if ((m_fd = ::open(m_fn.c_str(), flags)) < 0) {
        LOGERR("open() errno " << errno << " on " << m_fn << endl);
        return false;
    }
    return true;
}


const unsigned char *FifoReader::data(size_t size)
{
    //LOGDEB("FifoReader: data " << size << " bytes. blocking " << m_blocking
    // << endl);
    if (m_tmpbufsize < size) {
        m_tmpbuf = (unsigned char *)realloc(m_tmpbuf, size);
        m_tmpbufsize = size;
    }

    if (m_blocking) {
        size_t remaining = size;
        while (remaining) {
            ssize_t nread = read(m_fd, m_tmpbuf + size - remaining, remaining);
            if (nread <= 0) {
                LOGERR("FifoReader::read: ret " << nread << " errno " <<
                       errno << " : " << strerror(errno) << endl);
                return 0;
            } else {
                remaining -= nread;
            }
        }
    } else {
        ssize_t nread = read(m_fd, m_tmpbuf, size);
        if (nread != ssize_t(size)) {
            if (nread >= 0 || errno == EAGAIN) {
                if (nread < 0)
                    nread = 0;
                //LOGDEB("FifoReader::data: inserting " << size - nread <<
                //       " bytes\n");
                memset(m_tmpbuf + nread, 0, size - nread);
            } else {
                LOGERR("FifoReader::read: ret " << nread << " errno " <<
                       errno << " : " << strerror(errno) << endl);
                return 0;
            }
        }
    }
    
    if (m_needswap)
        swapSamples(m_tmpbuf, bytesPerSample(), m_tmpbufsize / bytesPerSample());

    //LOGDEB("FifoReader: data done" << endl);
    return m_tmpbuf;
}
