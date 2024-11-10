#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>

static bool started = false;
static char path_filename[1024];
static pthread_mutex_t cs_log_mutex =  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

bool LogInit(const char *filename)
{
    started = true;
    strcpy(path_filename, filename);
    return true;
}

bool Log(const char *fmt, ...)
{
    if (started == false)
        return false;

    pthread_mutex_lock( &cs_log_mutex );

    char buffer[1024];
    va_list arg;
    va_start(arg, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, arg);
    va_end(arg);	

	FILE *f = fopen(path_filename, "a");
    bool res = (f != NULL);
	if (res)
	{
        char buff[25];
        time_t now = time(NULL);
        strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));        

        fprintf(f, "%s : %s\n", buff, buffer);
        
        fclose(f);
    }

    pthread_mutex_unlock( &cs_log_mutex );

    return res;
}

void LogTerm()
{
    started = false;   
}
