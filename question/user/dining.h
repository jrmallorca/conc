#ifndef __dining_H
#define __dining_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "libc.h"

#define PHILOSOPHERS 16 // Number of threads

// State of resource
typedef enum {
  CLEAN, // Indicates resource unused
  DIRTY  // Indicates resource used
} chopstick_state;

// Thread
typedef struct {
  chopstick l; // Left
  chopstick r; // Right
} philosopher;

// Resource
typedef struct {
  chopstick_state state;
} chopstick;

#endif
