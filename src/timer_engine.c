#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "timer_engine.h"
#include "timer_obj.h"

static timer_engine_t g_timer_engine = NULL;

void do_tick(timer_engine_t t_e) {
	if (t_e == NULL || g_timer_engine->mTimerTaskQueue == NULL) {
		return;
	}
	int task_remove = 0;
	_jqueue_node_t prev = g_timer_engine->mTimerTaskQueue->front;
	while (prev != NULL) {
		if (prev->data != NULL) {
			timer_obj_t to = (timer_obj_t)prev->data;
			if (timer_obj_tick(to)) {
				jqueue_remove(g_timer_engine->mTimerTaskQueue, prev->data);
				task_remove = 1;
			}
		}
		prev = prev->prev;
	}
	if (task_remove && jqueue_size(g_timer_engine->mTimerTaskQueue) <= 0) {
		stop_engine();
	}
}

void tick(int arg) {
	if (g_timer_engine != NULL) {
		do_tick(g_timer_engine);
	}
}


void start_engine() {
	if (g_timer_engine == NULL || g_timer_engine->m_timer_engine_stated) {
		return;
	}
	signal(SIGALRM, tick);
        struct itimerval timeInterval;

        unsigned int secondUnit = 0;
        unsigned int msUnit = 0;
        if (KTimerIntervalUnit > 1000)
        {
                secondUnit = KTimerIntervalUnit / 1000;
                msUnit = KTimerIntervalUnit % 1000;
        } else {
                msUnit = KTimerIntervalUnit;
        }
        timeInterval.it_value.tv_sec = secondUnit;
        timeInterval.it_value.tv_usec = msUnit * 1000;
        timeInterval.it_interval.tv_sec  = secondUnit;
        timeInterval.it_interval.tv_usec = msUnit * 1000;
        int ret = 0;
        ret = setitimer(ITIMER_REAL , &timeInterval, NULL);
        if ( ret != 0)
        {
		g_timer_engine->m_timer_engine_stated = 0;
#ifdef __TIMER_DEBUG__
                printf("start gobal timer fail\n");
#endif
        }
        else
        {
		g_timer_engine->m_timer_engine_stated = 1;
#ifdef __TIMER_DEBUG__
                printf("start gobal timer ok\n");
#endif
        }
}

void stop_engine() {
	if (g_timer_engine == NULL || !g_timer_engine->m_timer_engine_stated) {
		return;
	}
	signal(SIGALRM, tick);
	struct itimerval timeInterval;
	timeInterval.it_value.tv_sec = 0;
	timeInterval.it_value.tv_usec = 0;
	timeInterval.it_interval.tv_sec  = 0;
	timeInterval.it_interval.tv_usec = 0;
	int ret = 0;
	ret = setitimer(ITIMER_REAL , &timeInterval, NULL);
	if ( ret != 0)
	{
#ifdef __TIMER_DEBUG__
		printf("stop gobal timer fail\n");
#endif
	}
	else
	{
		g_timer_engine->m_timer_engine_stated = 0;
#ifdef __TIMER_DEBUG__
		printf("stop gobal timer ok\n");
#endif
	}
}

void appendTimerTask(timer_obj_t t) {
	if (g_timer_engine == NULL) {
		get_timer_engine();
	}
	if (g_timer_engine->mTimerTaskQueue == NULL) {
		g_timer_engine->mTimerTaskQueue = jqueue_new();
	}
	jqueue_push(g_timer_engine->mTimerTaskQueue, t, 0);
	if (g_timer_engine->m_timer_engine_stated == 0 &&
		jqueue_size(g_timer_engine->mTimerTaskQueue) > 0) {
		start_engine();
	}
}

void cancelTimerTask(timer_obj_t t) {
	if (g_timer_engine == NULL || jqueue_size(g_timer_engine->mTimerTaskQueue) <= 0) {
		return;
	}
	jqueue_remove(g_timer_engine->mTimerTaskQueue, t);
	if (jqueue_size(g_timer_engine->mTimerTaskQueue) <= 0) {
		stop_engine();
	}
}

timer_engine_t get_timer_engine() {
	if (g_timer_engine == NULL) {
		timer_engine_t ret = malloc(sizeof(_timer_engine));
		ret->m_timer_engine_stated = 0;
		ret->mTimerTaskQueue = NULL;
		g_timer_engine = ret;
	}
	return g_timer_engine;
}

void release_timer_engine() {
	if (g_timer_engine == NULL) {
		return;
	}
	stop_engine();
	free(g_timer_engine);
	g_timer_engine = NULL;
}
