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
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#include "hashmap.h"
#include "ExecRsync.h"
#include "misc.h"
#include "Log.h"

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

volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

void gotsig(int sig, siginfo_t *info, void *ucontext) 
{
}

time_t start_time;

char exec_output[10*1024];

const char *pInternalDataPath = NULL;
const char *pExternalDataPath = NULL;
const char *pMediaAndFilesDataPath = NULL;
char pExternalLibPath[1024];
char monitoredFolder[1024];
char destination[1024];
bool play=false;

int fd;
int wd;

struct hashmap *map_pending;

sem_t semaphore;

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
char buffer[EVENT_BUF_LEN];

static pthread_mutex_t cs_mutex =  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

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
    while (hashmap_iter(map_pending, &iter, &item)) 
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

#define DELAY 4

void *sync_stuff(void *ptr)
{
    Log("Started syncing thread");

    while(keepRunning)
    {
        sem_wait(&semaphore);

        if (keepRunning==false)
            break;

        double age;
        const FileEntry *pOldest = NULL;
        GetOldestMod(&pOldest, &age);
        if (pOldest!=NULL)
        {
            if (age<DELAY)
            {
                sleep(DELAY-age); 
            }
            
            char filename[1024];
            sprintf(filename, "%s/%s", monitoredFolder, pOldest->name);
            bool res = do_rsync(
                pExternalLibPath, 
                filename, 
                destination, 
                "/storage/emulated/0/Documents/dropbear_rsa_host_key",
                exec_output);

            //if (res)
            {
                pthread_mutex_lock( &cs_mutex );
                hashmap_delete(map_pending, &pOldest->name);
                pthread_mutex_unlock( &cs_mutex );
            }
        }
    }

    Log("Stop syncing thread");
    
    return NULL;
}

void *poll_notifies(void *ptr)
{
    //int hdlsig = (int)ptr;
/*
    struct sigaction sa;
    sa.sa_handler = NULL;
    sa.sa_sigaction = gotsig;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(0, &sa, NULL) < 0) {
            perror("sigaction");
            return (void *)-1;
    }
*/
    /*
    pollfd pd;
    pd.fd = fd;
    pd.events = POLLIN;
    */
    Log("Start inotify thread");

    while(keepRunning)
    {
        /*
        int timeout= 1; 
        
        int rc = poll(&pd, 1, timeout);
        if ((pd.revents & POLLIN) == 0)
            break;
        */

        //inotify_event ev;
        ssize_t length = read(fd, buffer, EVENT_BUF_LEN);
        if (length<0)
            continue;
            
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
                        if (event->name[0]!='.') // dont process hidden files
                        {
                            char *s = (char *)malloc(strlen(event->name)+1);
                            strcpy(s, event->name);                    

                            FileEntry item;
                            item.name = s;
                            time(&item.rawtime);
                            
                            pthread_mutex_lock( &cs_mutex );
                            hashmap_set(map_pending, &item);
                            pthread_mutex_unlock( &cs_mutex );
                            
                            // Tell the other thread there is work to do
                            sem_post(&semaphore);
                        }
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }    

    Log("Stop inotify thread");

    return NULL;
}

int GetLibDir(char *out);
#include "FilePicker.h"

pthread_t thread1;
pthread_t thread2;

