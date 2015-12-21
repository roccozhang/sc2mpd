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

#ifndef _FIFOREADER_H_INCLUDED_
#define _FIFOREADER_H_INCLUDED_

#include "audioreader.h"

#include <string>

// This just somewhat encapsulates the code in the original
// WavSender.cpp The file is still read and converted in memory at
// once, and held in a memory array
class FifoReader : public AudioReader {
public:
    FifoReader(const std::string& fn, int chans,
               int sampleRate, int bitsPerSample, bool needswap, bool blocking);

    ~FifoReader();
    bool open();
    bool isblocking() {return m_blocking;}
    const unsigned char *data(size_t size);

private:
    std::string m_fn;
    bool m_needswap;
    bool m_blocking;
    int m_fd;
    unsigned char *m_tmpbuf;
    size_t m_tmpbufsize;
};

#endif /* _FIFOREADER_H_INCLUDED_ */
