#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>
#include <termios.h>


#define MM2S_CONTROL_REGISTER       0x00
#define MM2S_STATUS_REGISTER        0x04
#define MM2S_SRC_ADDRESS_REGISTER   0x18
#define MM2S_TRNSFR_LENGTH_REGISTER 0x28

#define S2MM_CONTROL_REGISTER       0x30
#define S2MM_STATUS_REGISTER        0x34
#define S2MM_DST_ADDRESS_REGISTER   0x48
#define S2MM_BUFF_LENGTH_REGISTER   0x58

#define IOC_IRQ_FLAG                1<<12
#define IDLE_FLAG                   1<<1

#define STATUS_HALTED               0x00000001
#define STATUS_IDLE                 0x00000002
#define STATUS_SG_INCLDED           0x00000008
#define STATUS_DMA_INTERNAL_ERR     0x00000010
#define STATUS_DMA_SLAVE_ERR        0x00000020
#define STATUS_DMA_DECODE_ERR       0x00000040
#define STATUS_SG_INTERNAL_ERR      0x00000100
#define STATUS_SG_SLAVE_ERR         0x00000200
#define STATUS_SG_DECODE_ERR        0x00000400
#define STATUS_IOC_IRQ              0x00001000
#define STATUS_DELAY_IRQ            0x00002000
#define STATUS_ERR_IRQ              0x00004000

#define HALT_DMA                    0x00000000
#define RUN_DMA                     0x00000001
#define RESET_DMA                   0x00000004
#define ENABLE_IOC_IRQ              0x00001000
#define ENABLE_DELAY_IRQ            0x00002000
#define ENABLE_ERR_IRQ              0x00004000
#define ENABLE_ALL_IRQ              0x00007000

#define PRF 256
#define PRT 1/PRF

// Global variables
void *bram, *gpio_1, *gpio;
int num_ec, num_rp, ack_finish, timer, finished_count = 0;
FILE* global_errlog, * timelog, * bytelog, * fptr , *report, *errlog;
uint32_t *buffer1;
struct timeval ts, ts1, ts2, ts10,ts11;
char path[100], dirname[100], filename[100], timename[100], reportname[100], errlogname[100];
time_t now;
uint8_t run = 1;
unsigned int *reset_gpio, *trigger_gpio, *addr_gpio, *written_out, *decimation, *buflength,  *idleImaging,
             *dma_virtual_addr,*virtual_dst_addr;


//DMA Fucntions

unsigned int write_dma(unsigned int *virtual_addr, int offset, unsigned int value)
{
    virtual_addr[offset>>2] = value;
    return 0;
}

unsigned int read_dma(unsigned int *virtual_addr, int offset)
{
    return virtual_addr[offset>>2];
}

void dma_s2mm_status(unsigned int *virtual_addr)
{
    unsigned int status = read_dma(virtual_addr, S2MM_STATUS_REGISTER);
    // Your existing code...
}

int dma_s2mm_sync(unsigned int *virtual_addr)
{
    unsigned int s2mm_status = read_dma(virtual_addr, S2MM_STATUS_REGISTER);

	// sit in this while loop as long as the status does not read back 0x00001002 (4098)
	// 0x00001002 = IOC interrupt has occured and DMA is idle
	while(!(s2mm_status & IOC_IRQ_FLAG) || !(s2mm_status & IDLE_FLAG))
	{
        dma_s2mm_status(virtual_addr);

        s2mm_status = read_dma(virtual_addr, S2MM_STATUS_REGISTER);
    }


	return 0;
}
void print_mem(void *virtual_address, int byte_count)
{
    char *data_ptr = virtual_address;
    for(int i = 0; i < byte_count; i++) {
        printf("%02X", data_ptr[i]);
        if(i % 4 == 3) {
            printf(" ");
        }
    }

    printf("\n");
}

void signal_handler(int signum);

