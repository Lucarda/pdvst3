#include <stdlib.h>
#include <stdio.h>
#if _WIN32
#include <windows.h>
#else
#include <time.h>
#include <semaphore.h>
#endif



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
    //http://www.csc.villanova.edu/~mdamian/threads/posixsem.html

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;          // Seconds
    ts.tv_nsec += (ms % 1000) * 1e6; // Nanoseconds
  
    int res = sem_timedwait(mutex, &ts);

    if (res == 0)  return 1;
    else return 0;
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
    //sem_post(mutex);
    
    
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
    //sem_post(mutex);
    
    int value;
    sem_getvalue(mutex, &value);
    while (value > 0) {
        sem_wait(mutex);  // Decrement until count is 0
        sem_getvalue(mutex, &value);
    }
    
}
#endif

