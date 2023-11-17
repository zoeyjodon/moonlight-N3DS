/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "pair_record.h"

extern ssize_t getline(char **buf, size_t *bufsiz, FILE *fp);

void add_pair_address(char* address) {
  // Prevent duplicates
  char* address_list[MAX_PAIRED_DEVICES];
  int address_count = 0;
  list_paired_addresses(address_list, &address_count);

  bool exists = false;
  for (int i = 0; i < address_count; i++) {
    if (strcmp(address_list[i], address) == 0) {
      exists = true;
    }
    free(address_list[i]);
  }
  if (exists) {
    return;
  }

  char* address_file = (char*) MOONLIGHT_3DS_PATH "/paired";
  FILE* fd = fopen(address_file, "a");
  if (fd == NULL) {
    fprintf(stderr, "Can't open pair file: %s\n", address_file);
    return;
  }
  fprintf(fd, "%s\n", address);
  fclose(fd);
}

void remove_pair_address(char* address) {
  char* address_list[MAX_PAIRED_DEVICES];
  int address_count = 0;
  list_paired_addresses(address_list, &address_count);

  char* address_file = (char*) MOONLIGHT_3DS_PATH "/paired";
  remove(address_file);

  FILE* fd = fopen(address_file, "w");
  for (int i = 0; i < address_count; i++) {
    if (strcmp(address_list[i], address) != 0) {
      fprintf(fd, "%s\n", address);
    }
    free(address_list[i]);
  }
  fclose(fd);
}

void list_paired_addresses(char** address_list, int* address_count) {
  char* address_file = (char*) MOONLIGHT_3DS_PATH "/paired";
  FILE* fd = fopen(address_file, "r");
  if (fd == NULL) {
    fprintf(stderr, "Can't open pair file: %s\n", address_file);
    return;
  }

  int idx = 0;
  char *line = NULL;
  size_t len = 0;
  while (getline(&line, &len, fd) != -1) {
    if (strlen(line) < 2) {
      continue;
    }
    address_list[idx] = malloc(strlen(line));
    strcpy(address_list[idx], line);
    address_list[idx][strlen(line) - 1] = '\0';
    idx++;
  }
  *address_count = idx;

  fclose(fd);
}
