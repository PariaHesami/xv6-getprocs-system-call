#ifndef PROCINFO_H
#define PROCINFO_H

#include "types.h"

struct procinfo {
  int  pid;          // Process ID
  int  ppid;         // Parent process ID
  int  state;        // Numeric process state
  uint sz;           // Size of process memory (bytes)
  char name[16];     // Process name
};

#endif
