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

#include "bbprofile.h"

#define OUTPUT_FILE "./output.txt"

int shm_fd = -1;
char shm_path[] = "/bbprofile_shm";
struct prof_obj *prof_obj_table;

int main(int argc, char **argv) {
	char **argvx;
	pid_t c_pid, pid;
	int status;
	int i;

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
		if ((pid = wait(&status)) < 0) {
			perror("wait");
			exit(1);
		}

		printf("Profile run finished\n");

		for (i = 0; i < SHM_SIZE; i++) {
			if (prof_obj_table[i].total != 0) {
				printf("BB %d on thread %d, total %d\n", prof_obj_table[i].bbid, i & ~((~0)<<5), prof_obj_table[i].total);
			}
		}
	}
}