int initialize(uint32_t decim, uint32_t buflen){
	gettimeofday(&ts, NULL);
	timer = (int)ts.tv_sec;

	// Access device memory
    printf("********************* nmap regiters *********************************\n");
    printf("SAR Capture Code.\n");

    printf("Opening a character device file of the Arty's DDR memory...\n");
    int ddr_memory = open("/dev/mem", O_RDWR | O_SYNC);
    if(ddr_memory<0){
  		perror("open");
	//	fprintf(global_errlog, "%d - ERROR: Could not open file in device memory \n", timer); 
		return 0;      
    }

    printf("Memory map the address of the AXI GPIO 0 for RESET.\n");
    unsigned int* reset_gpio = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0xA0020000);
    if (reset_gpio == MAP_FAILED){
		perror("mmap reset_gpio");
	//	fprintf(global_errlog, "%d - ERROR: Could not access reset_gpio \n", timer); 
		return 0;
	}

    printf("Memory map the address of the AXI GPIO 1 for trigger.\n");
    trigger_gpio = mmap(NULL, 65535, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0xA0030000);
    if (trigger_gpio == MAP_FAILED){
		perror("mmap trigger_gpio");
	//	fprintf(global_errlog, "%d - ERROR: Could not access trigger_gpio \n", timer); 
		return 0;
	}


	printf("Memory map the address of the DMA AXI IP via its AXI lite control interface register block.\n");
    dma_virtual_addr = mmap(NULL, 65535, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0xA0010000);
    if (dma_virtual_addr == MAP_FAILED){
		perror("mmap dma_virtual_addr");
	//	fprintf(global_errlog, "%d - ERROR: Could not access dma_virtual_addr \n", timer); 
		return 0;
	}

	printf("Memory map the S2MM destination address register block.\n");
    virtual_dst_addr = mmap(NULL, 40, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0x0f000000);
    if (virtual_dst_addr == MAP_FAILED){
		perror("mmap virtual_dst_addr");
	//	fprintf(global_errlog, "%d - ERROR: Could not access virtual_dst_addr \n", timer); 
		return 0;
	}

    //Reset the system
    printf("\nSetting Reset Low");
    *((uint32_t*)(reset_gpio + 0)) =0;
    usleep(10);
    printf("\nSetting Reset High");
    *((uint32_t*)(reset_gpio + 0)) =1;
	
	printf("Initialized!!\n");
	return 1;
}

int read_boot_count() {

	//FILE *fboot, *fboot1;
	
	gettimeofday(&ts, NULL);
	timer = (int)ts.tv_sec;

	num_rp = 12;
	num_ec = 21;
	
	printf("Boot count read!!\n");
	return 1;
}	

int fcreate(){

	FILE* fboot;

	// Get timestamp
	gettimeofday(&ts, NULL);
	timer = (int)ts.tv_sec;
    	
    char root_dir [100] = "";
    	
	// Create a dir matching ec boot count.  
	if (num_ec > 0)
		sprintf(root_dir, "/home/E%d", num_ec);
	else
		sprintf(root_dir, "/home/Exx");	
		
	if (mkdir(root_dir, 0777) != 0){
		fprintf(global_errlog, "%d - ERROR: Could not create root dir %s; Might already exist\n", timer, root_dir);
	//	return 0;
	}

	sprintf(path, "%s/sar/", root_dir); 
	if (mkdir(path, 0777) != 0){
		fprintf(global_errlog, "%d - ERROR: Could not create sar dir %s; Might already exist\n", timer,path);
	//	return 0;
	}

	int err;
	sprintf(dirname, "%sR%d_EC%d_S%d", path, num_rp, num_ec, timer);
	err = mkdir(dirname, 0777);

	if (err != 0) {
		fprintf(global_errlog, "%d - ERROR: Could not open file to store: [%s]\n", timer,strerror(err));
	//	return 0;
	}
	

	sprintf(errlogname, "%s/error_logger_R%d_E%d_T%d.txt", path, num_rp, num_ec, timer);
    errlog = fopen(errlogname, "a");

	
	//sprintf(filename, "%s/daq_R%d_EC%d_T%d.bin", dirname, num_rp, num_ec, timer);
	sprintf(filename, "%s/daq_R%d_EC%d_T%d.txt", dirname, num_rp, num_ec, timer);
	printf("Data stored in %s", filename);
	
    // Open timestamp log
	sprintf(timename, "%s/timestamp_logger_R%d_EC%d_T%d.txt", dirname, num_rp, num_ec, timer);
	timelog = fopen(timename, "a");
	
	// Open data write file
	
	if ((fptr = fopen(filename, "a")) == NULL) {
		fprintf(errlog, "%d - ERROR: Could not open file to store %s\n", timer, filename);
	}

	// Open Flight report
    sprintf(reportname, "%s/daq_report_R%d_EC%d_T%d.txt", path, num_rp, num_ec, timer);
    report = fopen(reportname, "a");

	fprintf(errlog, "*************** DAQ Error Logger ***************\n\n");

	// Print info into Flight report
	fprintf(report, "*************** DAQ Flight Report ***************\n\n");

	fprintf(report, "RP Boot count: %d\nEC Boot count: %d\n\n", num_rp, num_ec);
	gettimeofday(&ts, NULL);
	fprintf(report, "Current Timestamp: %lf\n\n", (double)ts.tv_sec + (ts.tv_usec * 1.0 / 1000000.0));

	printf("Crossed milestone 1\n");


	fflush(report);
	fflush(errlog);

	fprintf(report, "Files created!!\n");
	printf("Files created!!\n");
	 
	return 1;
}

