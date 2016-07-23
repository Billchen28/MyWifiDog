
#ifndef __TIMER_OBJ__
#define __TIMER_OBJ__

#include <sys/time.h>

#define KTimerIntervalUnit 500

typedef int (*callback_func)(void *data,int);

typedef struct timer_callback_st {
	int m_type;
	void *data;
	callback_func m_callback_func;
}_timer_callback, *timer_callback_t;

timer_callback_t new_timer_callback();

void free_timer_callback(timer_callback_t t);

int timer_callback(timer_callback_t t);

typedef struct timer_obj_st {
	unsigned int m_Interval;
	unsigned int m_cycl_tick;
	unsigned int m_current_tick;
	timer_callback_t mTimer_callback;
} _timer_obj, *timer_obj_t;

timer_obj_t new_timer_obj(unsigned int anInterval, int type, void *data, callback_func callback);

int timer_obj_tick(timer_obj_t t);

void free_timer_obj(timer_obj_t t);

#endif
