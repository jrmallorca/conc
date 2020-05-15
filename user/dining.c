#include "dining.h"

/* To solve the Dining Philosophers problem, we will implement the (slightly modified) Chandy-Misra
   solution where:
   
   1. Each chopstick is always in the possession of one of it's two philosophers.
      Also, a dirty chopstick is always cleaned just before it is given to its other philosopher.

   2. Each chopstick has a mutex. We can think of locked as "philosopher holds chopstick in hand" and
      unlocked as "chopstick is on the table, besides the owner's plate". If locked, a neighbour
      cannot take the chopstick and vice versa.

   3. Initialization.
      Every process receives a unique integer ID number. For every pair of philosophers who contend
      for a chopstick, one chopstick is created, assigned to the philosopher with the lower ID number
      (lower neighbor), and marked as "dirty."

   4. Thinking.
      When a philosopher p is thinking, p's neighbours are free to take p's chopsticks and clean it
      themselves.

   5. Hungry.
      When a philosopher p is preparing to eat, p requests for any chopsticks that p doesn't already
      have from the appropriate neighbor.

      During this time, if p's neighbour(s) are also hungry, they can check if p's chopsticks are
      dirty and unlocked. If the mentioned conditions are met, the neighbour can take the appropriate
      chopstick from p, clean it and lock it.

   6. Eating.
      A philosopher p may start eating as soon as p has 2 chopsticks, clean or dirty. Once they're
      finished eating, all chopsticks become dirty and free for the taking.
*/

// % is NOT a modulo operator
int mod( int x, int m ) {
  int r = x % m;
  return r < 0 ? ( r + m ) : r;
}

// While we don't own chopsticks, get chopsticks from others only if they're dirty
void request( int id, char id_c[2], chopstick* l, chopstick* r ) {
  // A philosopher must check if they're the owner of the chopstick (owner_id) and they've locked it (mutex == 0)
  while( id != l->owner_id || l->mutex == 1 || id != r->owner_id || r->mutex == 1 ) {
    if( id != l->owner_id && l->dirty ) { // If not owner and not locked in
      write( STDOUT_FILENO, "Philosopher ", 12 );
      write( STDOUT_FILENO, id_c, 2 );
      write( STDOUT_FILENO, " wants left\n", 12 );

      // Wait until chopstick available to take
      sem_wait( &l->mutex );

      // Clean chopstick and set owner to this philosopher
      l->dirty    = false;
      l->owner_id = id;

      write( STDOUT_FILENO, "Philosopher ", 12 );
      write( STDOUT_FILENO, id_c, 2 );
      write( STDOUT_FILENO, " gets left\n", 11 );
    }

    if( id != r->owner_id && r->dirty ) {
      write( STDOUT_FILENO, "Philosopher ", 12 );
      write( STDOUT_FILENO, id_c, 2 );
      write( STDOUT_FILENO, " wants right\n", 13 );
      
      // Wait until chopstick available to take
      sem_wait( &r->mutex );

      // Clean chopstick and set owner to this philosopher
      r->dirty    = false;
      r->owner_id = id;

      write( STDOUT_FILENO, "Philosopher ", 12 );
      write( STDOUT_FILENO, id_c, 2 );
      write( STDOUT_FILENO, " gets right\n", 12 );
    }
  }
}

// Thread is idle (Can give away resources)
void thinking( char id_c[2] ) {
  write( STDOUT_FILENO, "Philosopher ", 12 );
  write( STDOUT_FILENO, id_c, 2 );
  write( STDOUT_FILENO, " is thinking\n", 13 );

  // "Think"
  sleep( atoi( id_c ) + 1 );
}

// Thread wanting to execute (Needs resources)
void hungry( int id, char id_c[2], chopstick* l, chopstick* r ) {
  write( STDOUT_FILENO, "Philosopher ", 12 );
  write( STDOUT_FILENO, id_c, 2 );
  write( STDOUT_FILENO, " is hungry\n", 11 );

  // Request for chopsticks (resources) to eat
  request( id, id_c, l, r );
}

// Thread execute (Has resources)
// After eating, they're still the owner of the chopsticks (owner_id) but they can be given away (mutex == 1)
void eating( char id_c[2], chopstick* l, chopstick* r ) {
  write( STDOUT_FILENO, "Philosopher ", 12 );
  write( STDOUT_FILENO, id_c, 2 );
  write( STDOUT_FILENO, " is eating\n", 11 );

  // "Eat"
  sleep( atoi( id_c ) + 1 );

  // Set chopsticks as dirty and indicate to neighbours the chopsticks can be taken from this philosopher
  l->dirty = true;
  sem_post( &l->mutex );
  r->dirty = true;
  sem_post( &r->mutex );
}

void main_dining() {
  // Open a shm region for chopsticks
  int c_fd = shm_open( sizeof( chopstick ) * PHILOSOPHERS );

  // Create 16 philosophers
  for( int i = PHILOSOPHERS - 1; i >= 0; i-- ) {
    if( 0 == fork() ) {
      // Attributes of philosopher
      int id = i;
      char id_c[ 2 ]; // ID in char form
      itoa( id_c, id );
      chopstick* l;
      chopstick* r;

      chopstick* chopsticks = mmap( c_fd ); // Chopsticks array

      // Set up the table
      // 1st philosopher has 2 chopsticks, last philosopher has no chopsticks
      // Every other philosopher has 1 chopstick on their right
      int num;
      num = mod( id - 1, PHILOSOPHERS );
      l = &chopsticks[ id ];
      if( id < num ) {
        l->mutex    = 0;
        l->owner_id = id;
        l->dirty    = true;
      }

      num = mod( id + 1, PHILOSOPHERS );
      r = &chopsticks[ num ];
      if( id < num ) {
        r->mutex    = 0;
        r->owner_id = id;
        r->dirty    = true;
      }

      // Make sure every philosopher sets up their part of the table before dining
      yield();

      // Forever cycle between thinking, hungry and eating.
      while( 1 ) {
        thinking( id_c );
        hungry( id, id_c, l, r );
        eating( id_c, l, r );
      }
    }
  }

  exit( EXIT_SUCCESS );
}