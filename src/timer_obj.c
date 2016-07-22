
#include "timer_obj.h"

#include <stdlib.h>

timer_callback_t new_timer_callback(int type, void *data, callback_func callbck) {
	timer_callback_t ret = malloc(sizeof(_timer_callback));
	ret->m_type = type;
	ret->data = data;
	ret->m_callback_func = callbck;
	return ret;
}

void free_timer_callback(timer_callback_t t) {
	free(t);
}

int timer_callback(timer_callback_t t) {
	return (*t->m_callback_func)(t->data, t->m_type);
}

int timer_callback(timer_callback_t t);

timer_obj_t new_timer_obj(unsigned int anInterval, int type, void *data, callback_func callback) {
	timer_obj_t ret = malloc(sizeof(_timer_obj));
	ret->m_Interval = anInterval;
	ret->m_current_tick = 0;
	ret->mTimer_callback = new_timer_callback(type, data, callback);
	if (anInterval <= KTimerIntervalUnit) {
		ret->m_cycl_tick = 1;
	} else {
		ret->m_cycl_tick = anInterval / KTimerIntervalUnit;
	}
	return ret;
}

int timer_obj_tick(timer_obj_t t) {
	if (t->m_current_tick > 0) {
		t->m_current_tick--;
	}
	if (t->m_current_tick == 0) {
		timer_callback(t->mTimer_callback);
		return 1;
	} else {
		return 0;
	}
}

void free_timer_obj(timer_obj_t t) {
	if (t->mTimer_callback != NULL) {
		free_timer_callback(t->mTimer_callback);
		t->mTimer_callback = NULL;
	}
	free(t);
}
