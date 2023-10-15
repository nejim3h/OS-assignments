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

int NCPU;
int TSLICE;

typedef struct{
    int pid;
    char name[100];
    char first_arg[100];
    time_t prev_queued_time;
    double wait_time;
    time_t execution_time;
    int priority;
} process;

typedef struct {
    process arr[100];
    int size;
    int capacity;
    sem_t mutex;
    time_t first_arrival;
}priority_queue;
 

void insertHelper(priority_queue* pq, int index)
{
 
    // Store parent of element at index
    // in parent variable
    int parent = (index - 1) / 2;
 
    if (pq->arr[parent].priority > pq->arr[index].priority) {
        // Swapping when child is smaller
        // than parent element
        process temp = pq->arr[parent];
        pq->arr[parent] = pq->arr[index];
        pq->arr[index] = temp;
 
        // Recursively calling insertHelper
        insertHelper(pq, parent);
    }
    else if(pq->arr[parent].priority == pq->arr[index].priority){
        if(pq->arr[parent].prev_queued_time > pq->arr[index].prev_queued_time){
            process temp = pq->arr[parent];
            pq->arr[parent] = pq->arr[index];
            pq->arr[index] = temp;  
            insertHelper(pq, parent);
        }
    }
}

void insert(priority_queue* pq, process p)
{
 
    if (pq->size < pq->capacity) {
        pq->arr[pq->size] = p;
        insertHelper(pq, pq->size);
        pq->size++;
    }
}

int first_job=0;

struct cmnd_Elt {
    char command[1024];
    int pid;
    time_t start_time;
    double execution_time;
};

struct cmnd_Elt cmnd_Array[100];
int cmnd_count = 0;
int ctrl_clicked = 0;

static void my_handler(int signum) {
    if (signum == SIGINT) {
        ctrl_clicked = 1;
    }
}



int read_user_input(char* input, int size, char* command, char** arguments) {
    fgets(input, size, stdin);
    if (ctrl_clicked) {
        return 5;
    }
    if (cmnd_count < 100) {
        strcpy(cmnd_Array[cmnd_count].command, input);
        cmnd_count++;
    }

    input[strcspn(input, "\n")] = '\0';
    char* temp = strtok(input, " \n");
    strcpy(command, temp);

    int i = 0;
    while (temp != NULL) {
        arguments[i] = temp;
        temp = strtok(NULL, " \n");
        i++;
    }
    arguments[i] = NULL;
    return 0;
}



void create_process_for_scheduler(priority_queue* ready_queue, char** arguments) {
    char executable[1024];
    strcpy(executable,arguments[1]);

    int length = strlen(executable);  //  executable = ./fib

    if (length < 3) {
        printf("Invalid input: %s\n", executable);
        return;
    }

    char* new_arguments[1024];

    char first_argument[length - 1];
    int j = 0;

    for (int i = 2; i < length; i++) {
        first_argument[j] = executable[i];
        j++;
    }
    first_argument[j] = '\0';           // first argument = fib

    int i=0;
    while(arguments[i]!=NULL){
        i++;
    }
    int size_of_arguments=i;
    new_arguments[0]=first_argument;
    int k=2;
    if(size_of_arguments>2){
        i=1;
        while(arguments[k+1]!=NULL){ 
            new_arguments[i]=arguments[k];
            i++;
            k++;
        }
        if(atoi(arguments[k])>4){
            printf("Valid priority is in the range [1,4].\n");
            return;
        }

        if(atoi(arguments[k])<1){
            printf("Valid priority is in the range [1,4].\n");
            return;
        }

    }
    else{
        i=1;
        while(arguments[k]!=NULL){ 
            new_arguments[i]=arguments[k];
            i++;
            k++;
        }   
    }
    
    char cNCPU[3];
    snprintf(cNCPU, sizeof(cNCPU), "%d", NCPU);
    new_arguments[i]=cNCPU;
    new_arguments[i+1]=NULL;
    // new_arguments[i]=NULL;

    int child_PID = fork();
    if (child_PID < 0) {
        printf("Something went wrong.\n");
        exit(10);
    } else if (child_PID == 0) {
        int my_pid= getpid();
        kill(my_pid,SIGSTOP);
        if (execvp(executable, new_arguments) == -1) {
            perror("execlp");
            exit(10);
        }
    } else {

        process p;
        strcpy(p.name,executable);
        if(new_arguments[1]!=NULL){
            strcpy(p.first_arg,new_arguments[1]);
        }
        else{
            strcpy(p.first_arg,"NULL");
        }
        p.pid=child_PID;
        p.prev_queued_time=time(NULL);
        p.wait_time=0;
        if(size_of_arguments>2){
            p.priority=atoi(arguments[k]);
        }
        else{
            p.priority=1;
        }
        // printf("priority - %d\n", p.priority);
        sem_wait(&ready_queue->mutex);
        insert(ready_queue,p);
        sem_post(&ready_queue->mutex);
        if(first_job==0){
            ready_queue->first_arrival=p.prev_queued_time;
            first_job=1;
        }
    }
}



int launch(char* command, char** arguments) {
    int status;
    // status = create_process_and_run(command, arguments);
    return status;
}


void shell_Loop(priority_queue* ready_queue) {
    char input[1024];
    char command[1024];
    char* arguments[1024];
    char cNCPU[3];
    char cTSLICE[4];

    int sched_pid=fork();
    if(sched_pid==0){                           
        snprintf(cNCPU, sizeof(cNCPU), "%d", NCPU);
        snprintf(cTSLICE, sizeof(cTSLICE), "%d", TSLICE);              
        execlp("./adv_sched", "adv_sched",cNCPU, cTSLICE,NULL);
    }
    else{
        do {
            printf(">>> $ ");
            fflush(stdout);
            int sig_received = read_user_input(input, sizeof(input), command, arguments);
            if (sig_received) {
                kill(sched_pid,SIGINT);
                sem_destroy(&ready_queue->mutex); 
                munmap(ready_queue, sizeof(priority_queue));
                shm_unlink("sm2");
                return;
            }
            if (strcmp(command, "submit") == 0) {
                if (arguments[1] != NULL) {
                    if (ready_queue->size< 100) {
                        // int i=0;
                        // while(arguments[i]!=NULL){
                        //     printf("%s\n",arguments[i]);
                        //     i++;
                        // }
                        create_process_for_scheduler(ready_queue,arguments);
                    } else {
                        printf("Maximum process limit reached.\n");
                    }
                } else {
                    printf("Usage: submit <pname>\n");
                }
            }else {
                launch(command, arguments);
            }
        } while (1);
    }
}

int main(int argv, char**argc) {
    
    NCPU = atoi(argc[1]);  
    TSLICE = atoi(argc[2]);  

    struct sigaction sig;
    memset(&sig, 0, sizeof(sig));
    sig.sa_handler = my_handler;
    sigaction(SIGINT, &sig, NULL);

    int fd = shm_open("sm2", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(priority_queue));

    priority_queue* ready_queue = mmap(0, sizeof(priority_queue), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    sem_init(&ready_queue->mutex, 1, 1);
    ready_queue->capacity=100;
    ready_queue->size=0;
    shell_Loop(ready_queue);

    return 0;
}