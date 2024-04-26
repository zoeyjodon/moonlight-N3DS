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
#include <stdbool.h>
#include <string.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <locale>

#include "pair_record.hpp"

// trim from start (in place)
inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
inline void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

void add_pair_address(std::string address) {
  // Prevent duplicates
  auto address_list = list_paired_addresses();

  for (auto entry : address_list) {
    if (entry == address) {
      return;
    }
  }
  address_list.push_back(address);

  char* address_file = (char*) MOONLIGHT_3DS_PATH "/paired";
  remove(address_file);

  FILE* fd = fopen(address_file, "w");
  for (auto addr_string : address_list) {
    trim(address);
    fprintf(fd, "%s\n", address.c_str());
  }
  fclose(fd);
}

void remove_pair_address(std::string address) {
  auto address_list = list_paired_addresses();

  char* address_file = (char*) MOONLIGHT_3DS_PATH "/paired";
  remove(address_file);

  FILE* fd = fopen(address_file, "w");
  for (auto addr_string : address_list) {
    if (addr_string == address) {
      trim(addr_string);
      fprintf(fd, "%s\n", addr_string.c_str());
    }
  }
  fclose(fd);
}

std::vector<std::string> list_paired_addresses() {
  std::vector<std::string> addresses = std::vector<std::string>();
  std::ifstream pair_file(MOONLIGHT_3DS_PATH "/paired");
  std::string line;
  while (std::getline(pair_file, line))
  {
    trim(line);
    addresses.push_back(line);
  }
  return addresses;
}
