#ifndef _CHRONO_H_
#define _CHRONO_H_

/** Easy interface to measuring time intervals */
class Chrono {
public:
    /** Initialize, setting the origin time */
    Chrono();

    /** Re-store current time and return mS since init or last call */
    long restart();
    /** Re-store current time and return uS since init or last call */
    long urestart();

    /** Snapshot current time to static storage */
    static void refnow();

    /** Return interval value in various units.
     *
     * Frozen means give time since the last refnow call (this is to
     * allow for using one actual system call to get values from many
     * chrono objects, like when examining timeouts in a queue 
     */
    long millis(bool frozen = false);
    long micros(bool frozen = false);
    float secs(bool frozen = false);

    /** Return current orig */
    long long amicros() const;

    struct TimeSpec {
        time_t tv_sec; /* Time in seconds */
        long   tv_nsec; /* And nanoseconds (< 10E9) */
    };

private:
    TimeSpec m_orig;
    static TimeSpec o_now;
};

#endif /* _CHRONO_H_ */
