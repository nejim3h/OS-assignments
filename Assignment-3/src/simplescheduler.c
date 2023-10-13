#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>    
#include <sys/wait.h>  
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>

int NCPUs;
int TSLICE;


static void my_handler(int signum) {
    if (signum == SIGINT) {
        exit(0);
    }
}


char* getProcessState(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *procfile = fopen(path, "r");
    if (procfile == NULL) {
        return "Not Found";
    }

    char line[256];
    char* state = "Unknown";

    while (fgets(line, sizeof(line), procfile)) {
        if (strncmp(line, "State:", 6) == 0) {
            state = line + 7; 
            break;
        }
    }

    fclose(procfile);
    // printf("%s",state);
    return state;
}

typedef struct Process{
    char pname[100];
    int pid;
    float start_time;
}Process;

struct queue{
    int array[100];
    int front;
    int rear;
    sem_t mutex;
}typedef queue;


int dequeue(queue* q) {
    int to_return = q->array[q->front+1];
    q->front++;
    return to_return;
}

void enqueue (queue* q,int p){
    q->rear++;
    q->array[q->rear]=p;
}

int isEmpty(queue* pt){
    if(pt->front==pt->rear){
        return 1;
    }
    return 0;
}

// void scheduler(queue* ready_queue) {


// }

int main(int argv, char**argc){

    NCPUs = atoi(argc[1]);  
    TSLICE = atoi(argc[2]); 


    struct sigaction sig;
    memset(&sig, 0, sizeof(sig));
    sig.sa_handler = my_handler;
    sigaction(SIGINT, &sig, NULL);

    int fd = shm_open("shared memory", O_RDWR, 0666);

    queue* ready_queue = mmap(0, sizeof(queue), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    // scheduler(ready_queue);

    while(1){
        int running_array[NCPUs];
        int running_count = 0;
        if(!isEmpty(ready_queue)){
            sem_wait(&ready_queue->mutex);
            while(!isEmpty(ready_queue) && running_count<NCPUs){
                // printf("i - %d\n",running_count);
                printf(" ");
                running_array[running_count]=dequeue (ready_queue);
                // printf("in sched- %d\n",running_array[running_count]);
                kill(running_array[running_count], SIGCONT);
                running_count++; 
            }
            sem_post(&ready_queue->mutex);
        }


        if(running_count>0){
            sleep(TSLICE);
            sem_wait(&ready_queue->mutex);
            for (int i = 0; i < running_count; i++) {
                if(getProcessState(running_array[i])[0]!='Z'){
                    // printf("YES\n");
                    kill(running_array[i], SIGSTOP);
                    enqueue(ready_queue, running_array[i]);
                }
            }
            sem_post(&ready_queue->mutex);
        }

    }  

    return 0;
}
