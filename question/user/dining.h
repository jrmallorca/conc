#ifndef __dining_H
#define __dining_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>
#include <time.h>

#include "libc.h"

#define PHILOSOPHERS 16 // Number of threads
#define SHM_SIZE 0x00001000

// State of resource
typedef enum {
  DIRTY, // Indicates resource used
  CLEAN  // Indicates resource unused
} chopstick_state;

// Resource
typedef struct {
  chopstick_state state;
} chopstick;

// Thread
typedef struct {
  chopstick l; // Left
  chopstick r; // Right
} philosopher;

#endif
