#include "dining.h"

/* To solve the Dining Philosophers problem, we will implement the
   Chandy-Misra solution where:

   1.
   2.
   3.
   4.
*/

extern uint32_t shm; // Address to shared memory
uint32_t offset;     // Size of shared memory

// Allocate n-byte shared memory region
void shm_open( uint32_t size ) {
  offset = size;
  memset( &shm - size, 0, size );
}

// Return pointer to shared memory
void* mmap() {
  return &shm - offset;
}

void main_dining() {
  // Allocate memory for array of chopsticks to share
  shm_open( sizeof( chopstick ) * PHILOSOPHERS );

  // Create 16 philosophers
  for( int i = 0; i < PHILOSOPHERS; i++ ) {
    if( 0 == fork() ) {
      // Assign ID
      int id = i;
      char p[2];
      itoa( p, id );

      // Assign pointer to shared resource
      chopstick* chopsticks = mmap();

      // Forever cycle between thinking, hungry and eating.
      while(1) {
        if( chopsticks[id].state == CLEAN ) chopsticks[id].state = DIRTY;
        else chopsticks[id].state = CLEAN;

        write( STDOUT_FILENO, "Philosopher ", 12);
        write( STDOUT_FILENO, p, 2);
        write( STDOUT_FILENO, " is thinking\n", 13 );

        yield();
      }
    }
  }

  exit( EXIT_SUCCESS );
}