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

#ifndef _AUDIOREADER_H_INCLUDED_
#define _AUDIOREADER_H_INCLUDED_

#include <string>

// This just somewhat encapsulates the code in the original
// WavSender.cpp The file is still read and converted in memory at
// once, and held in a memory array
class AudioReader {
public:
    AudioReader() {
        m_numChannels = m_sampleRate = 0;
    }
    virtual ~AudioReader() {}

    virtual bool open() = 0;

    virtual bool isblocking() {return false;}
    
    // Get pointer to data buffer of specified offset and size.
    virtual const unsigned char *data(size_t cnt) = 0;

    // Return, compute if necessary, misc audio stream parameters:
    virtual unsigned int numChannels() {
        return m_numChannels;
    }
    virtual unsigned int sampleRate() {
        return m_sampleRate;
    }
    virtual unsigned int bitsPerSample() {
        return m_bitsPerSample;
    }
    virtual unsigned int byteRate() {
        return m_numChannels * m_sampleRate * m_bitsPerSample / 8;
    }
    virtual unsigned int bytesPerSample() {
        return m_bitsPerSample / 8;
    }
    virtual unsigned int bytesPerFrame() {
        return bytesPerSample() * m_numChannels;
    }
    virtual unsigned int sampleCount() {
        return 0;
    }

protected:
    void setAudioParams(int sampleRate, int bitsPerSample, int chans) {
        m_numChannels = chans;
        m_sampleRate = sampleRate;
        m_bitsPerSample = bitsPerSample;
    }
    unsigned int m_numChannels;
    unsigned int m_sampleRate; // sampling frequency, Hz
    unsigned int m_bitsPerSample;
};

#endif /* _AUDIOREADER_H_INCLUDED_ */
