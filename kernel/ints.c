/* Olal OS - interrupcoes (IDT, PIC, timer PIT) e multitarefa preemptiva. */
#include "olal.h"

extern void idt_load(void *);
extern void irq0_stub(void);
extern void isr_default(void);
extern void irq_default(void);

struct IDTEntry { u16 off_lo, sel; u8 zero, type; u16 off_hi; } __attribute__((packed));
struct IDTPtr   { u16 limit; u32 base; } __attribute__((packed));
static struct IDTEntry idt[256];
static struct IDTPtr idtp;

volatile u32 g_ticks = 0;        /* tiques do timer (100/s) -> uptime */
volatile u32 g_switches = 0;     /* trocas de contexto */
volatile u32 g_work[3] = {0,0,0};/* contadores das tarefas de demonstracao */

#define NTASKS 4                 /* 0 = interface; 1..3 = tarefas extras */
typedef struct { u32 esp; u8 active; } Task;
static Task tasks[NTASKS];
static int current = 0;
static u8 task_stack[NTASKS][8192];

static void set_gate(int n, void (*h)(void)){
    u32 a = (u32)h;
    idt[n].off_lo = a & 0xFFFF; idt[n].sel = 0x08;
    idt[n].zero = 0; idt[n].type = 0x8E; idt[n].off_hi = (a >> 16) & 0xFFFF;
}

/* tratador do timer: salva a pilha atual, escolhe a proxima tarefa ativa,
   devolve a pilha dela (troca de contexto preemptiva). */
u32 timer_irq(u32 esp){
    g_ticks++;
    tasks[current].esp = esp;
    int n = 0;
    do { current = (current + 1) % NTASKS; n++; } while(!tasks[current].active && n < NTASKS);
    g_switches++;
    return tasks[current].esp;
}

/* cria uma tarefa: monta uma pilha que "volta" para entry via iret */
static void task_create(int slot, void (*entry)(void)){
    u32 *sp = (u32*)(task_stack[slot] + sizeof(task_stack[slot]));
    *(--sp) = 0x202;          /* eflags (IF=1) */
    *(--sp) = 0x08;           /* cs */
    *(--sp) = (u32)entry;     /* eip */
    for(int i = 0; i < 8; i++) *(--sp) = 0;   /* eax..edi (pushad) */
    tasks[slot].esp = (u32)sp;
    tasks[slot].active = 1;
}

/* tarefas de demonstracao: incrementam um contador e cedem a CPU (hlt) */
static void worker0(void){ for(;;){ g_work[0]++; __asm__ volatile("hlt"); } }
static void worker1(void){ for(;;){ g_work[1]++; __asm__ volatile("hlt"); } }
static void worker2(void){ for(;;){ g_work[2]++; __asm__ volatile("hlt"); } }

int  task_count(void){ int c=0; for(int i=0;i<NTASKS;i++) if(tasks[i].active) c++; return c; }
void task_spawn(void){
    void (*w[3])(void) = { worker0, worker1, worker2 };
    for(int i = 1; i < NTASKS; i++)
        if(!tasks[i].active){ task_create(i, w[i-1]); return; }
}
void task_kill_demos(void){ for(int i=1;i<NTASKS;i++){ tasks[i].active=0; g_work[i-1]=0; } }

static void pic_remap(void){
    outb(0x20,0x11); outb(0xA0,0x11);
    outb(0x21,0x20); outb(0xA1,0x28);    /* IRQs -> int 0x20.. / 0x28.. */
    outb(0x21,0x04); outb(0xA1,0x02);
    outb(0x21,0x01); outb(0xA1,0x01);
    outb(0x21,0xFE); outb(0xA1,0xFF);    /* habilita so o IRQ0 (timer) */
}
static void pit_init(int hz){
    int div = 1193182 / hz;
    outb(0x43,0x36);
    outb(0x40, div & 0xFF); outb(0x40,(div>>8)&0xFF);
}

void ints_init(void){
    for(int i = 0; i < 32; i++) set_gate(i, isr_default);
    for(int i = 32; i < 48; i++) set_gate(i, irq_default);
    set_gate(32, irq0_stub);
    idtp.limit = sizeof(idt) - 1; idtp.base = (u32)idt;
    idt_load(&idtp);

    tasks[0].active = 1;                  /* tarefa 0 = a interface (atual) */
    current = 0;

    pic_remap();
    pit_init(100);
    __asm__ volatile("sti");
}
