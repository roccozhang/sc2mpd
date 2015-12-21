/*
Copyright 2012, OpenHome. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

THIS SOFTWARE IS PROVIDED BY OPENHOME ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation
are those of the authors and should not be interpreted as representing
official policies, either expressed or implied, of OpenHome.
*/

#include "wavreader.h"

#include <iostream>
#include <string.h>

#include "audioutil.h"
#include "log.h"

using namespace std;

bool WavReader::open()
{
    if (!readHeader()) {
        cerr << "Can't open file: " << m_fn << endl;
        return false;
    }
    if (!readData()) {
        cerr << "Can't read file: " << m_fn << endl;
        return false;
    }
    return true;
}

const unsigned char *WavReader::data(size_t packetbytes)
{
    if (m_index + packetbytes <= totalBytes()) {
        m_index += packetbytes;
        //cerr << "WavReader::data: " << packetbytes << " at " <<
        //m_index - packetbytes << endl;
        return m_data + m_index - packetbytes;
    } else if (totalBytes() == m_index) {
        m_index = packetbytes;
        return m_data;
    } else {
        unsigned int remaining = packetbytes - (totalBytes() - m_index);
        if (m_tmpbufsize < packetbytes) {
            m_tmpbuf = (unsigned char *)realloc(m_tmpbuf, packetbytes);
            m_tmpbufsize = packetbytes;
        }
        memcpy(m_tmpbuf, m_data + m_index, remaining);
        memcpy(m_tmpbuf + remaining, m_data, packetbytes - remaining);
        m_index = packetbytes - remaining;
        return m_tmpbuf;
    }
}

// Read WAV file
bool WavReader::readHeader()
{
    m_fp = fopen(m_fn.c_str(), "rb");
    
    if (m_fp == 0) {
        LOGDEB("Unable to open specified wav file\n");
        return false;
    }
    
    unsigned char header[44];
    
    size_t count = fread((void*)header, 1, 44, m_fp);
    
    if (count != 44) {
        LOGDEB("Unable to read the specified wav file\n");
        return false;
    }
    
    if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' ||
        header[3] != 'F') {
        LOGDEB("Invalid wav file\n");
        return false;
    }
    
    unsigned int header0, header1, header2, header3;

    header0 = header[4];
    header1 = header[5];
    header2 = header[6];
    header3 = header[7];

    // unsigned int chunkSize = header0 | (header1 << 8) | (header2 << 16) | (header3 << 24);
    
    if (header[8] != 'W' || header[9] != 'A' || header[10] != 'V' ||
        header[11] != 'E') {
        LOGDEB("Invalid wav file\n");
        return false;
    }
    
    if (header[12] != 'f' || header[13] != 'm' || header[14] != 't' ||
        header[15] != ' ') {
        LOGDEB("Invalid wav file\n");
        return false;
    }
    
    header0 = header[16];
    header1 = header[17];
    header2 = header[18];
    header3 = header[19];

    unsigned int subChunk1Size =
        header0 | (header1 << 8) | (header2 << 16) | (header3 << 24);
    
    if (subChunk1Size != 16) {
        LOGDEB("Unsupported wav file\n");
        return false;
    }
    
    header0 = header[20];
    header1 = header[21];

    unsigned int audioFormat = header0 | (header1 << 8);
    
    if (audioFormat != 1) {
        LOGDEB("Unsupported wav file\n");
        return false;
    }
    
    header0 = header[22];
    header1 = header[23];

    m_numChannels = header0 | (header1 << 8);
    
    header0 = header[24];
    header1 = header[25];
    header2 = header[26];
    header3 = header[27];

    m_sampleRate = header0 | (header1 << 8) | (header2 << 16) | (header3 << 24);
    
    header0 = header[28];
    header1 = header[29];
    header2 = header[30];
    header3 = header[31];

    m_byteRate = header0 | (header1 << 8) | (header2 << 16) | (header3 << 24);
    
    //header0 = header[32];
    //header1 = header[33];

    //unsigned int blockAlign = header0 | (header1 << 8);
    
    header0 = header[34];
    header1 = header[35];

    m_bitsPerSample = header0 | (header1 << 8);
    
    if (header[36] != 'd' || header[37] != 'a' || header[38] != 't' ||
        header[39] != 'a') {
        LOGDEB("Invalid wav file\n");
        return false;
    }
    
    header0 = header[40];
    header1 = header[41];
    header2 = header[42];
    header3 = header[43];

    m_subChunk2Size = header0 | (header1 << 8) | (header2 << 16) |
        (header3 << 24);

    m_dataoffs = ftello(m_fp);
    return true;
}

bool WavReader::readData()
{
    //LOGDEB("WavReader::readData: m_dataoffs " << m_dataoffs << endl);
    if (m_fp == 0) {
        LOGERR("WavReader::readData: not open\n");
        return false;
    }
    
    if (m_data)
        free(m_data);
    m_data = (unsigned char *)malloc(m_subChunk2Size);
    if (m_data == 0) {
        LOGERR("Malloc " << m_subChunk2Size / (1024*1024) << " Mb failed\n");
        return false;
    }
    fseeko(m_fp, m_dataoffs, SEEK_SET);

    ssize_t count = fread((void*)m_data, 1, m_subChunk2Size, m_fp);
    
    if (count != m_subChunk2Size) {
        LOGDEB("Unable to read wav file asked " << m_subChunk2Size << " got " <<
               count << endl);
        return false;
    }
    swapSamples(m_data, bytesPerSample(), sampleCount());

    return true;
}
