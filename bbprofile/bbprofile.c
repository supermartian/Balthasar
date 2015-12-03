/*
 * bbprofile.c
 * Copyright (C) 2015 Yuzhong Wen <wyz2014@vt.edu>
 *
 * Distributed under terms of the MIT license.
 *
 * A runtime for profiling the execution time of basicblocks,
 * Which also takes multi-thread into consideration.
 *
 * */

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include "bbprofile.h"

pid_t gettid()
{
	return syscall(SYS_gettid);
}

int shm_fd = -1;
char shm_path[] = "/bbprofile_shm";
struct prof_obj *prof_obj_table;

void bbprof_start(int bbid)
{
	if (shm_fd == -1) {
		shm_fd = shm_open(shm_path, O_RDWR, S_IRUSR | S_IWUSR);
		ftruncate(shm_fd, SHM_SIZE * sizeof(struct prof_obj));
		prof_obj_table = (struct prof_obj *) mmap(NULL, SHM_SIZE * sizeof(struct prof_obj), PROT_READ|PROT_WRITE,
				MAP_SHARED, shm_fd, 0);

		if (prof_obj_table == -1) {
			// SHM failed, exit
			exit(0);
		}
	}

	// We use tid and bbid combined together to identify the
	// executed basic block
	// (We might be able to analysis MAY-HAPPEN-IN-PARALLEL here?)
	pid_t pid = gettid();

	// Write the thing to the internal hash table
	struct prof_obj *obj = &prof_obj_table[bbid << 7 | ((uint64_t)pid & ((~0) << 7))];
	obj->start = perf_counter();
}

void bbprof_end(int bbid)
{
	pid_t pid = gettid();

	// Write the thing to the internal hash table
	struct prof_obj *obj = &prof_obj_table[bbid << 7 | ((uint64_t)pid & ((~0) << 7))];
	obj->total += perf_counter() - obj->start;
	obj->count ++;
}
