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

int end_print_history=0;

static void my_handler(int signum) {
    if (signum == SIGINT) {
        end_print_history=1;
        // exit(0);
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

typedef struct{
    int pid;
    char name[100];
    char first_arg[100];
    time_t prev_queued_time;
    double wait_time;
    time_t execution_time;
    int priority;
} process;

struct queue{
    process array[100];
    int front;
    int rear;
    sem_t mutex;
    time_t first_arrival;
}typedef queue;


// process dequeue(queue* q) {
//     process to_return = q->array[q->front+1];
//     q->front++;
//     return to_return;
// }


process dequeue(queue* q) {
    process to_return;
    int highest_priority_index = -1;
    int highest_priority = -1;
    for (int i = q->front + 1; i <= q->rear; i++) {
        if (q->array[i].priority > highest_priority) {
            highest_priority = q->array[i].priority;
            highest_priority_index = i;
        }
    }
    if (highest_priority_index == -1) {
        // The queue is empty.
        return to_return;
    }
    to_return = q->array[highest_priority_index];
    for (int i = highest_priority_index; i < q->rear; i++) {
        q->array[i] = q->array[i + 1];
    }
    q->rear--;
    if (q->front > q->rear) {
        // The queue is now empty after dequeuing the last element.
        q->front = q->rear = -1;
    }
    return to_return;
}

void enqueue(queue* q, process p) {
    if (q->front == -1 && q->rear == -1) {
        q->front = 0;
        q->rear = 0;
        q->array[q->rear] = p;
    } else {
        int i;
        for (i = q->rear; i >= q->front; i--) {
            if (p.priority < q->array[i].priority) {
                q->array[i + 1] = q->array[i];
            } else {
                break;
            }
        }
        q->array[i + 1] = p;
        q->rear++;
    }
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

    process all_processes[1000];
    int process_number=0;

    while(!end_print_history){
        process running_array[NCPUs];
        int running_count = 0;
        if(!isEmpty(ready_queue)){
            sem_wait(&ready_queue->mutex);
            while(!isEmpty(ready_queue) && running_count<NCPUs){
                // printf("i - %d\n",running_count);
                printf(" ");
                running_array[running_count]=dequeue (ready_queue);
                // printf("in sched- %d\n",running_array[running_count]);
                kill(running_array[running_count].pid, SIGCONT);
                time_t starting_time;
                time(&starting_time);
                running_array[running_count].wait_time+= difftime(starting_time,running_array->prev_queued_time);
                running_count++; 
            }
            sem_post(&ready_queue->mutex);
        }


        if(running_count>0){
            sleep(TSLICE);
            sem_wait(&ready_queue->mutex);
            for (int i = 0; i < running_count; i++) {
                if(getProcessState(running_array[i].pid)[0]!='Z'){
                    // printf("YES\n");
                    kill(running_array[i].pid, SIGSTOP);
                    running_array[i].prev_queued_time=time(NULL);
                    enqueue(ready_queue, running_array[i]);
                }
                else{
                    running_array[i].execution_time=time(NULL);
                    all_processes[process_number]=running_array[i];
                    process_number++;
                }
            }
            sem_post(&ready_queue->mutex);
        }

    }  
    
    printf("\n--------------------------------\n");
    printf("Name   PID   Wait Time    Execution Time\n");
    for(int i=0;i<process_number;i++){
        printf("%s ",all_processes[i].name);
        if(strcmp(all_processes[i].first_arg,"NULL")!=0){
            printf("%s ",all_processes[i].first_arg);
        }
        printf("%d ",all_processes[i].pid);
        // printf("%s ",ctime(&all_processes[i].prev_queued_time));
        printf("%.2lf seconds ",all_processes[i].wait_time);
        // printf("%s \n",ctime(&all_processes[i].execution_time));
        printf("%.2lf seconds\n",difftime(all_processes[i].execution_time,ready_queue->first_arrival));
    }

    return 0;
}
