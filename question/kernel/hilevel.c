/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

/* We assume there will be two user processes, stemming from execution of the 
 * two user programs P3 and P4, and can therefore
 * 
 * - allocate a fixed-size process table (of PCBs), and then maintain an index 
 *   into it to keep track of the currently executing process, and
 * - employ a fixed-case of round-robin scheduling: no more processes can be 
 *   created, and neither can be terminated, so assume both are always ready
 *   to execute.
 */

pcb_t procTab[ MAX_PROCS ]; pcb_t* executing = NULL;

// Get the next free PCB in process table
pcb_t* get_free_pcb() {
  for( int i = 0; i < MAX_PROCS; i++ ) {
    if( procTab[ i ].status == STATUS_INVALID ) return &procTab[ i ];
  }

  return NULL; // If no free PCB
}

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

  executing = next;                           // update   executing process to P_{next}

  return;
}

void schedule( ctx_t* ctx ) {
  pcb_t* next;
  int max_priority = 0;

  // Find process with highest priority and assign it as next process
  for(int i = 0; i < MAX_PROCS; i++) {
    if( procTab[ i ].status != STATUS_INVALID && procTab[ i ].status != STATUS_TERMINATED ) {
      procTab[ i ].priority = procTab[i].b_priority + procTab[i].age;

      if( max_priority <= procTab[ i ].priority ) {
        next = &procTab[ i ];
        max_priority = procTab[ i ].priority;
      }
    }
  }

  // Increase age of other processes in the ready queue
  for(int i = 0; i < MAX_PROCS; i++) {
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

extern uint32_t tos_procs;
extern void main_console();

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
  procTab[ 0 ].priority   = 1;

  /* Invalidate all other entries in the process table, so it's clear they are not
   * representing valid (i.e., active) processes.
   */

  for( int i = 1; i < MAX_PROCS; i++ ) {
    procTab[ i ].pid        = i;
    procTab[ i ].status     = STATUS_INVALID;
    procTab[ i ].tos        = ( uint32_t )( &tos_procs ) - (i * PROC_SIZE);
  }

  /* Once the PCBs are initialised, we arbitrarily select the 0-th PCB to be
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
   * - read  the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call, then
   * - write any return value back to preserved usr mode registers.
   */

  // #define SYS_YIELD     ( 0x00 )
  // #define SYS_WRITE     ( 0x01 )
  // #define SYS_READ      ( 0x02 )
  // #define SYS_FORK      ( 0x03 )
  // #define SYS_EXIT      ( 0x04 )
  // #define SYS_EXEC      ( 0x05 )
  // #define SYS_KILL      ( 0x06 )
  // #define SYS_NICE      ( 0x07 )

  switch( id ) {
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

      // Get a free PCB in the process table
      pcb_t* child_pcb = get_free_pcb();

      // If there's no free PCB left, return
      if( child_pcb == NULL ) break;

      // Copy context from parent PCB to child PCB
      memcpy( &child_pcb->ctx, ctx, sizeof(ctx_t));

      // Copy stack from parent PCB to child PCB
      // memcpy() works from the bottom up
      uint32_t parent_stack = executing->tos - PROC_SIZE;
      uint32_t child_stack  = child_pcb->tos - PROC_SIZE;
      memcpy( ( void* ) child_stack, ( void* ) parent_stack, PROC_SIZE );

      // Calculate offset for the sp of child PCB
      uint32_t offset = (uint32_t)( &executing->tos - ctx->sp );

      // Create PCB and set the attributes
      memset( child_pcb, 0, sizeof( pcb_t ) );
      child_pcb->status     = STATUS_CREATED;
      child_pcb->ctx.sp     = child_pcb->tos - offset;
      child_pcb->b_priority = 1;
      child_pcb->age        = 0;
      child_pcb->priority   = 1;

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

      // Indicate that the current process has been terminated
      executing->status = STATUS_TERMINATED;

      // Schedule another process
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

      // Entry point of process (E.g. &main_P3)
      uint32_t addr = ( uint32_t )( ctx->gpr[ 0 ] );

      // Set attributes
      ctx->pc = addr;
      ctx->sp = executing->tos;

      break;
    }
    case 0x06 : { // 0x06 => kill( pid, SIG_TERM )
      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
