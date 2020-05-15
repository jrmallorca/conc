/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

pcb_t procTab[ MAX_PROCS ]; pcb_t* executing = NULL;

extern uint32_t tos_procs;
extern void main_console();

region regions[ MAX_SHM ] = { 0 };

// -------------------------------------------------------------------------------------------------------------------
// Getters

pcb_t* get_pcb ( pid_t pid ) {
  for( int i = 0; i < MAX_PROCS; i++ ) {
    if( procTab[i].pid == pid ) return &procTab[i];
  }
  return NULL;
}

int get_free_region_index() {
  for( int i = 0; i < MAX_SHM; i++ ) {
    if( regions[ i ].state == UNOCCUPIED ) return i;
  }

  return -1; // If no free PCB
} 

// Get the next free PCB in process table
int get_free_pcb_index() {
  for( int i = 0; i < MAX_PROCS; i++ ) {
    if( procTab[ i ].status == STATUS_INVALID || procTab[ i ].status == STATUS_TERMINATED ) return i;
  }

  return -1; // If no free PCB
}

// -------------------------------------------------------------------------------------------------------------------
// Scheduling

void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {
  char prev_pid = '?', next_pid = '?';

  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of P_{prev}
    prev_pid = '0' + prev->pid;
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
    next_pid = '0' + next->pid;
  }

  PL011_putc( UART0, '[',      true );
  PL011_putc( UART0, prev_pid, true );
  PL011_putc( UART0, '-',      true );
  PL011_putc( UART0, '>',      true );
  PL011_putc( UART0, next_pid, true );
  PL011_putc( UART0, ']',      true );
  PL011_putc( UART0, '\n',     true );

  executing = next;                             // update   executing process to P_{next}

  return;
}

// Using priority+age-based scheduling
void schedule( ctx_t* ctx ) {
  pcb_t* next;
  int priority;
  int max_priority = 0;

  // Find process with highest priority and assign it as next process
  for( int i = 0; i < MAX_PROCS; i++ ) {
    if( procTab[ i ].status != STATUS_INVALID && procTab[ i ].status != STATUS_TERMINATED ) {
      priority = procTab[i].b_priority + procTab[i].age; // base priority + age

      if( max_priority <= priority ) {
        next = &procTab[ i ];
        max_priority = priority;
      }
    }
  }

  // Increment age of other processes in the ready queue
  for( int i = 0; i < MAX_PROCS; i++ ) {
    if( procTab[ i ].status != STATUS_INVALID && procTab[ i ].status != STATUS_TERMINATED ) {
      if( next->pid != procTab[i].pid ) procTab[i].age++;
      else procTab[i].age = 0;
    }
  }

  // Switch context
  dispatch( ctx, executing, next );
  executing->status = STATUS_READY;
  next->status = STATUS_EXECUTING;
  return;
}

// -------------------------------------------------------------------------------------------------------------------
// Hilevel handlers

void hilevel_handler_rst( ctx_t* ctx ) {
  PL011_putc( UART0, '[', true );
  PL011_putc( UART0, 'R', true );
  PL011_putc( UART0, ']', true );

  /* Configure the mechanism for interrupt handling by
   *
   * - configuring timer st. it raises a (periodic) interrupt for each
   *   timer tick,
   * - configuring GIC st. the selected interrupts are forwarded to the
   *   processor via the IRQ interrupt signal, then
   * - enabling IRQ interrupts.
   */

  TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  /* Initialise the console. Note that:
   * - the CPSR value of 0x50 means the processor is switched into USR mode,
   *   with IRQ interrupts enabled, and
   * - the PC and SP values match the entry point and top of stack.
   */

  memset( &procTab[ 0 ], 0, sizeof( pcb_t ) ); // initialise 0-th PCB = console
  procTab[ 0 ].pid        = 0;
  procTab[ 0 ].status     = STATUS_CREATED;
  procTab[ 0 ].tos        = ( uint32_t )( &tos_procs );
  procTab[ 0 ].ctx.cpsr   = 0x50;
  procTab[ 0 ].ctx.pc     = ( uint32_t )( &main_console );
  procTab[ 0 ].ctx.sp     = procTab[ 0 ].tos;
  procTab[ 0 ].b_priority = 1;
  procTab[ 0 ].age        = 0;

  /* Invalidate all other entries in the process table, so it's clear they are not
   * representing valid (i.e., active) processes.
   */

  for( int i = 1; i < MAX_PROCS; i++ ) {
    procTab[ i ].status = STATUS_INVALID;
  }

  /* Once the PCBs are initialised, we select the 0-th PCB (console) to be
   * executed: there is no need to preserve the execution context, since it
   * is invalid on reset (i.e., no process was previously executing).
   */

  dispatch( ctx, NULL, &procTab[ 0 ] );

  int_enable_irq();

  return;
}

void hilevel_handler_irq( ctx_t* ctx ) {
  // Step 2: read  the interrupt identifier so we know the source.

  uint32_t id = GICC0->IAR;

  // Step 4: handle the interrupt, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
    PL011_putc( UART0, '[', true );
    PL011_putc( UART0, 'T', true );
    PL011_putc( UART0, ']', true );

    schedule( ctx );
    TIMER0->Timer1IntClr = 0x01;
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}

