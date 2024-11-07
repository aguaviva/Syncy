#include <string.h>
#include <unistd.h>
#include "Syncy.h"
#include <GLES3/gl3.h>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#ifdef ANDROID
#include <EGL/egl.h>
#include "../android_native_app_glue.h"
#include "backends/imgui_impl_android.h"
#else
#include <GLFW/glfw3.h>
#include "backends/imgui_impl_glfw.h"
#endif

#include "sys/inotify.h"  
#include <poll.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "hashmap.h"

struct FileEntry {
    char *name;
    time_t rawtime;
};

int FileEntry_compare(const void *a, const void *b, void *udata) {
    const FileEntry *ua = (const FileEntry *)a;
    const FileEntry *ub = (const FileEntry *)b;
    return strcmp(ua->name, ub->name);
}

uint64_t FileEntry_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const FileEntry *FileEntry = (const struct FileEntry *)item;
    return hashmap_sip(FileEntry->name, strlen(FileEntry->name), seed0, seed1);
}

const char *pInternalDataPath = NULL;
const char *pExternalDataPath = NULL;
char pExternalLibPath[1024];

bool play=false;

int fd;
int wd;


struct hashmap *map;


#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
char buffer[EVENT_BUF_LEN];

static pthread_mutex_t cs_mutex =  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

char exec_output[1024];


void do_rsync(const char *filename)
{

    pid_t pid;

    int link[2];
    if (pipe(link)==-1)
    {
        printf("failed to pipe()");
    }

    if ((pid = fork()) == 0) 
    {
        dup2 (link[1], STDOUT_FILENO);
        close(link[0]);
        close(link[1]);


        /* the child process */
        const char * const argv[] = 
        {
            "-av",
            "--stats", 
            "--progress",
            "--human-readable",
            filename,
            "/tmp/",
            NULL
        };

#ifdef ANDROID    
        chdir(pExternalLibPath); 
        char file[1024];
        sprintf(file, "%s/dbclient", pExternalLibPath);

        
        char key[1024];
        sprintf(key, "\"%s/dbclient -p 22222 -i %s/dropbear_rsa_host_key\"", pExternalLibPath, pExternalLibPath);
        
        sprintf(file, "%s/rsync", pExternalLibPath);
        execl(file, "-avz", "--stats", "--progress", filename, "/tmp", (char*)NULL);
        //execl(file,  "-avz", "-e",  key, filename, "raul@xchip.duckdns.org:/tmp", (char*)NULL);

        //sprintf(file, "%s/dbclient", pExternalLibPath);
        //execl(file,  "xchip.duckdns.org", "-p", "22222", (char*)NULL);

#else        
        execl("/usr/bin/df", "-h", (char*)NULL);
        //execl("/usr/bin/rsync", "-av", "--stats", "--progress", filename, "/tmp", (char*)NULL);
        
        //execv("/usr/bin/rsync", "-h", (char*)NULL);
        //execv("/usr/bin/rsync", (char * const *)argv);

#endif        
        /* if execl() was successful, this won't be reached */
        _exit(127);
    }

    if (pid > 0) 
    {
        close(link[1]);
        int ntotal = 0;
        int nbytes = 0;
        exec_output[ntotal]='\0';

        while(0 != (nbytes = read(link[0], &exec_output[ntotal], sizeof(1024)))) {
            ntotal += nbytes;
            exec_output[ntotal]='\0';
        }
        wait(NULL);

        /* the parent process calls waitpid() on the child */
        int status;
        pid_t respid = waitpid(pid, &status, 0); 
        if ( respid == -1)
        {                        
            /* waitpid() failed */
            ntotal+=sprintf(&exec_output[ntotal], "waitpid() failed %i\n", respid);
            switch(errno)
            {
                case ECHILD: ntotal+=sprintf(&exec_output[ntotal], "No child process\n"); break;
                case EINVAL: ntotal+=sprintf(&exec_output[ntotal], "Invalid optons\n"); break;
                case EINTR: ntotal+=sprintf(&exec_output[ntotal], "Interrupted by signal\n"); break;
                default: ntotal+=sprintf(&exec_output[ntotal], "Error unknown\n"); break;
            }
        }
        else
        {
            if (WIFEXITED(status) && !WEXITSTATUS(status)) 
            {
                strcpy(exec_output, "the program terminated normally and executed successfully\n");
                /* the program terminated normally and executed successfully */
            } 
            else if (WIFEXITED(status) && WEXITSTATUS(status)) 
            {
                if (WEXITSTATUS(status) == 127) {
                    /* execl() failed */
                    strcpy(exec_output, "execl() failed\n");
                } 
                else 
                {
                    /* the program terminated normally, but returned a non-zero status */
                    switch (WEXITSTATUS(status)) 
                    {
                    /* handle each particular return code that the program can return */
                    }
                    sprintf(exec_output, "program returned value status %i\n", status);
                }
            } 
            else 
            {
                /* the program didn't terminate normally */
                strcpy(exec_output, "the program didn't terminate normally");
            }
        } 
    } 
    else 
    {
        /* failed to fork() */
        strcpy(exec_output, "failed to fork()");
    }    
}

double GetAge(const FileEntry *item)
{
    time_t t;
    time(&t);
    return  difftime(t, item->rawtime );
}

