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

// compile: gcc replay.c -pthread
//Note: all sizes are in bytes
// CONFIGURATION PART

#define MEM_ALIGN 4096
#define LARGEST_REQUEST_SIZE (32 * 1024 * 1024)

int maxio = 4000000; //halt if number of IO > maxio, to prevent printing too many to metrics file

int64_t DISK_SIZE = 0;
pthread_t *tid; 


//OTHER GLOBAL VARIABLES
int fd;
int totalio;

struct io_u {
    double timestamp;
    int rw; //1 for read, 0 for write
    int64_t offset;
    int64_t buflen;
};

unsigned char *eightpagemap; //8*4k map
int64_t dirty_pages = 0;


/*=============================================================*/

/* get disk size in bytes */
int64_t get_disksz_in_bytes(int devfd)
{
    int64_t sz;
    ioctl(devfd, BLKGETSIZE64, &sz);
    return sz;
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
void set_8page_map(int64_t offset, int64_t len) {
    int64_t page_offset = offset / 4096;
    int64_t page_len = len / 4096;
    unsigned char *eight_page =  eightpagemap + page_offset/8;
    int eight_page_idx = page_offset % 8;
    for (int i = 0; i < page_len; i++) {
        if (*eight_page & (1 << eight_page_idx)) {
            dirty_pages ++;
        }
        *eight_page |= (1 << eight_page_idx);
        eight_page_idx ++;
        if (eight_page_idx == 8) {
            eight_page_idx = 0;
            eight_page ++;
        }
    }
}
void arrangeIO(char **requestarray){
    for(int i = 0; i < totalio; i++){
        char *trace_req = requestarray[i];
        struct io_u iou;
        struct io_u *io = &iou;
        int64_t io_end;
        /*IO arrival time */ 
        io->timestamp = atof(strtok(trace_req," ")); 
        /*ignore dev no */
        strtok(NULL," ");  
        /*offset in sectors to bytes*/
        io->offset = (int64_t)atoll(strtok(NULL," ")) * 512; 
        io->offset %= (DISK_SIZE); 
        /*request size in sectors to bytes*/
        io->buflen = atoi(strtok(NULL," ")) * 512;
        io_end = io->offset + io->buflen;

        /* Round offset down to 4K, Round end up to 4K */
        io->offset = io->offset/4096*4096;
        io_end = 4096 * (1 + ((io_end - 1)/4096));

        io->buflen = io_end - io->offset;
        /*IO type, 1 for read and 0 for write */
        io->rw = atoi(strtok(NULL," ")); 
        if (io->offset + io->buflen >= DISK_SIZE) {
            io->offset = DISK_SIZE - io->buflen;
        }
        set_8page_map(io->offset, io->buflen);
    }
}


void performIO(){    
    void *buff;
    unsigned char *map_8page = eightpagemap;
    int ret;
    int64_t page_count = DISK_SIZE/4096;
    int64_t written = 0;

    if (posix_memalign(&buff, MEM_ALIGN, LARGEST_REQUEST_SIZE)){
        perror("memory allocation failed");
        exit(-1);
    }

    for(int64_t i = 0; i < page_count; i++) {
        printf("Touching Progress: %.2f%%, dirtying %ld/%ld       \r",(float)i / page_count * 100, written, dirty_pages);
        int j = 0;
        if ((*map_8page) & (1 << j)) {
            written ++;
            ret = pwrite(fd, buff, 4096, i*4096);
            if (ret < 0) {
                perror("Write error");
                exit(-1);
            }
        }
        j++;
        if (j == 8) {
            j = 0;
            map_8page++;
        }
    }
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
        perror("Cannot open");
        exit(-1);
    }

    DISK_SIZE = get_disksz_in_bytes(fd);
    eightpagemap = (unsigned char *)calloc(DISK_SIZE/4096/8, sizeof(unsigned char));
    // read the trace before everything else
    totalio = readTrace(&request, argv[2]);
    performIO();

    free(eightpagemap);
    return 0;
}
