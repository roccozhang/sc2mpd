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

#ifndef _WAV_H_INCLUDED_
#define _WAV_H_INCLUDED_

#include "audioreader.h"

#include <string>

// This just somewhat encapsulates the code in the original
// WavSender.cpp The file is still read and converted in memory at
// once, and held in a memory array
class WavReader : public AudioReader {
public:
    WavReader(const std::string& fn)
        : m_fn(fn), m_fp(0), m_data(0), m_subChunk2Size(0), m_dataoffs(0),
          m_index(0), m_tmpbuf(0), m_tmpbufsize(0) {
    }
    ~WavReader() {
        if (m_fp)
            fclose(m_fp);
        if (m_data)
            free(m_data);
        if (m_tmpbuf)
            free(m_tmpbuf);
    }

    bool open();

    // Get pointer to data at offset. Caller is supposed to know how
    // much there is ahead.
    const unsigned char *data(size_t size);

    unsigned int subChunk2Size() {
        return m_subChunk2Size;
    }
    unsigned int sampleCount() {
        return m_subChunk2Size / bytesPerSample();
    }
    unsigned int totalBytes() {
        return m_subChunk2Size;
    }

private:
    bool readHeader();
    bool readData();
    
    std::string m_fn;
    FILE *m_fp;
    unsigned char *m_data;
    unsigned int m_byteRate;
    unsigned int m_subChunk2Size; // Data segment size in bytes
    off_t m_dataoffs; // Data offset in file

    off_t m_index; // Current position of reader
    unsigned char *m_tmpbuf;
    size_t m_tmpbufsize;
};

#endif /* _WAV_H_INCLUDED_ */