int store_data(uint32_t decim, uint32_t buflen) {

	int imaging, idle, created_flag=1, written, counter=0, strip_count =0, position=0, enable = 0, reset, finished_flag=1, trigger_value;
	double start_time, end_time, time_ten, time_eleven;
	
	printf("Executing store_data function...\n");

	while (run) {
// Obtain current GPIO values from FPGA
	//	imaging = *idleImaging  & 0x0000001;
	//	idle = *idleImaging & 0x0000010;
		trigger_value = *trigger_gpio;
    //   written = *written_out;

	//	printf("imaging %d Idle: %d\n", imaging, idle);

// Check for Imaging state
	//	if (imaging) {
// To avoid creating multiple strips for one Imaging -> Ready transition
            		if(created_flag == 1){
                		strip_count++;
                		fprintf(report, "Strip Count: %d\n ", strip_count);
				gettimeofday(&ts1, NULL);
				start_time = (double)ts1.tv_sec + (ts1.tv_usec * 1.0 / 1000000.0);
				fflush(report);
            		}
			created_flag = 0;
			printf("written %d trigger value: %d, finished_flag: %d\n", written, trigger_value, finished_flag);

// To not conflict writing twice for the same trigger pulse
       //     if (written) {
			  if(trigger_value){
					if(finished_flag){
                        
                        finished_count = finished_count+1;

            printf("Clearing the destination register block...\n");
                        memset(virtual_dst_addr, 0, buflen*4);

            printf("Reset the DMA.\n");
                        write_dma(dma_virtual_addr, S2MM_CONTROL_REGISTER, RESET_DMA);
                        dma_s2mm_status(dma_virtual_addr);

            printf("Halt the DMA.\n");
                        write_dma(dma_virtual_addr, S2MM_CONTROL_REGISTER, HALT_DMA);
                        dma_s2mm_status(dma_virtual_addr);

            printf("Enable all interrupts.\n");
                        write_dma(dma_virtual_addr, S2MM_CONTROL_REGISTER, ENABLE_ALL_IRQ);
                        dma_s2mm_status(dma_virtual_addr);

            printf("Writing the destination address for the data from S2MM in DDR...\n");
                        write_dma(dma_virtual_addr, S2MM_DST_ADDRESS_REGISTER, 0x0f000000);
                        dma_s2mm_status(dma_virtual_addr);

            printf("Run the S2MM channel.\n");
                        write_dma(dma_virtual_addr, S2MM_CONTROL_REGISTER, RUN_DMA);
                        dma_s2mm_status(dma_virtual_addr);

                        
            printf("Writing S2MM transfer length of 32 bytes...\n");
                        write_dma(dma_virtual_addr, S2MM_BUFF_LENGTH_REGISTER, buflen*4);
                        dma_s2mm_status(dma_virtual_addr);


            printf("Waiting for S2MM sychronization...\n");
                        dma_s2mm_sync(dma_virtual_addr);
                        dma_s2mm_status(dma_virtual_addr);

                        // fwrite(virtual_dst_addr, sizeof(int32_t), buflen, fptr);
						for (int i = 0; i < buflen; i++) {
  							  fprintf(fptr, "%d\n", ((int32_t*)virtual_dst_addr)[i]);
							}

                        finished_flag = 0;

                        *((uint32_t*)(reset_gpio + 0)) = 0;
                        usleep(1);
                        *((uint32_t*)(reset_gpio + 0)) = 1;

            // Get time
                        gettimeofday(&ts, NULL);
                        fprintf(timelog, "%lf\n", (double)ts.tv_sec + (ts.tv_usec * 1.0 / 1000000.0));
                        fflush(timelog);

                        if (!written) {
                                fprintf(errlog, "%d: ERROR: Could not write data to file\n", (int)ts.tv_sec);
                                fflush(errlog);
                            }

                        if (finished_count % 60000 == 0)	{
                            fclose(fptr);
                            gettimeofday(&ts, NULL);
                            timer = (int)ts.tv_sec;
                            sprintf(filename, "%s/daq_R%d_EC%d_T%d.bin", dirname, num_rp, num_ec, timer);
                            if ((fptr = fopen(filename, "ab+")) == NULL) {
                                printf(" ERROR: Could not open file to store");
                                fprintf(errlog, "%d - ERROR: Could not open file to store %s\n", timer, filename);
                                fflush(errlog);
                
                                return 0;
                            }
                        }
}
		 }
		  	 else{
                     if(!trigger_value){
        	        	finished_flag = 1;
		 	     }
             }
		}
	
	
}

