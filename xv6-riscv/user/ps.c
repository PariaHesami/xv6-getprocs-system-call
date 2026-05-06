#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/procinfo.h"
#include "user.h"

#define MAXPROCS 64   // should match NPROC in kernel/param.h

// Convert numeric kernel procstate to a human-readable string.
// The names are chosen to match the project specification.
char*
state_to_string(int s)
{
  // In xv6-riscv, enum procstate is:
  // UNUSED = 0, USED = 1, SLEEPING = 2, RUNNABLE = 3, RUNNING = 4, ZOMBIE = 5.
  // The project expects "EMBRYO" for state 1, so we map it that way here.
  switch (s) {
  case 0: return "UNUSED";
  case 1: return "EMBRYO";
  case 2: return "SLEEPING";
  case 3: return "RUNNABLE";
  case 4: return "RUNNING";
  case 5: return "ZOMBIE";
  default: return "UNKNOWN";
  }
}

// Return number of decimal digits in a non-negative integer (for column width).
static int
num_digits(int x)
{
  int d = 1;
  while (x >= 10) {
    x /= 10;
    d++;
  }
  return d;
}

// Print an integer left-aligned in a field of given width.
static void
print_int_field(int value, int width)
{
  int d = num_digits(value);
  printf("%d", value);
  for (int i = d; i < width; i++)
    printf(" ");
}

// Print a string left-aligned in a field of given width.
static void
print_str_field(char *s, int width)
{
  int len = strlen(s);
  printf("%s", s);
  for (int i = len; i < width; i++)
    printf(" ");
}

int
main(int argc, char *argv[])
{
  struct procinfo infos[MAXPROCS];
  int n, i;

  // Fetch process information from the kernel via getprocs().
  n = getprocs(infos, MAXPROCS);
  if (n < 0) {
    printf("ps: getprocs failed\n");
    exit(1);
  }

  // Fixed-width header to match the required format.
  printf("PID    PPID   STATE      SIZE       NAME\n");

  for (i = 0; i < n; i++) {
    if (infos[i].state == 0)    // skip UNUSED entries
      continue;

    // Columns: PID (6), PPID (7), STATE (10), SIZE (10), NAME (rest of line).
    print_int_field(infos[i].pid, 6);
    print_int_field(infos[i].ppid, 7);
    print_str_field(state_to_string(infos[i].state), 10);
    print_int_field(infos[i].sz, 10);
    printf("%s\n", infos[i].name);
  }

  exit(0);
}
