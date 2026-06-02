#include "proc.h"
#include "heap.h"
#include "print.h"
#include "str.h"
#include "io.h"

/*
 * Preemptive round-robin kernel-thread scheduler.
 *
 * Each task has its own 8 KB kernel stack.  On every timer tick the
 * current task's InterruptFrame* (= RSP at that moment) is saved into
 * task->saved_rsp.  proc_tick() then picks the next READY task and
 * returns its saved_rsp; isr_common switches RSP to that value and
 * iretq resumes the new task from exactly where it was interrupted.
 *
 * A brand-new task is given a fake InterruptFrame at the top of its
 * kernel stack so that the first "context switch into" it looks
 * identical to any other iretq return.
 */

/* Selectors from GDT (defined in boot/main.asm) */
#define KERN_CS    0x08
#define KERN_DS    0x10
#define RFLAGS_IF  0x202   /* IF=1, reserved bit 1 */

static Task     tasks[MAX_TASKS];
static int      cur   = 0;   /* index of currently running task */
static uint32_t npid  = 1;   /* next PID to assign */

/* ── helpers ── */

static Task *find_slot(void) {
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state == TASK_UNUSED) return &tasks[i];
    return (void*)0;
}

static int task_idx(const Task *t) {
    return (int)(t - tasks);
}

/* ── public API ── */

void proc_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) tasks[i].state = TASK_UNUSED;

    /* Task 0 = the current execution context (becomes the shell thread).
     * We don't need to set saved_rsp — it will be populated on the
     * first timer IRQ.                                                */
    tasks[0].pid        = npid++;
    tasks[0].state      = TASK_RUNNING;
    tasks[0].name       = "kernel";
    tasks[0].ticks      = 0;
    tasks[0].priority   = 5;
    tasks[0].kstack_base= 0;   /* uses the original boot stack */
    cur = 0;
}

Task *proc_spawn(const char *name, void (*entry)(void), int priority) {
    Task *t = find_slot();
    if (!t) return (void*)0;

    /* Allocate kernel stack */
    void *kstack = kmalloc(KSTACK_SIZE);
    if (!kstack) return (void*)0;
    t->kstack_base = (uint64_t)(uintptr_t)kstack;

    /*
     * Build a fake InterruptFrame at the top of the stack.
     * Layout (low → high addr, RSP will point to lowest entry = r15):
     *
     *  r15 r14 r13 r12 r11 r10 r9 r8 rbp rdi rsi rdx rcx rbx rax
     *  int_no  err_code
     *  rip  cs  rflags  rsp  ss
     *
     * We build from high to low, so we decrement sp before each store.
     */
    uint64_t *sp = (uint64_t*)((uint8_t*)kstack + KSTACK_SIZE);
    uint64_t  initial_rsp = (uint64_t)(uintptr_t)sp;

    /* hardware-pushed frame */
    *--sp = (uint64_t)KERN_DS;        /* ss        */
    *--sp = initial_rsp;              /* rsp       */
    *--sp = RFLAGS_IF;                /* rflags    */
    *--sp = (uint64_t)KERN_CS;        /* cs        */
    *--sp = (uint64_t)(uintptr_t)entry; /* rip     */

    /* isr stub fields */
    *--sp = 0;                         /* err_code */
    *--sp = 0;                         /* int_no   */

    /* GP registers — all zero for a fresh task */
    for (int i = 0; i < 15; i++) *--sp = 0; /* rax..r15 */

    t->saved_rsp  = (uint64_t)(uintptr_t)sp;
    t->pid        = npid++;
    t->state      = TASK_READY;
    t->name       = name;
    t->ticks      = 0;
    t->priority   = priority;

    return t;
}

uint64_t proc_tick(uint64_t cur_rsp) {
    /* save current task's stack pointer */
    tasks[cur].saved_rsp = cur_rsp;
    tasks[cur].ticks++;

    /* mark as ready (not running anymore) */
    if (tasks[cur].state == TASK_RUNNING)
        tasks[cur].state = TASK_READY;

    /* round-robin: find next READY task */
    int n = MAX_TASKS;
    int next = cur;
    for (int i = 1; i <= n; i++) {
        int c = (cur + i) % MAX_TASKS;
        if (tasks[c].state == TASK_READY) { next = c; break; }
    }

    if (next == cur) {
        /* no other task ready — stay on current */
        tasks[cur].state = TASK_RUNNING;
        return 0;   /* no context switch */
    }

    tasks[next].state = TASK_RUNNING;
    cur = next;
    return tasks[next].saved_rsp;   /* switch to new stack */
}

void proc_block(void) {
    tasks[cur].state = TASK_BLOCKED;
    /* The calling task will not be scheduled until proc_unblock() is called.
     * We yield immediately to let the scheduler pick another task.      */
    proc_yield();
}

void proc_unblock(Task *t) {
    if (t && t->state == TASK_BLOCKED) t->state = TASK_READY;
}

Task *proc_current(void) { return &tasks[cur]; }
uint32_t proc_getpid(void) { return tasks[cur].pid; }

void proc_yield(void) {
    /* Trigger a software interrupt to give up the CPU.
     * We reuse IRQ vector 32 (timer) semantics via a trick:
     * raise the scheduler by invoking our own timer handler path.     */
    __asm__ volatile("int $0x20" ::: "memory");
}

void proc_list(void) {
    Color fg = COLOR_LIGHT_GRAY, bg = COLOR_BLACK;
    print_set_color(COLOR_LIGHT_CYAN, bg);
    print_str("  PID  State    Ticks     Name\n");
    print_set_color(COLOR_DARK_GRAY, bg);
    print_str("  "); for(int i=0;i<44;i++) print_char('\xC4'); print_newline();
    static const char *sname[] = {"unused ","ready  ","running","blocked","dead   "};
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) continue;
        print_set_color(i == cur ? COLOR_LIGHT_GREEN : fg, bg);
        print_str("  ");
        char n[6]; str_fmtuint(n, sizeof(n), tasks[i].pid);
        for(int j=(int)str_len(n);j<4;j++) print_char(' ');
        print_str(n); print_str("  ");
        print_str(sname[tasks[i].state < 5 ? tasks[i].state : 0]);
        print_str("  ");
        char t[12]; str_fmtuint(t, sizeof(t), tasks[i].ticks);
        for(int j=(int)str_len(t);j<8;j++) print_char(' ');
        print_str(t); print_str("  ");
        print_str(tasks[i].name);
        print_newline();
    }
    print_set_color(fg, bg);
}
