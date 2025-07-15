/*
 * This file is part of pdvst3.
 *
 * Copyright (C) 2025 Lucas Cordiviola
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#if _WIN32
#include <windows.h>
#else
#include <time.h>
#include <semaphore.h>
#include <unistd.h>
#endif
extern "C"
{
    #include "pdvstTransfer.h"
}


#if _WIN32

int xxWaitForSingleObject(HANDLE mutex, int ms)
{
    int ret;
    ret = WaitForSingleObject(mutex, ms);

    if (ret == WAIT_TIMEOUT)
        return 0;
    else if (ret == WAIT_OBJECT_0)
        return 1;
    else
        return(ret);
}

#else

int xxWaitForSingleObject(sem_t *mutex, int ms)
{
    if (ms == -1) ms = 30000;
    float elapsed_time = 0;
    int wait_time = 10; // Wait time between attempts in microseconds
    int ret= -1;
    while (1)
    {
        if (sem_trywait(mutex) == 0)
            return 1;
        if (elapsed_time >= ms) {
            // Timeout has been reached
            return 0;
        }
        usleep(wait_time);
        elapsed_time += (wait_time / 1000.);
    }
}
#endif

#if _WIN32

int xxReleaseMutex(HANDLE mutex)
{
    ReleaseMutex(mutex);
    return 0;
}

#else

int xxReleaseMutex(sem_t *mutex)
{
    sem_post(mutex);
    return 0;
}
#endif

#if _WIN32

void xxSetEvent(HANDLE mutex)
{
    SetEvent(mutex);
}

#else

void xxSetEvent(sem_t *mutex)
{
    int value;
    sem_getvalue(mutex, &value);
    if (value == 0) {
        sem_post(mutex);  // Increment to 1 (signaled)
    }
}
#endif

#if _WIN32

void xxResetEvent(HANDLE mutex)
{
    ResetEvent(mutex);
}

#else

void xxResetEvent(sem_t *mutex)
{
    int value;
    sem_getvalue(mutex, &value);
    while (value > 0) {
        sem_wait(mutex);  // Decrement until count is 0
        sem_getvalue(mutex, &value);
    }
}
#endif

