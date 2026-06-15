/* Olal OS - alocador de memoria dinamica (heap) com lista de blocos livres.
   Arena em 8 MB. kmalloc/kfree com first-fit, split e coalescencia. */
#include "olal.h"

#define HEAP_BASE 0x00800000u
#define HEAP_SIZE (8u * 1024 * 1024)

typedef struct Block { u32 size; u32 used; struct Block *next; } Block;
static Block *head;

volatile u32 g_heap_used = 0;
volatile u32 g_heap_allocs = 0;

void heap_init(void){
    head = (Block*)HEAP_BASE;
    head->size = HEAP_SIZE - sizeof(Block);
    head->used = 0;
    head->next = 0;
    g_heap_used = 0;
    g_heap_allocs = 0;
}

void *kmalloc(u32 n){
    n = (n + 7) & ~7u;                       /* alinha em 8 bytes */
    for(Block *b = head; b; b = b->next){
        if(!b->used && b->size >= n){
            if(b->size >= n + sizeof(Block) + 8){   /* divide o bloco */
                Block *nb = (Block*)((u8*)b + sizeof(Block) + n);
                nb->size = b->size - n - sizeof(Block);
                nb->used = 0;
                nb->next = b->next;
                b->size = n;
                b->next = nb;
            }
            b->used = 1;
            g_heap_used += b->size + sizeof(Block);
            g_heap_allocs++;
            return (u8*)b + sizeof(Block);
        }
    }
    return 0;                                 /* sem memoria */
}

void kfree(void *p){
    if(!p) return;
    Block *b = (Block*)((u8*)p - sizeof(Block));
    b->used = 0;
    g_heap_used -= b->size + sizeof(Block);
    /* coalesce com o proximo se livre */
    for(Block *c = head; c; c = c->next){
        while(c->next && !c->used && !c->next->used){
            c->size += c->next->size + sizeof(Block);
            c->next = c->next->next;
        }
    }
}

u32 heap_total(void){ return HEAP_SIZE; }