// Log the current PID to a file. 
// Used by rp_monitord. 
int write_pid(const char* filename) {
	char pid_str[8];
    FILE *fpid;

	if ((fpid = fopen(filename, "w")) == NULL){
       fprintf(stderr, "Couldn't open PID file to write");
		return -1;
	}

	snprintf(pid_str, 8, "%d\n", getpid());
	printf("DAQ PID is %s\n", pid_str);

    size_t bytes_written = fwrite(pid_str, sizeof(char), strlen(pid_str), fpid);
	fflush(fpid);
	fclose(fpid);
	return bytes_written > 0? 1: 0;
}

int main(int argc, char** argv)
{

		signal(SIGINT, signal_handler);
		signal(SIGTERM, signal_handler);

		// Write current PID to a file. 
		if (!write_pid("/tmp/daq.pid")) 
			fprintf(stderr, "PID logging failed");
		
		int ack_rboot, ack_init, ack_fcreate;
		uint32_t buflen, decim;
		//global_errlog = fopen("/home/cstuff/error_logger.txt", "a");
		decim = atoi(argv[1]);
		buflen = atoi(argv[2]);

		printf("The decimation is set to %d and Buffer Length to %d\n", decim, buflen);

		ack_init = initialize(decim,buflen);

		if (!ack_init) {
			gettimeofday(&ts, NULL);
			timer = (int)ts.tv_sec;
		//	fprintf(global_errlog, "%d - Initialization error\n", timer);
			exit(1);
		}

		ack_rboot = read_boot_count();
		if (!ack_rboot) {
			gettimeofday(&ts, NULL);
			timer = (int)ts.tv_sec;
		//	fprintf(global_errlog, "%d - Boot counter function failed\n", timer);
			exit(1);
		}

		ack_fcreate = fcreate();
		if (!ack_fcreate) {
		//	fprintf(global_errlog, "%d - File creation function failed\n", timer);
			exit(1);
		}

		store_data(decim, buflen);
}

void signal_handler(int signum){
	if(signum == SIGINT){
		printf("\nCounter: %d\n", finished_count+1);
		run = 0;
	}

	if(signum == SIGTERM){
		printf("\nCounter: %d\n", finished_count+1);
		run = 0;
	}
}