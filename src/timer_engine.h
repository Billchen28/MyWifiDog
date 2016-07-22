#ifndef __TIMER_ENGINE__
#define __TIMER_ENGINE__

#ifndef __TIMER_DEBUG__
#define __TIMER_DEBUG__
#endif

#include "jqueue.h"

typedef struct timer_obj_st *timer_obj_t;

typedef struct timer_engine_st {
	jqueue_t mTimerTaskQueue;
	int m_timer_engine_stated;
}_timer_engine, *timer_engine_t;

timer_engine_t get_timer_engine();

void appendTimerTask(timer_obj_t t);

void cancelTimerTask(timer_obj_t t);

void release_timer_engine();

#endif