bool GetOldestMod(const FileEntry **pOldest, double *pMinTime)
{
    size_t iter = 0;
    void *item;

    *pOldest = NULL;
    *pMinTime = 1e20;

    pthread_mutex_lock( &cs_mutex );
    while (hashmap_iter(map, &iter, &item)) 
    {
        const FileEntry *entry = (const FileEntry *)item;
        double seconds = GetAge(entry);
        if (seconds<*pMinTime)
        {
            *pOldest = entry;
            *pMinTime = seconds;
        }
    }
    pthread_mutex_unlock( &cs_mutex );

    return true;
}

#define DELAY 2
void *sync_stuff(void *ptr)
{
    printf("Started syncing thread\n");

    do_rsync("/home/raul/pp.png");

    while(true)
    {
        double age;
        const FileEntry *pOldest = NULL;
        GetOldestMod(&pOldest, &age);
        if (pOldest!=NULL && age>DELAY)
        {
            char filename[1024];
            sprintf(filename, "%s/%s", pExternalDataPath, pOldest->name);
            printf("%s\n", filename);
            do_rsync(filename);
            if (true)
            {
                pthread_mutex_lock( &cs_mutex );
                hashmap_delete(map, &pOldest->name);
                pthread_mutex_unlock( &cs_mutex );
            }
        }
        else
        {
            sleep(DELAY-age);
        }
    }
}

int GetLibDir(char *out);
#include "FilePicker.h"
pthread_t thread1;
void Syncy_Init(void *window)
{
    strcpy(exec_output, "Init OK\n");   

#ifdef ANDROID    
    android_app * pApp = (android_app *)window;
    pInternalDataPath = pApp->activity->internalDataPath;
    pExternalDataPath = pApp->activity->externalDataPath;

    //pExternalDataPath = "/storage/emulated/0/Download";
    pExternalDataPath = "/storage/emulated/0/Download";
    GetLibDir(pExternalLibPath);
    //strcpy(exec_output, pExternalLibPath);//, "/rsync");

    linked_list *files = GetFilesInFolder(pExternalLibPath);
    SortLinkedList(files);
    linked_list *pTmp = files;
    while(pTmp != NULL)
    {
        strcat(exec_output, pTmp->pStr);
        strcat(exec_output, "\n");
        pTmp = pTmp->pNext;
    }
    FreeLinkedList(files);

#else    
    pExternalDataPath = "/home/raul";
#endif


    map = hashmap_new(sizeof(FileEntry), 0, 0, 0, FileEntry_hash, FileEntry_compare, NULL, NULL);    

    fd = inotify_init();
    wd = inotify_add_watch(fd, pExternalDataPath, IN_CLOSE_WRITE);

    int iret1 = pthread_create( &thread1, NULL, sync_stuff, (void*) NULL);
    printf("Starting thread %i\n", iret1);
}

void Syncy_Shutdown()
{
    hashmap_free(map);

    inotify_rm_watch(fd, wd);
    close(fd); 
}


void Syncy_MainLoopStep()
{
    pollfd pd;
    pd.fd = fd;
    pd.events = POLLIN;

    for(;;)
    {
        int timeout= 1; 
        
        int rc = poll(&pd, 1, timeout);
        if ((pd.revents & POLLIN) == 0)
            break;

        //inotify_event ev;
        size_t length = read(fd, buffer, EVENT_BUF_LEN);

        int i=0;
        /*actually read return the list of change events happens. Here, read the change event one by one and process it accordingly.*/
        while ( i < length ) 
        {     
            struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];     
            if ( event->len ) 
            {
                if ( event->mask & IN_CLOSE_WRITE ) 
                {
                    if ( (event->mask & IN_ISDIR) == false) 
                    {
                        char *s = (char *)malloc(strlen(event->name)+1);
                        strcpy(s, event->name);                    

                        FileEntry item;
                        item.name = s;
                        time(&item.rawtime);
                        
                        pthread_mutex_lock( &cs_mutex );
                        hashmap_set(map, &item);
                        pthread_mutex_unlock( &cs_mutex );
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }

    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f)); // place the next window in the top left corner (0,0)
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize); // make the next window fullscreen
    ImGui::Begin("imgui window", NULL, window_flags); // create a window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));

    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    ImGui::Checkbox("Play", &play);
    ImGui::Text("%s", pExternalDataPath);
    ImGui::Text("%s", pInternalDataPath);
    ImGui::Text("%s", pExternalLibPath);

    static int item_selected_idx = -1;
    if (ImGui::BeginListBox("listbox 1"))
    {
        size_t iter = 0;
        void *rawitem;
        pthread_mutex_lock( &cs_mutex );
        while (hashmap_iter(map, &iter, &rawitem)) 
        {
            const FileEntry *item = (const FileEntry *)rawitem;
            const bool is_selected = (item_selected_idx == iter);

            double age = GetAge(item);
            
            char filename[1024];
            sprintf(filename, "%02i - %s", (int)age, item->name);

            if (ImGui::Selectable(filename, is_selected))
                item_selected_idx = iter;
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        pthread_mutex_unlock( &cs_mutex );
        ImGui::EndListBox();
    }

    int line_count = 20;
    float height = ImGui::GetTextLineHeight() * line_count;
    ImGui::BeginChild("TextWrapLimiter", ImVec2(1000, height), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(exec_output);
    ImGui::PopTextWrapPos();
    ImGui::EndChild();

    ImGui::PopStyleVar(); // window padding

    ImGui::End();
}
