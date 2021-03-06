/*
 * bbprof_launch.c
 * Copyright (C) 2015 Yuzhong Wen <wyz2014@vt.edu>
 *
 * Distributed under terms of the MIT license.
 *
 * A launcher for profiling the execution time of basicblocks
 *
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "bbprofile.h"

#define OUTPUT_FILE "./output.txt"

int shm_fd = -1;
char shm_path[] = "/bbprofile_shm";
char bbinfo_file_path[] = "./bbinfo.dat";
struct prof_obj *prof_obj_table;
int exit_flag;

void signal_handler(int sig) {
	printf("OMG!\n");
	exit_flag = 1;
}

// Organize the thing in a better way
void bb_postprocessing(uint64_t bbnum) {
	int i;
	uint64_t bbcnt = 0;
	int output_fd = open(bbinfo_file_path, O_WRONLY|O_CREAT, 0644);
	struct bb_info *bb = malloc(bbnum * sizeof(struct bb_info));
	memset(bb, 0, bbnum * sizeof(struct bb_info));

	for (i = 0; i < SHM_SIZE; i++) {
		if (prof_obj_table[i].total != 0) {
			if (bb[prof_obj_table[i].bbid].total != 0) {
				bb[prof_obj_table[i].bbid].is_parallel ++;
			} else {
				bbcnt ++;
			}
			bb[prof_obj_table[i].bbid].total = prof_obj_table[i].total / prof_obj_table[i].count;
			printf("out to file %d %d\n", prof_obj_table[i].bbid, bb[prof_obj_table[i].bbid].total);
		}
	}

	write(output_fd, (void *) &bbnum, sizeof(uint64_t));
	write(output_fd, (void *) bb, bbnum * sizeof(struct bb_info));
	close(output_fd);

	//free(bb);
}

int main(int argc, char **argv) {
	char **argvx;
	pid_t c_pid, pid;
	uint64_t maxbb = 0;
	int status;
	int i;

	exit_flag = 0;

	// There we are going to write the profiling data into this shared memory space
	shm_fd = shm_open(shm_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	ftruncate(shm_fd, SHM_SIZE * sizeof(struct prof_obj));
	prof_obj_table = (struct prof_obj *) mmap(NULL, SHM_SIZE * sizeof(struct prof_obj), PROT_READ|PROT_WRITE,
			MAP_SHARED, shm_fd, 0);

	if (prof_obj_table == -1) {
		// SHM failed, exit
		exit(0);
	}

	memset(prof_obj_table, 0, SHM_SIZE * sizeof(struct prof_obj));

	// Everything is set, now we launch the profileee
	argvx = argv;
	argvx++;

	c_pid = fork();

	if (c_pid == 0) {
		printf("Launching %s\n", argv[1]);

		execvp(argv[1], argvx);
		perror("execve failed");
	} else if (c_pid > 0) {
		printf("Profiler is waiting...\n");
		signal(SIGINT, signal_handler);

		while (exit_flag == 0) {
			sleep(1);
		}

		printf("Profile run finished\n");

		for (i = 0; i < SHM_SIZE; i++) {
			if (prof_obj_table[i].total != 0) {
				if (maxbb < prof_obj_table[i].bbid) {
					maxbb = prof_obj_table[i].bbid;
				}

				printf("BB %d on thread %d, total %d\n", prof_obj_table[i].bbid, i & ~((~0)<<6), (prof_obj_table[i].total/prof_obj_table[i].count) >> 8);
			}
		}

		printf("max num of bb %d\n", maxbb);
		bb_postprocessing(maxbb);
	} else {
		printf("Excuse me %d?????\n", c_pid);
	}

	printf("Now you should have all the info inside the SHM, run the LLVM NOW!\n");
	return 0;
}
