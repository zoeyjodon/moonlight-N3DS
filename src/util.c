/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2017 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef __3DS__
#include <3ds.h>
#endif

#ifdef HAVE_GETAUXVAL
#include <sys/auxv.h>

#ifndef HWCAP2_AES
#define HWCAP2_AES (1 << 0)
#endif
#endif

#if defined(__linux__) && defined(__riscv)
#if __has_include(<sys/hwprobe.h>)
#include <sys/hwprobe.h>
#else
#include <unistd.h>

#if __has_include(<asm/hwprobe.h>)
#include <asm/hwprobe.h>
#include <sys/syscall.h>
#else
#define __NR_riscv_hwprobe 258
struct riscv_hwprobe {
    int64_t key;
    uint64_t value;
};
#define RISCV_HWPROBE_KEY_IMA_EXT_0 4
#endif

// RISC-V Scalar AES [E]ncryption and [D]ecryption
#ifndef RISCV_HWPROBE_EXT_ZKND
#define RISCV_HWPROBE_EXT_ZKND (1 << 11)
#define RISCV_HWPROBE_EXT_ZKNE (1 << 12)
#endif

// RISC-V Vector AES
#ifndef RISCV_HWPROBE_EXT_ZVKNED
#define RISCV_HWPROBE_EXT_ZVKNED (1 << 21)
#endif

static int __riscv_hwprobe(struct riscv_hwprobe *pairs, size_t pair_count,
                           size_t cpu_count, unsigned long *cpus,
                           unsigned int flags)
{
    return syscall(__NR_riscv_hwprobe, pairs, pair_count, cpu_count, cpus, flags);
}

#endif
#endif

int write_bool(char *path, bool val) {
  int fd = open(path, O_RDWR);

  if(fd >= 0) {
    int ret = write(fd, val ? "1" : "0", 1);
    if (ret < 0)
      fprintf(stderr, "Failed to write %d to %s: %d\n", val ? 1 : 0, path, ret);

    close(fd);
    return 0;
  } else
    return -1;
}

int read_file(char *path, char* output, int output_len) {
  int fd = open(path, O_RDONLY);

  if(fd >= 0) {
    output_len = read(fd, output, output_len);
    close(fd);
    return output_len;
  } else
    return -1;
}

bool ensure_buf_size(void **buf, size_t *buf_size, size_t required_size) {
  if (*buf_size >= required_size)
    return false;

  *buf_size = required_size;
  *buf = realloc(*buf, *buf_size);
  if (!*buf) {
    fprintf(stderr, "Failed to allocate %zu bytes\n", *buf_size);
    abort();
  }

  return true;
}

bool has_fast_aes() {
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if defined(HAVE_GETAUXVAL) && (defined(__arm__) || defined(__aarch64__))
  #if defined(__arm__) && defined(HWCAP2_AES)
    return !!(getauxval(AT_HWCAP2) & HWCAP2_AES);
  #elif defined(__aarch64__)
    return !!(getauxval(AT_HWCAP) & HWCAP_AES);
  #else
    return false;
  #endif
#elif __has_builtin(__builtin_cpu_supports) && (defined(__i386__) || defined(__x86_64__))
  return __builtin_cpu_supports("aes");
#elif defined(__BUILTIN_CPU_SUPPORTS__) && defined(__powerpc__)
  return __builtin_cpu_supports("vcrypto");
#elif defined(__linux__) && defined(__riscv)
    struct riscv_hwprobe pairs[1] = {
        { RISCV_HWPROBE_KEY_IMA_EXT_0, 0 },
    };

    // If this syscall is not implemented, we'll get -ENOSYS
    // and the value field will remain zero.
    __riscv_hwprobe(pairs, sizeof(pairs) / sizeof(struct riscv_hwprobe), 0, NULL, 0);

    return (pairs[0].value & (RISCV_HWPROBE_EXT_ZKNE | RISCV_HWPROBE_EXT_ZKND)) ==
               (RISCV_HWPROBE_EXT_ZKNE | RISCV_HWPROBE_EXT_ZKND) ||
           (pairs[0].value & RISCV_HWPROBE_EXT_ZVKNED);
#elif __SIZEOF_SIZE_T__ == 4
  #warning Unknown 32-bit platform. Assuming AES is slow on this CPU.
  return false;
#else
  #warning Unknown 64-bit platform. Assuming AES is fast on this CPU.
  return true;
#endif
}

#ifdef __3DS__
bool ensure_linear_buf_size(void **buf, size_t *buf_size, size_t required_size) {
  if (*buf_size >= required_size)
    return false;

  linearFree(*buf);

  *buf_size = required_size;
  *buf = linearAlloc(*buf_size);
  if (!*buf) {
    fprintf(stderr, "Failed to allocate %zu bytes\n", *buf_size);
    abort();
  }

  return true;
}
#endif
