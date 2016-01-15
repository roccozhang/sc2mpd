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

#include <strings.h>

#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <iostream>
#include <vector>

#include "audioreader.h"
#include "wavreader.h"
#include "fiforeader.h"
#include "log.h"
#include "stringtotokens.h"

using namespace std;

struct AParms {
    int freq;
    int bits;
    int chans;
    bool needswap; 
    AParms() : freq(0), bits(0), chans(0), needswap(0) {}
};

// freq:bits:chans:needswap 44100:16:2:0/1
static bool parseAudioParams(const string& p, AParms *out)
{
    vector<string> v;

    stringToTokens(p, v, ":");
    if (v.size() != 4) {
        LOGERR("parseAudioParams: bad params " << p << " v size " << v.size()
               << endl);
        return false;
    }

    out->freq = atoi(v[0].c_str());
    out->bits = atoi(v[1].c_str());
    out->chans = atoi(v[2].c_str());
    int val = atoi(v[3].c_str());
    out->needswap = val ? true: false;

    //LOGDEB("freq " << out->freq << " bits " << out->bits << " chans "<<
    //out->chans << " val " << val << endl);

    if (out->freq == 0 || out->bits == 0 || out->chans == 0 ||
        (val != 0 && val != 1)) {
        LOGERR("parseAudioParams: bad params " << p << endl);
        return false;
    }
    return true;
}

AudioReader *openAudio(const string& fn, const string& audioparams,
                       bool srcblock)
{
    string::size_type dot = fn.find_last_of(".");
    string ext;
    if (dot != string::npos) {
        ext = fn.substr(dot);
    }
    
    if (!strcasecmp(ext.c_str(), ".wav")) {
        return new WavReader(fn);
    } else if (ext.empty() || !strcasecmp(ext.c_str(), ".fifo")) {
        AParms parms;
        if (!parseAudioParams(audioparams, &parms)) {
            return 0;
        }
        LOGDEB("Audioparams: freq " << parms.freq << " bits " << parms.bits <<
               " chans " << parms.chans << endl);
        struct stat st;
        if (stat(fn.c_str(), &st)) {
            LOGERR(" stat() errno: " << errno << " for " << fn << endl);
            return 0;
        }
        if ((st.st_mode & S_IFMT) == S_IFIFO) {
            return new FifoReader(fn, parms.freq, parms.bits, parms.chans,
                                  parms.needswap, srcblock);
        } else {
            LOGERR("Not a fifo: " << fn << endl);
            return 0;
        }
    } else {
        LOGERR("Unprocessed file extension: " << fn.substr(dot) << endl);
        return 0;
    }
    return 0;
}


// Convert sample endianness
void swapSamples(unsigned char *data, int bytesPerSamp, int scount)
{
    //LOGDEB("swapSamples: bps " << bytesPerSamp << " count " << scount << endl);
    if (bytesPerSamp == 2) {
        swab(data, data, scount * 2);
    } else {
        unsigned char sample[4];
        // Byte index in data buffer
        unsigned int pindex = 0;
    
        while (scount-- > 0) {
            int sindex = 0;
            while (sindex < bytesPerSamp) {
                sample[sindex++] = data[pindex++];
            }
            
            sindex = 0;
            while (sindex < bytesPerSamp) {
                data[--pindex] = sample[sindex++];
            }
            
            pindex += bytesPerSamp;
        }
    }
}
