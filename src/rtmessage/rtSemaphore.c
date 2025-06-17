/*
 * If not stated otherwise in this file or this component's Licenses.txt file
 * the following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#include "rtSemaphore.h"
#include "rtLog.h"
#include "rtError.h"
#include "rtMemory.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#ifdef __APPLE__
#include <sys/time.h> // For gettimeofday on macOS
#else
#include <time.h> // For clock_gettime on Linux
#endif

#define ERROR_CHECK(CMD) \
{ \
  int err; \
  if((err=CMD) != 0) \
  { \
    rtLog_Error("Error %d:%s running command " #CMD, err, strerror(err)); \
    rc = RT_ERROR; \
  } \
}

struct _rtSemaphore
{
  pthread_mutex_t m;
  pthread_cond_t c;
  int v;
};

rtError rtSemaphore_Create(rtSemaphore* sem)
{
  int rc = 0;
  pthread_mutexattr_t mattrib;
  pthread_condattr_t cattrib;
  *sem = rt_try_malloc(sizeof(struct _rtSemaphore));
  if(!*sem)
    return rtErrorFromErrno(ENOMEM);
  (*sem)->v = 0;
  ERROR_CHECK(pthread_mutexattr_init(&mattrib));
  ERROR_CHECK(pthread_mutexattr_settype(&mattrib, PTHREAD_MUTEX_ERRORCHECK));
  ERROR_CHECK(pthread_mutex_init(&(*sem)->m, &mattrib));  
  ERROR_CHECK(pthread_mutexattr_destroy(&mattrib));
  ERROR_CHECK(pthread_condattr_init(&cattrib));
#ifndef __APPLE__
  ERROR_CHECK(pthread_condattr_setclock(&cattrib, CLOCK_MONOTONIC));
#endif
  ERROR_CHECK(pthread_cond_init(&(*sem)->c, &cattrib));
  ERROR_CHECK(pthread_condattr_destroy(&cattrib));
  return rc;
}

rtError rtSemaphore_Destroy(rtSemaphore sem)
{
  int rc = RT_OK;
  ERROR_CHECK(pthread_cond_signal(&sem->c));
  ERROR_CHECK(pthread_mutex_destroy(&sem->m));
  ERROR_CHECK(pthread_cond_destroy(&sem->c));
  free(sem);
  return rc;
}

rtError rtSemaphore_Post(rtSemaphore sem)
{
  int rc = RT_OK;
  ERROR_CHECK(pthread_mutex_lock(&sem->m));
  sem->v++;
  ERROR_CHECK(pthread_mutex_unlock(&sem->m));
  ERROR_CHECK(pthread_cond_signal(&sem->c));
  return rc;
}

rtError rtSemaphore_GetValue(rtSemaphore sem, int* val)
{
  int rc = RT_OK;
  *val = sem->v;
  return rc;
}

rtError rtSemaphore_Wait(rtSemaphore sem)
{
    return rtSemaphore_TimedWait(sem, NULL);
}

rtError rtSemaphore_TimedWait(rtSemaphore sem, struct timespec* t)
{
    int rc = RT_OK;
    int err;
    struct timespec abs_timeout;

    ERROR_CHECK(pthread_mutex_lock(&sem->m));
    if(sem->v > 0)
    {
        sem->v--;
        ERROR_CHECK(pthread_mutex_unlock(&sem->m));
        return RT_OK;
    }

    if(t)
    {
        // Calculate absolute timeout
#ifdef __APPLE__
      // Use gettimeofday for macOS (CLOCK_REALTIME equivalent)
        struct timeval tv;
        if(gettimeofday(&tv, NULL) != 0)
        {
            rtLog_Error("Error getting time: %s", strerror(errno));
            ERROR_CHECK(pthread_mutex_unlock(&sem->m));
            return RT_ERROR;
        }
        abs_timeout.tv_sec = tv.tv_sec + t->tv_sec;
        abs_timeout.tv_nsec = tv.tv_usec * 1000 + t->tv_nsec;
        if(abs_timeout.tv_nsec >= 1000000000)
        {
            abs_timeout.tv_sec++;
            abs_timeout.tv_nsec -= 1000000000;
        }
#else
      // Use CLOCK_MONOTONIC for Linux
        struct timespec now;
        if(clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        {
            rtLog_Error("Error getting time: %s", strerror(errno));
            ERROR_CHECK(pthread_mutex_unlock(&sem->m));
            return RT_ERROR;
        }
        abs_timeout.tv_sec = now.tv_sec + t->tv_sec;
        abs_timeout.tv_nsec = now.tv_nsec + t->tv_nsec;
        if(abs_timeout.tv_nsec >= 1000000000)
        {
            abs_timeout.tv_sec++;
            abs_timeout.tv_nsec -= 1000000000;
        }
#endif
        err = pthread_cond_timedwait(&sem->c, &sem->m, &abs_timeout);
    }
    else
    {
        err = pthread_cond_wait(&sem->c, &sem->m);
    }

    if(err == 0)
    {
        if(sem->v > 0)
            sem->v--;
    }
    else
    {
        if(t && err == ETIMEDOUT)
        {
            rc = RT_ERROR_TIMEOUT;
        }
        else if(err != 0)
        {
            rtLog_Error("Error %d:%s running command pthread_cond_timedwait", err, strerror(err));
            rc = RT_ERROR;
        }
    }
    ERROR_CHECK(pthread_mutex_unlock(&sem->m));
    return rc;
}
