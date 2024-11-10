#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <assert.h>

static char term_data[10*1024];
static char *term_end;
static int term_length;
static pthread_mutex_t cs_log_mutex =  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

void Term_clear()
{
    term_end=term_data;
    term_data[0] = 0;
    term_length = sizeof(term_data);
}

void Term_add(const char *fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);

    pthread_mutex_lock( &cs_log_mutex );

    //make it MT friendly..

    // we put data after the 0
    vsnprintf(term_end, term_length, fmt, arg);
    va_end(arg);
/*
    char first = *(term_end+1);
    for(int i=1;i<len;i++)
    {
        term_end[i] = term_end[i+1];
    }

    //overwrite 0 so it shows up at once
    term_end[0] = first;
*/  
    int len = strlen(term_end);
    assert(len>=0);
    term_end += len;
    term_length -= len;


    pthread_mutex_unlock( &cs_log_mutex );

}

char *Term_get_data()
{
    return term_data;
}

