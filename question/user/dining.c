#include "dining.h"

/* To solve the Dining Philosophers problem, we will implement the
   Chandy-Misra solution where:

   1.
   2.
   3.
   4.
*/

// Use shm_open, mmap, etc. for shared resources (chopsticks)

int id;

void main_dining() {
  // Create chopsticks
  for( int i = 0; i < PHILOSOPHERS; i++ ) {
    chopsticks[ i ].state = DIRTY;
  }

  // Assign id for 1st philosopher
  id = 0;

  // Create 15 child philosophers
  for( int i = 1; i < PHILOSOPHERS; i++ ) {
    if( fork() == 0 ) {
      id = i;
    }
  }

  while(1) {
  }
}