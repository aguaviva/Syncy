#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include "Log.h"

#define READ_END 0
#define WRITE_END 1

bool execute(const char *pWorkingDir, const char *filename, const char **params, char *out)
{
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1 || pipe(stdin_pipe) == -1) 
    {
        sprintf(out, "Can't create pipe\n");
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        sprintf(out, "Can't fork\n");
        Log("Can't fork");
        return false;
    }

    if (pid == 0) 
    {
        // Child process
        close(stdin_pipe[WRITE_END]);
        close(stdout_pipe[READ_END]);
        close(stderr_pipe[READ_END]);

        dup2(stdin_pipe[READ_END], STDIN_FILENO);
        dup2(stdout_pipe[WRITE_END], STDOUT_FILENO);
        dup2(stderr_pipe[WRITE_END], STDERR_FILENO);

        close(stdin_pipe[READ_END]);
        close(stdout_pipe[WRITE_END]);
        close(stderr_pipe[WRITE_END]);

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

        char data[]="ls\nexit\n";
        write(stdin_pipe[WRITE_END], out, sizeof(data));

        ssize_t count=0;
        out+=sprintf(out, "stdout:\n");

        // Read stdout
        while ((count = read(stdout_pipe[READ_END], out, 1)) > 0) {
            out += count;
            *out = '\0';
        }

        out+=sprintf(out, "\nstderr:\n");

        // Read stderr
        while ((count = read(stderr_pipe[READ_END], out, 1)) > 0) {
            out += count;
            *out = '\0';
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
            out+=sprintf(out, "waitpid() failed %i\n", respid);
            switch(errno)
            {
                case ECHILD: out+=sprintf(out, "No child process\n"); break;
                case EINVAL: out+=sprintf(out, "Invalid optons\n"); break;
                case EINTR: out+=sprintf(out, "Interrupted by signal\n"); break;
                default: out+=sprintf(out, "Error unknown\n"); break;
            }
        }
        else
        {
            if (WIFEXITED(status)) 
            {                
                out+=sprintf(out, "\nexit status %d\n", WEXITSTATUS(status));

                if (WEXITSTATUS(status)==0)
                {
                    Log("Exec : %s, OK", filename);
                    return true;
                }               

                Log("Exec : %s, ok: returned status %i", filename, WEXITSTATUS(status));
                return false;
            } 
            else 
            {
                out+=sprintf(out, "Child did not exit normally\n");
                Log("Exec : %s, err: abnormal termination %i", filename, status);
                return false;
            }
        }
    }    
    return false;
}