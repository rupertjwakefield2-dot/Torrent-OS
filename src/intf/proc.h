#pragma once
#include <stdint.h>

#define MAX_TASKS   16
#define KSTACK_SIZE 8192    /* 8 KB kernel stack per task */

typedef enum {
    TASK_UNUSED  = 0,
    TASK_READY   = 1,
    TASK_RUNNING = 2,
    TASK_BLOCKED = 3,
    TASK_DEAD    = 4,
} TaskState;

typedef struct Task {
    uint64_t    saved_rsp;    /* kernel stack pointer (saved on context switch) */
    uint32_t    pid;
    TaskState   state;
    const char *name;
    uint64_t    kstack_base;  /* base (lowest address) of kernel stack */
    uint64_t    ticks;        /* total timer ticks this task has run */
    int         priority;     /* 1 (low) – 10 (high) */
} Task;

/* Initialise scheduler.  Must be called before cpu_sti(). */
void     proc_init(void);

/* Spawn a new kernel thread.  entry() runs at ring 0.
 * Returns the new Task*, or NULL if the table is full. */
Task    *proc_spawn(const char *name, void (*entry)(void), int priority);

/* Called every timer tick (from IRQ0 handler).
 * cur_rsp = RSP pointing at the current InterruptFrame on the kernel stack.
 * Returns: new task RSP to switch to, or 0 if no switch needed. */
uint64_t proc_tick(uint64_t cur_rsp);

/* Mark the calling task as blocked (won't be scheduled until unblocked). */
void     proc_block(void);

/* Unblock a task so it can be scheduled again. */
void     proc_unblock(Task *t);

/* Voluntarily yield remaining timeslice. */
void     proc_yield(void);

/* Returns the currently running Task. */
Task    *proc_current(void);

/* Print the task table to the terminal. */
void     proc_list(void);

uint32_t proc_getpid(void);
