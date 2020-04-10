#include "dining.h"

/* To solve the Dining Philosophers problem, we will implement the Chandy-Misra solution where:
   
   1. Each chopstick is always in the possession of one of it's two philosophers.
      Also, a dirty chopstick is always cleaned just before it is given to its other philosopher.

   2. Initialization.
      Every process receives a unique integer ID number. For every pair of philosophers who contend
      for a chopstick, one chopstick is created, assigned to the philosopher with the lower ID number
      (lower neighbor), and marked as "dirty."

   3. Thinking.
      When a philosopher p is thinking, if that philosopher p receives a request for a particular
      chopstick c from one of that philosopher's neighbors, then p gives the neighbor that
      chopstick c (after cleaning it).

   4. Hungry.
      When a philosopher p is preparing to eat, p requests any chopsticks that p doesn't already
      have from the appropriate neighbor.

      During this time, if a neighbor asks p for a chopstick that p possesses, then p sends that
      chopstick (after cleaning it) if it's dirty, and keeps that chopstick for the present if it's
      clean. p defers the requests for already-clean chopsticks (i.e., remembers the clean requests
      for later delivery).

   5. Eating.
      A philosopher p may start eating as soon as p has all of p's chopsticks. While eating, all
      requests for chopsticks are deferred, and all chopsticks become dirty.

   6. Cleanup.
      Immediately after eating, a philosopher delivers any chopsticks for which there are deferred
      requests (after cleaning them). That philosopher then proceeds to eat.

*/

// Attributes of philosopher
int id;
char id_c[ 2 ]; // ID in char form
p_state state;  
chopstick* l;
chopstick* r;

chopstick* chopsticks;      // Chopsticks array
request (*neighbours)[ 2 ]; // Request array (2D)

// Initially give philosopher the correct amount of chopsticks depending on id
// Initially make each chopstick dirty
void set_up_table( int n_fd, int c_fd ) {
  // Convert ID into char[]
  itoa( id_c, id );

  // Assign pointers
  neighbours = ( request (*)[2] ) mmap( n_fd );
  chopsticks = mmap( c_fd );

  // Set up the table
  // 1st philosopher has 2 chopsticks, last philosopher has no chopsticks
  // Every other philosopher has 1 chopstick
  if( id < ( ( id - 1 ) % PHILOSOPHERS ) ) {
    l        = &chopsticks[ ( id - 1 ) % PHILOSOPHERS ];
    l->id    = ( id - 1 ) % PHILOSOPHERS;
    l->state = DIRTY;
  }
  else l = NULL;

  if( id < ( ( id + 1 ) % PHILOSOPHERS ) ) {
    r        = &chopsticks[ id ];
    r->id    = id;
    r->state = DIRTY;
  }
  else r = NULL;
}

void check_requests( request (*neighbours)[2] ) {
  if( neighbours[ id ][ 0 ].state == true ) { // Request from left neighbour
    if( state != HUNGRY || l->state != CLEAN ) {
      l->state = CLEAN;

      neighbours[ id ][ 0 ].state        = false;
      neighbours[ id ][ 0 ].chopstick_id = l->id;

      l = NULL;
    }
  }
  if( neighbours[ id ][ 1 ].state == true ) { // Request from right neighbour
    if( state != HUNGRY || r->state != CLEAN ) {
      r->state = CLEAN;

      neighbours[ id ][ 1 ].state        = false;
      neighbours[ id ][ 1 ].chopstick_id = r->id;

      r = NULL;
    }
  }
}

void requests( request (*neighbours)[2], chopstick* chopsticks ) {
  while( 1 ) {
    check_requests( neighbours );

    // Request neighbours
    if( l == NULL ) neighbours[ ( id - 1 ) % PHILOSOPHERS ][ 1 ].state = true; // Left neighbour
    if( r == NULL ) neighbours[ ( id + 1 ) % PHILOSOPHERS ][ 0 ].state = true; // Right neighbour

    if( l != NULL && r != NULL ) break;
    // else yield();

    // Check if philosopher can take chopsticks
    if( l == NULL )
      if( neighbours[ ( id - 1 ) % PHILOSOPHERS ][ 1 ].state == false )
        l = &chopsticks[ neighbours[ ( id - 1 ) % PHILOSOPHERS ][ 1 ].chopstick_id ];
    if( r == NULL )
      if( neighbours[ ( id + 1 ) % PHILOSOPHERS ][ 0 ].state == false )
        r = &chopsticks[ neighbours[ ( id + 1 ) % PHILOSOPHERS ][ 0 ].chopstick_id ];
  }
}

// Thread is idle (Can give away resources)
void thinking( request (*neighbours)[2] ) {
  write( STDOUT_FILENO, "Philosopher ", 12 );
  write( STDOUT_FILENO, id_c, 2 );
  write( STDOUT_FILENO, " is thinking\n", 13 );
  state = THINKING;

  int i = 0;
  while( i < id ) {
    check_requests( neighbours );
    i++;
  }
}

// Thread wanting to execute (Needs resources)
void hungry( request (*neighbours)[2], chopstick* chopsticks ) {
  write( STDOUT_FILENO, "Philosopher ", 12 );
  write( STDOUT_FILENO, id_c, 2 );
  write( STDOUT_FILENO, " is hungry\n", 10 );
  state = HUNGRY;

  requests( neighbours, chopsticks );
}

// Thread execute (Has resources)
// Thread defers request while eating
void eating() {
  write( STDOUT_FILENO, "Philosopher ", 12 );
  write( STDOUT_FILENO, id_c, 2 );
  write( STDOUT_FILENO, " is eating\n", 10 );
  state = EATING;

  l->state = DIRTY;
  r->state = DIRTY;
}

void main_dining() {
  int c_fd = shm_open( sizeof( chopstick ) *   PHILOSOPHERS       ); // SHM for chopsticks
  int n_fd = shm_open( sizeof( request   ) * ( PHILOSOPHERS * 2 ) ); // SHM for requests

  // Create 16 philosophers
  for( int i = 0; i < PHILOSOPHERS; i++ ) {
    if( 0 == fork() ) {
      id = i;

      set_up_table( n_fd, c_fd );

      // Forever cycle between thinking, hungry and eating.
      while( 1 ) {
        thinking( neighbours );
        hungry( neighbours, chopsticks );
        eating();
      }
    }
  }

  exit( EXIT_SUCCESS );
}