void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) {
  /* Based on the identifier (i.e., the immediate operand) extracted from the
   * svc instruction,
   *
   * - read the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call, then
   * - write any return value back to preserved usr mode registers.
   */

  switch( id ) {
    case 0x00 : { // 0x00 => yield()
      PL011_putc( UART0, '[', true );
      PL011_putc( UART0, 'Y', true );
      PL011_putc( UART0, ']', true );

      schedule( ctx );

      break;
    }
    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      // Print
      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      // Set return values
      ctx->gpr[ 0 ] = n;

      break;
    }
    case 0x03 : { // 0x03 -> fork()
      PL011_putc( UART0, '[', true );
      PL011_putc( UART0, 'F', true );
      PL011_putc( UART0, ']', true );

      // Get PCB
      int idx = get_free_pcb_index();
      if( idx == -1 ) { // If there's no free PCB left, return
        ctx->gpr[0] = -1;
        break;
      }
      pcb_t* child_pcb = &procTab[ idx ];

      // Copy context from parent PCB to child PCB
      memcpy( &child_pcb->ctx, ctx, sizeof( ctx_t ) );

      // Copy stack from parent PCB to child PCB
      // memcpy() works from the bottom up
      uint32_t parent_stack = executing->tos - PROC_SIZE;
      uint32_t child_stack  = child_pcb->tos - PROC_SIZE;
      memcpy( ( void* ) child_stack, ( void* ) parent_stack, PROC_SIZE );

      // Calculate offset for the sp of child PCB
      uint32_t offset = (uint32_t)( executing->tos - ctx->sp );

      // Create PCB and set the attributes
      child_pcb->pid        = idx;
      child_pcb->status     = STATUS_CREATED;
      child_pcb->tos        = ( uint32_t )( &tos_procs ) - (idx * PROC_SIZE);
      child_pcb->ctx.sp     = child_pcb->tos - offset;
      child_pcb->b_priority = 1;
      child_pcb->age        = 0;

      // Set return values
      ctx->gpr[0]           = child_pcb->pid; // Return value for parent
      child_pcb->ctx.gpr[0] = 0;              // Return value for child

      break;
    }
    case 0x04 : { // 0x04 => exit( status )
      PL011_putc( UART0, '[', true );
      PL011_putc( UART0, 'E', true );
      PL011_putc( UART0, 'X', true );
      PL011_putc( UART0, 'I', true );
      PL011_putc( UART0, 'T', true );
      PL011_putc( UART0, ']', true );

      // Reset contents of PCB, indicate termination and re-schedule
      memset( executing, 0, sizeof( pcb_t ) );
      executing->status = STATUS_TERMINATED;
      schedule( ctx );

      break;
    }
    case 0x05 : { // 0x05 => exec( addr )
      PL011_putc( UART0, '[', true );
      PL011_putc( UART0, 'E', true );
      PL011_putc( UART0, 'X', true );
      PL011_putc( UART0, 'E', true );
      PL011_putc( UART0, 'C', true );
      PL011_putc( UART0, ']', true );

      // Get entry point of process (E.g. &main_P3)
      uint32_t addr = ( uint32_t )( ctx->gpr[ 0 ] );

      // Set attributes
      ctx->pc = addr;
      ctx->sp = executing->tos;

      break;
    }
    case 0x06 : { // 0x06 => kill( pid, x )
      PL011_putc( UART0, '[', true );
      PL011_putc( UART0, 'K', true );
      PL011_putc( UART0, ']', true );

      pid_t pid = ( pid_t )( ctx->gpr[ 0 ] );

      // Get the PCB, reset it and indicate termination
      pcb_t* target = get_pcb( pid );
      if( target != NULL ) {
        memset( target, 0, sizeof( pcb_t ) );
        target->status = STATUS_TERMINATED;
      }

      break;
    }
    case 0x07 : { // 0x07 => nice( pid, x )
      PL011_putc( UART0, '[', true );
      PL011_putc( UART0, 'P', true );
      PL011_putc( UART0, ']', true );

      pid_t pid = ( pid_t )( ctx->gpr[ 0 ] );
      int     x = (int    )( ctx->gpr[ 1 ] );

      // Get the PCB and set base priority to x
      pcb_t* target = get_pcb( pid );
      if( target != NULL ) target->b_priority = x;
    }
    case 0x08 : { // 0x08 => shm_open( uint32_t size )
      uint32_t size = ( uint32_t )( ctx->gpr[ 0 ] );

      // Find unoccupied region
      int idx = get_free_region_index();
      if( idx == -1 ) { // If there's no free shm left, return
        ctx->gpr[0] = -1;
        break;
      }
      region* r = &regions[ idx ];
    
      // Set attributes
      r->fd     = idx;
      r->offset = ( idx != 0 ) ? ( regions[ idx - 1 ].offset - size ) : ( ( uint32_t ) ( &shm - size ) );
      r->size   = size;
      r->state  = OCCUPIED;
    
      // Set shared memory region
      memset( ( void* )( r->offset ), 0, size );
    
      // Return fd
      ctx->gpr[0] = idx;
      break;
    }
    case 0x09 : { // 0x09 => mmap( int fd )
      int fd = ( int )( ctx->gpr[ 0 ] );

      // Return a pointer to the shm region
      ctx->gpr[0] = regions[ fd ].offset;

      break;
    }
    case 0x0A : { // 0x0A => shm_unlink( int fd )
      int fd = ( int )( ctx->gpr[ 0 ] );

      // Reset contents of shm region
      memset( ( void* )( regions[ fd ].offset ), 0, regions[ fd ].size );

      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
