#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include "Log.h"
#include "Term.h"

#define READ_END 0
#define WRITE_END 1

static int stdin_pipe[2];
static int stdout_pipe[2];
static int stderr_pipe[2];

bool execute(const char *pWorkingDir, const char *filename, const char **params)
{

    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1 || pipe(stdin_pipe) == -1) 
    {
        Log("Error: Can't create pipe");
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        Log("Error: Can't fork");
        return false;
    }

    if (pid == 0) 
    {
        //setvbuf(STDOUT_FILENO, NULL, _IONBF, 0); 

        // Child process
        close(stdin_pipe[WRITE_END]);
        dup2(stdin_pipe[READ_END], STDIN_FILENO);

        close(stdout_pipe[READ_END]);
        dup2(stdout_pipe[WRITE_END], STDOUT_FILENO);

        close(stderr_pipe[READ_END]);
        dup2(stderr_pipe[WRITE_END], STDERR_FILENO);

        Log("Exec: %s", filename);

        chdir(pWorkingDir); 

        execv(filename, (char* const* )params);

        /* if execv() was successful, this won't be reached */
        _exit(127);
    }
    else
    {
        // Parent process
        close(stdin_pipe[READ_END]);
        close(stdout_pipe[WRITE_END]);
        close(stderr_pipe[WRITE_END]);

        //sleep(5);
        ssize_t count=0;
        char out;

        // Read stderr
        Term_add("\nstderr:\n=======\n");
        while ((count = read(stderr_pipe[READ_END], &out, 1)) > 0) {
            for(int i=0;i<count;i++)
                Term_add("%c", out);
        }

        // Read stdout
        Term_add("\nstdout:\n=======\n");
        while ((count = read(stdout_pipe[READ_END], &out, 1)) > 0) {
            for(int i=0;i<count;i++)
                Term_add("%c", out);
        }

        close(stdin_pipe[WRITE_END]);
        close(stdout_pipe[READ_END]);
        close(stderr_pipe[READ_END]);

        /* the parent process calls waitpid() on the child */
        int status;
        pid_t respid = waitpid(pid, &status, 0); 
        if ( respid == -1)
        {                        
            /* waitpid() failed */
            Log("waitpid() failed");
            switch(errno)
            {
                case ECHILD: Log("No child process\n"); break;
                case EINVAL: Log("Invalid optons\n"); break;
                case EINTR: Log("Interrupted by signal\n"); break;
                default: Log("Error unknown\n"); break;
            }
        }
        else
        {
            if (WIFEXITED(status)) 
            {                
                Term_add("\nexit status %d\n", WEXITSTATUS(status));

                if (WEXITSTATUS(status)==0)
                {
                    Log("Exec : %s, OK", filename);
                    return true;
                }               

                Log("Exec : %s OK, returned status %i", filename, WEXITSTATUS(status));
                return false;
            } 
            else 
            {
                Log("Exec : %s, err: abnormal termination %i", filename, status);
                return false;
            }
        }
    }    

    return false;
}

void pipein(char *str)
{
    write(stdin_pipe[WRITE_END], str, strlen(str));
}