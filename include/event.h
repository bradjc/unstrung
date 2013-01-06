#ifndef _UNSTRUNG_EVENT_H
#define _UNSTRUNG_EVENT_H

extern "C" {
#include <sys/time.h>
#include "oswlibs.h"
#include "rpl.h"
}

#include <map>
#include <algorithm>
#include <vector>

#include "prefix.h"

class network_interface;
class dag_network;
class rpl_node;
class rpl_eventless;
class rpl_event;

class rpl_event {
public:
    friend class rpl_eventless;

    enum event_types {
        rpl_send_dio = 1,
        rpl_send_dao = 2,
	rpl_event_max
    };

    rpl_event() { };

    rpl_event(struct timeval &relative, 
              unsigned int sec, unsigned int msec,
              event_types t, const char *reason, rpl_debug *deb) {
        init_event(relative, sec, msec, t, reason, deb);
    };

    static bool                    faked_time;
    static struct timeval          fake_time;
    static void set_fake_time(struct timeval n) {
	faked_time = true;
	fake_time  = n;
    };

    rpl_event(unsigned int sec, unsigned int msec,
              event_types t, const char *reason, rpl_debug *deb) {
        struct timeval now;
	gettimeofday(&now, NULL);
        init_event(now, sec, msec, t, reason, deb);
    };

    enum event_types event_type;
    const char *event_name();

    /* invoke this event */
    bool doit(void);
    bool send_dio_all(void);
    bool send_dao_all(void);

    bool passed(struct timeval &now) {
        //fprintf(stderr, "passed([%u,%u] < [%u,%u])\n",
        //        alarm_time.tv_sec, alarm_time.tv_usec,
        //        now.tv_sec, now.tv_usec);

        if(alarm_time.tv_sec  <  now.tv_sec)  return true;
        if(alarm_time.tv_sec  == now.tv_sec && 
           alarm_time.tv_usec <= now.tv_usec) return true;
        return false;
    };
    
    /* calculate against this event */
    struct timeval      occurs_in(struct timeval &now);
    struct timeval      occurs_in(void);     
    const int           miliseconds_util(void);
    const int           miliseconds_util(struct timeval &now);

    void requeue(void);
    void requeue(struct timeval &now);

    void printevent(FILE *out);

    /* the associated iface for this event */
    network_interface  *interface;

    struct timeval      alarm_time;
    const char *getReason() {
        return mReason;
    };

    void set_alarm(struct timeval &relative,
                   unsigned int sec, unsigned int msec)
    {
        struct timeval rel = relative;
	if(faked_time) {
	    rel = fake_time;
	} 

        last_time  = rel;
        repeat_sec = sec;
        repeat_msec= msec;
        alarm_time.tv_usec = rel.tv_usec + msec*1000;
        alarm_time.tv_sec  = rel.tv_sec  + sec;
        while(alarm_time.tv_usec > 1000000) {
            alarm_time.tv_usec -= 1000000;
            alarm_time.tv_sec++;
        }
	//fprintf(stderr, "%u: alarm for %u/%u + %u/%u\n", event_number, rel.tv_sec, rel.tv_usec, sec, msec);
    };

    void reset_alarm(unsigned int sec, unsigned int msec) {
        struct timeval now;
        gettimeofday(&now, NULL);
	set_alarm(now, sec, msec);
    };

    dag_network        *mDag;

    /* set to true to remove variable dates from debug output 
     * used by regression testing routines.
     */
    void init_event(struct timeval &relative,
                    unsigned int sec, unsigned int msec,
                    event_types t, const char *reason, rpl_debug *deb) {
        event_type = t;
        mReason[0]='\0';
        strncat(mReason, reason, sizeof(mReason)-1);
        debug = deb;
	event_number = event_counter++;
        set_alarm(relative, sec, msec);
    };

    bool operator<(const class rpl_event &b) const {
        int match = b.alarm_time.tv_sec - alarm_time.tv_sec;
        printf("compare1 a:%u b:%u match:%d\n", alarm_time.tv_sec, b.alarm_time.tv_sec, match);
        if(match > 0) return true;
        if(match < 0) return false;

        match = b.alarm_time.tv_usec - alarm_time.tv_usec;
        printf("compare2 a:%u b:%u match:%d\n", alarm_time.tv_usec, b.alarm_time.tv_usec, match);
        if(match > 0) return false;
        return true;
    }

private:
    unsigned int        event_number;
    static unsigned int event_counter;
    unsigned int        repeat_sec;
    unsigned int        repeat_msec;
    struct timeval      last_time;
    char mReason[32];
    rpl_debug *debug;
};

bool rpl_eventless(const class rpl_event *a, const class rpl_event *b);

class rpl_event_queue {
public:
    std::vector<class rpl_event *> queue;


    rpl_event_queue() {
	make_heap(queue.begin(), queue.end(), rpl_eventless);
    };

    class rpl_event *peek_event(void) {
	return queue.front();
    };

    void eat_event(void) {
	pop_heap(queue.begin(), queue.end(), rpl_eventless); queue.pop_back();
    };

    class rpl_event *next_event(void) {
	rpl_event *n = peek_event();
	eat_event();
	return n;
    };

    void add_event(class rpl_event *n) {
	queue.push_back(n);
	push_heap(queue.begin(), queue.end(), rpl_eventless);
    };

    int size(void) {
	return queue.size();
    };

    /* dump this event for humans */
    void printevents(FILE *out, const char *prefix) {
	int i = 0;
	std::vector<class rpl_event *>::iterator one = queue.begin();
	fprintf(out, "event list (%u events)\n", queue.size());
	while(one != queue.end()) {
	    fprintf(out, "%s%d: ", prefix, i);
	    (*one)->printevent(out);
	    fprintf(out, "\n");
	    i++; one++;
	}
    };

};

#endif /* _UNSTRUNG_EVENT_H */

/*
 * Local Variables:
 * c-basic-offset:4
 * c-style: whitesmith
 * End:
 */

