#ifndef __DINING_H
#define __DINING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

#include "libc.h"

#define PHILOSOPHERS 16 // Number of threads

// Resource
typedef struct {
  int owner_id;
  bool dirty;
  int mutex;
} chopstick;

#endif
