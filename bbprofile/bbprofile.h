/*
 * bbprofile.h
 * Copyright (C) 2015 wen <wen@wen-desktop>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef BBPROFILE_H
#define BBPROFILE_H

#define SHM_SIZE (65536*32)

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

__inline__ uint64_t perf_counter(void)
{
  uint32_t lo, hi;
  // take time stamp counter, rdtscp does serialize by itself, and is much cheaper than using CPUID
  __asm__ __volatile__ (
      "rdtscp" : "=a"(lo), "=d"(hi)
      );
  return ((uint64_t)lo) | (((uint64_t)hi) << 32);
}

struct prof_obj {
	uint64_t bbid;
	uint64_t start;
	uint64_t count;
	uint64_t total;
};

#endif /* !BBPROFILE_H */
