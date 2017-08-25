#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <inttypes.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <linux/kernel.h>
#include <sys/syscall.h>

#define NR_START_STAMP 332

// compile: gcc replay.c -pthread
//Note: all sizes are in bytes
// CONFIGURATION PART

#define MEM_ALIGN 4096
#define LARGEST_REQUEST_SIZE (32 * 1024 * 1024)

int numworkers = 128; // =number of threads
int printlatency = 1; //print every io latency
int maxio = 4000000; //halt if number of IO > maxio, to prevent printing too many to metrics file
int respecttime = 1;

int64_t DISK_SIZE = 0;
pthread_t *tid; 


//OTHER GLOBAL VARIABLES
int fd;
int totalio;
int jobtracker = 0;
int latecount = 0;
int slackcount = 0;
int readcount = 0;
int writecount = 0;
void *buff;
uint64_t starttime;

struct io_u {
    double timestamp;
    int rw; //1 for read, 0 for write
    int64_t offset;
    int64_t buflen;
};

struct io_u *all_io_u;

FILE *metrics; //current format: offset,size,type,latency(ms)

pthread_mutex_t lock;

/*=============================================================*/

/* get disk size in bytes */
int64_t get_disksz_in_bytes(int devfd)
{
    int64_t sz;
    ioctl(devfd, BLKGETSIZE64, &sz);
    return sz;
}

void prepareMetrics(){
    if(printlatency == 1 && totalio > maxio){
        fprintf(stderr, "Too many IO in the trace file!\n");
        exit(1);
    }
    if(printlatency == 1){
        metrics = fopen("replay_metrics.txt", "w+");
        
        if(!metrics){
            fprintf(stderr,"Error creating metrics file!\n");
            exit(1);
        }
    }
}

int readTrace(char ***req, char *tracefile){
    //first, read the number of lines
    FILE *trace = fopen(tracefile,"r");
    if (trace == NULL) {
        printf("Cannot open trace file: %s!\n", tracefile);
    }
    int ch;
    int numlines = 0;
    
    while(!feof(trace)){
        ch = fgetc(trace);
        if(ch == '\n'){
            numlines++;
        }
    }
    
    rewind(trace);

    //then, start parsing
    if((*req = malloc(numlines * sizeof(char*))) == NULL){
        fprintf(stderr,"Error in memory allocation\n");
        exit(1);
    }
    
    char line[100]; //assume it will not exceed 100 chars
    int i = 0;
    while(fgets(line, sizeof(line), trace) != NULL){
        line[strlen(line) - 1] = '\0';
        if(((*req)[i] = malloc((strlen(line) + 1) * sizeof(char))) == NULL){
            fprintf(stderr,"Error in memory allocation\n");
            exit(1);
        } //error here
        strcpy((*req)[i],line);
        i++;
    }
    fclose(trace);
    
    return numlines;
}

void arrangeIO(char **requestarray){
    all_io_u = malloc(totalio * sizeof(struct io_u));
    
    if(all_io_u == NULL){
        fprintf(stderr,"Error malloc in arrangeIO!\n");
        exit(1);
    }
    
    int i = 0;
    for(i = 0; i < totalio; i++){
        char *trace_req = requestarray[i];
        struct io_u *io = &all_io_u[i];
        /*IO arrival time */ 
        io->timestamp = atof(strtok(trace_req," ")); 
        /*ignore dev no */
        strtok(NULL," ");  
        /*offset in sectors to bytes*/
        io->offset = (int64_t)atoll(strtok(NULL," ")) * 512; 
        /*in case I/O size more than 4MB */
        io->offset %= (DISK_SIZE); 
        /*request size in sectors to bytes*/
        io->buflen = atoi(strtok(NULL," "));
        io->buflen = 1 + ((io->buflen - 1)/4);
        io->buflen *= 4096;
        /*IO type, 1 for read and 0 for write */
        io->rw = atoi(strtok(NULL," ")); 
        if (io->offset + io->buflen >= DISK_SIZE) {
            io->offset = DISK_SIZE - io->buflen;
        }
    }
}

void atomicAdd(int *val, int add){
    assert(pthread_mutex_lock(&lock) == 0);
    (*val) += add;
    assert(pthread_mutex_unlock(&lock) == 0);
}

int atomicReadAndReset(int *val){
    int va;
    assert(pthread_mutex_lock(&lock) == 0);
    va = *val;
    (*val) = 0;
    assert(pthread_mutex_unlock(&lock) == 0);
    return va;
}

