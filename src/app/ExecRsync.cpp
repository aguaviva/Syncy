#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

#define READ_END 0
#define WRITE_END 1

char destination[] = "username@myserver.org:/tmp";

void do_rsync(const char *pWorkingDir, const char *filename, char *out)
{
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1 || pipe(stdin_pipe) == -1) 
    {
        sprintf(out, "Can't crate pipe\n");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        sprintf(out, "Can't fork\n");
        return;
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

#ifdef ANDROID    
        chdir(pWorkingDir); 
                
        execl("./rsync", 
        "-avz", 
        "-e", "./dbclient -p 22222 -i ./dropbear_rsa_host_key -y -y", 
        "--progress", 
        filename, 
        destination, 
        (char*)NULL);
#else        
        execl("/usr/bin/df", "-h", (char*)NULL);
        //execl("/usr/bin/rsync", "-av", "--stats", "--progress", filename, "/tmp", (char*)NULL);
        
        //execv("/usr/bin/rsync", "-h", (char*)NULL);
        //execv("/usr/bin/rsync", (char * const *)argv);

#endif        
        /* if execl() was successful, this won't be reached */
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
                out+=sprintf(out, "Child exited with status %d\n", WEXITSTATUS(status));
            } 
            else 
            {
                out+=sprintf(out, "Child did not exit normally\n");
            }
        }
    }    
}