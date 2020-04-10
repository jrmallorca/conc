#ifndef __DINING_H
#define __DINING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

#include "libc.h"

#define PHILOSOPHERS 16 // Number of threads

// State of resource
typedef enum {
  DIRTY, // Indicates resource used
  CLEAN, // Indicates resource unused
} c_state;

// State of philosopher
typedef enum {
  THINKING,
  HUNGRY,
  EATING,
} p_state;

// Resource
typedef struct {
  int id;
  c_state state;
} chopstick;

typedef struct {
  bool state;
  int chopstick_id;
} request;

#endif