void *performIO(){    
    void *buff;
    int curtask;
    int mylatecount = 0;
    int myslackcount = 0;
    struct timeval now,t1,t2;
    struct io_u *io_task;
    
    useconds_t sleep_time;

    if (posix_memalign(&buff, MEM_ALIGN, LARGEST_REQUEST_SIZE)){
        fprintf(stderr,"memory allocation failed\n");
        exit(1);
    }
    
    while(jobtracker < totalio){
        int ret;
        //firstly save the task to avoid any possible contention later
        assert(pthread_mutex_lock(&lock) == 0);
        curtask = jobtracker;
        jobtracker++;
        assert(pthread_mutex_unlock(&lock) == 0);
        io_task = &all_io_u[curtask];
        //respect time part
        if(respecttime == 1){
            gettimeofday(&now,NULL); //get current time
            uint64_t elapsedtime = now.tv_sec * 1000000 + now.tv_usec - starttime;
            if(elapsedtime <= io_task->timestamp * 1000){
                sleep_time = (useconds_t)(io_task->timestamp * 1000) - elapsedtime;
                if(sleep_time > 100000){
                    myslackcount++;
                }    
                usleep(sleep_time);
            }else{ //I am late
                mylatecount++;
            }
        }
          
        //do the job      
        if(io_task->rw == 0){
            atomicAdd(&writecount, 1);
            gettimeofday(&t1,NULL); //reset the start time to before start doing the job
            ret = pwrite(fd, buff, io_task->buflen, io_task->offset);
            gettimeofday(&t2, NULL);
        }else{
            atomicAdd(&readcount, 1);
            gettimeofday(&t1,NULL); //reset the start time to before start doing the job
            ret = pread(fd, buff, io_task->buflen, io_task->offset);
            gettimeofday(&t2, NULL);
        }
        if (ret < 0) {
            fprintf(stderr,"Cannot %s size %ld to offset %ld!\n", io_task->rw?"read":"write", io_task->buflen, io_task->offset);
            exit(1);
        }
        /* Coperd: I/O latency in us */
        int iotime = (t2.tv_sec - t1.tv_sec) * 1e6 + (t2.tv_usec - t1.tv_usec);
        if(printlatency == 1){
            assert(pthread_mutex_lock(&lock) == 0);
            /* 
             * Coperd: keep consistent with fio latency log format:
             * 1: timestamp in ms
             * 2: latency in us
             * 3: r/w type [0 for w, 1 for r] (this is opposite of fio)
             * 4: I/O size in bytes
             * 5: offset in bytes
             */
            fprintf(metrics,"%.3f,%d,%d,%ld,%ld\n",io_task->timestamp, iotime, io_task->rw, io_task->buflen, io_task->offset);
            assert(pthread_mutex_unlock(&lock) == 0);
        }
    }
    
    atomicAdd(&latecount, mylatecount);
    atomicAdd(&slackcount, myslackcount);
    free(buff);
    while (1) {
        sleep(1);
    }
    return NULL;
}

void *printProgress(){
    int readcnt, writecnt;
    int finish = 0;
    int x;
    while(jobtracker <= totalio){
        readcnt = atomicReadAndReset(&readcount);
        writecnt = atomicReadAndReset(&writecount);
        printf("Progress: %.2f%%(%d/%d) (%d/%d)            \r",(float)jobtracker / totalio * 100,
                jobtracker, totalio, readcnt, writecnt);
        fflush(stdout);
        
        if(jobtracker == totalio){
            finish ++;
            if (finish == 5) {
                break;
            }
        }
        
        sleep(1);
    }
    for (x = 0; x < numworkers; x++) {
        pthread_cancel(tid[x]);
    }
    printf("\n");
    
    return NULL;
}

void operateWorkers(){
    struct timeval t1,t2;
    float totaltime;
    
    printf("Start doing requests...\n");
    
    // thread creation
    tid = malloc(numworkers * sizeof(pthread_t));
    if(tid == NULL){
        fprintf(stderr,"Error malloc thread!\n");
        exit(1);
    }
    pthread_t track_thread; //progress
    
    assert(pthread_mutex_init(&lock, NULL) == 0);
    
    int x;
    syscall(NR_START_STAMP);
    gettimeofday(&t1,NULL);
    starttime = t1.tv_sec * 1000000 + t1.tv_usec;
    for(x = 0; x < numworkers; x++){
        pthread_create(&tid[x], NULL, performIO, NULL);
    }
    pthread_create(&track_thread, NULL, printProgress, NULL); //progress
    for(x = 0; x < numworkers; x++){
        pthread_join(tid[x], NULL);
    }
    pthread_join(track_thread, NULL); //progress
    gettimeofday(&t2,NULL);
    totaltime = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
    printf("==============================\n");
    printf("Total run time: %.3f ms\n",totaltime);
    if(respecttime == 1){
        printf("Late rate: %.2f%%\n",100 * (float)latecount / totalio);
        printf("Slack rate: %.2f%%\n",100 * (float)slackcount / totalio);
    }
    
    fclose(metrics);
    assert(pthread_mutex_destroy(&lock) == 0);
    
    //run statistics
    system("python statistics.py");
}

int main(int argc, char *argv[]) {
    char device[64];
    char **request;
    
    if (argc != 3){
        printf("Usage: ./replayer /dev/md0 tracefile\n");
        exit(1);
    }else{
        printf("%s\n", argv[1]);
        sprintf(device,"%s",argv[1]);
        printf("Disk ==> %s\n", device);
    }

    // start the disk part
    fd = open(device, O_DIRECT | O_RDWR);
    if(fd < 0) {
        fprintf(stderr,"Cannot open %s\n", device);
        exit(1);
    }

    DISK_SIZE = get_disksz_in_bytes(fd);
    
    // read the trace before everything else
    totalio = readTrace(&request, argv[2]);
    arrangeIO(request);

    //check cache if needed
   /* if(check_cache == 1){
        checkCache(fd);
    }
    cleanCache();
    printf("After cleancache\n");*/
    prepareMetrics();
    printf("After prepareMetrics\n");
    operateWorkers();

    
    return 0;
}
