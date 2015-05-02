#ifndef TEST_CHRONO
/* Copyright (C) 2014 J.F.Dockes
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

// Measure and display time intervals.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <iostream>

#include "chrono.h"

using namespace std;

////////////////////

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 1
#endif

#define MILLIS(TS1, TS2)                                \
    ((long long)((TS2).tv_sec - (TS1).tv_sec) * 1000LL +  \
     ((TS2).tv_nsec - (TS1).tv_nsec) / 1000000)

#define MICROS(TS1, TS2)                                        \
    ((long long)((TS2).tv_sec - (TS1).tv_sec) * 1000000LL +       \
     ((TS2).tv_nsec - (TS1).tv_nsec) / 1000)

#define SECONDS(TS1, TS2)                             \
    (float((TS2).tv_sec - (TS1).tv_sec) +             \
     float((TS2).tv_nsec - (TS1).tv_nsec) * 1e-9)

// We use gettimeofday instead of clock_gettime for now and get only
// uS resolution, because clock_gettime is more configuration trouble
// than it's worth
static void gettime(int, Chrono::TimeSpec *ts)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
}
///// End system interface (used to be much more complicated in the 199xs...)

Chrono::TimeSpec Chrono::o_now;

void Chrono::refnow()
{
    gettime(CLOCK_REALTIME, &o_now);
}

long long Chrono::amicros() const
{
    TimeSpec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    return MICROS(ts, m_orig);
}

Chrono::Chrono()
{
    restart();
}

// Reset and return value before rest in milliseconds
long Chrono::restart()
{
    TimeSpec now;
    gettime(CLOCK_REALTIME, &now);
    long ret = MILLIS(m_orig, now);
    m_orig = now;
    return ret;
}
long Chrono::urestart()
{
    TimeSpec now;
    gettime(CLOCK_REALTIME, &now);
    long ret = MICROS(m_orig, now);
    m_orig = now;
    return ret;
}

// Get current timer value, milliseconds
long Chrono::millis(bool frozen)
{
    if (frozen) {
        return MILLIS(m_orig, o_now);
    } else {
        TimeSpec now;
        gettime(CLOCK_REALTIME, &now);
        return MILLIS(m_orig, now);
    }
}

//
long Chrono::micros(bool frozen)
{
    if (frozen) {
        return MICROS(m_orig, o_now);
    } else {
        TimeSpec now;
        gettime(CLOCK_REALTIME, &now);
        return MICROS(m_orig, now);
    }
}

float Chrono::secs(bool frozen)
{
    if (frozen) {
        return SECONDS(m_orig, o_now);
    } else {
        TimeSpec now;
        gettime(CLOCK_REALTIME, &now);
        return SECONDS(m_orig, now);
    }
}

#else

///////////////////// test driver


#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include <iostream>

#include "chrono.h"

using namespace std;

static char *thisprog;
static void
Usage(void)
{
    fprintf(stderr, "Usage : %s \n", thisprog);
    exit(1);
}

Chrono achrono;
Chrono rchrono;

void
showsecs(long msecs)
{
    fprintf(stderr, "%3.5f S", ((float)msecs) / 1000.0);
}

void
sigint(int sig)
{
    signal(SIGINT, sigint);
    signal(SIGQUIT, sigint);

    fprintf(stderr, "Absolute interval: ");
    showsecs(achrono.millis());
    fprintf(stderr, ". Relative interval: ");
    showsecs(rchrono.restart());
    cerr <<  " Abs micros: " << rchrono.amicros() <<
        " Relabs micros: " << rchrono.amicros() - 1430477861905884LL
         << endl;
    fprintf(stderr, ".\n");
    if (sig == SIGQUIT) {
        exit(0);
    }
}

int main(int argc, char **argv)
{

    thisprog = argv[0];
    argc--;
    argv++;

    if (argc != 0) {
        Usage();
    }

    for (int i = 0; i < 50000000; i++);
    fprintf(stderr, "Start secs: %.2f\n", achrono.secs());


    fprintf(stderr, "Type ^C for intermediate result, ^\\ to stop\n");
    signal(SIGINT, sigint);
    signal(SIGQUIT, sigint);
    achrono.restart();
    rchrono.restart();
    while (1) {
        pause();
    }
}

#endif /*TEST_CHRONO*/