void Syncy_StartApp(void *app)
{
    signal(SIGINT, intHandler);
    signal(SIGQUIT, intHandler);

    strcpy(exec_output, "Init OK\n");  
    
    time(&start_time);

    map_pending = hashmap_new(sizeof(FileEntry), 0, 0, 0, FileEntry_hash, FileEntry_compare, NULL, NULL);    

    char path[1024];

#ifdef ANDROID    
    android_app * pApp = (android_app *)app;
    pInternalDataPath = pApp->activity->internalDataPath;
    pExternalDataPath = pApp->activity->externalDataPath;
    pMediaAndFilesDataPath = "/storage/emulated/0";

#else    
    pMediaAndFilesDataPath = "/tmp";
    pInternalDataPath = pMediaAndFilesDataPath;
    pExternalDataPath = pMediaAndFilesDataPath;

    sprintf(path, "%s%s", pMediaAndFilesDataPath, "/Documents");
    mkdir(path,0700);

    sprintf(path, "%s%s", pMediaAndFilesDataPath, "/DCIM");
    mkdir(path,0700);

    sprintf(path, "%s%s", pMediaAndFilesDataPath, "/DCIM/Camera");
    mkdir(path,0700);
#endif

    sprintf(path, "%s%s", pMediaAndFilesDataPath, "/Documents/server.txt");
    read_string_from_file(path, destination);

    sprintf(path, "%s%s", pMediaAndFilesDataPath, "/Documents/log.txt");
    LogInit(path);
    Log("%s", "StartApp");

    fd = inotify_init();

    sprintf(monitoredFolder, "%s%s", pMediaAndFilesDataPath, "/DCIM/Camera");
    wd = inotify_add_watch(fd, monitoredFolder, IN_CLOSE_WRITE);

    // init background tasks

    sem_init(&semaphore, 0, 0);

    int iret1 = pthread_create( &thread1, NULL, sync_stuff, (void*) NULL);
    printf("Starting thread1 %i\n", iret1);
    int iret2 = pthread_create( &thread2, NULL, poll_notifies, (void*) NULL);
    printf("Starting thread2 %i\n", iret2);
}

void Syncy_StopApp()
{
    keepRunning = 0;

    //wakeup rsync thread 
    sem_post(&semaphore);
    sem_destroy(&semaphore);
    
    //wakeup inotify thread
    inotify_rm_watch(fd, wd);
    close(fd); 

    void *retval;
    pthread_join(thread2, &retval);
    pthread_join(thread1, &retval);

    hashmap_free(map_pending);

    Log("StopApp");
    LogTerm();
}

void Syncy_InitWindow(void *app)
{
#ifdef ANDROID    
    android_app * pApp = (android_app *)app;
    pApp;
    // can't do it earlier
    GetLibDir(pExternalLibPath);
/*    
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
*/    
#endif
}

void Syncy_TermWindow()
{
}


bool Syncy_MainLoopStep()
{
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f)); // place the next window in the top left corner (0,0)
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize); // make the next window fullscreen
    ImGui::Begin("imgui window", NULL, window_flags); // create a window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));

    ImGui::Dummy(ImVec2(0.0f, 60.0f));

    //ImGui::Checkbox("Play", &play);
    ImGui::Text("%s", pExternalDataPath);
    ImGui::Text("%s", pInternalDataPath);

    uint32_t seconds = (uint32_t)difftime(time(0), start_time); 
    int days = seconds / (24*60*60);
    int days_rem = seconds % (24*60*60);
    int hours =  days_rem / (60*60);
    int hours_rem =  days_rem % (60*60);
    int mins =  hours_rem / (60);
    int mins_rem = hours_rem % (60);
    int secs = mins_rem;
    ImGui::Text("Uptime: %i days, %02i:%02i:%02i",days, hours, mins, secs);

    static int item_selected_idx = -1;
    if (ImGui::BeginListBox("Pending"))
    {
        size_t iter = 0;
        void *rawitem;
        pthread_mutex_lock( &cs_mutex );
        while (hashmap_iter(map_pending, &iter, &rawitem)) 
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

    ImGui::Text("dest: %s",destination);

    int line_count = 40;
    float height = ImGui::GetTextLineHeight() * line_count;
    ImGui::BeginChild("TextWrapLimiter", ImVec2(1000, height), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(exec_output);
    ImGui::PopTextWrapPos();
    ImGui::EndChild();

    ImGui::PopStyleVar(); // window padding

    ImGui::End();

    return keepRunning;
}
