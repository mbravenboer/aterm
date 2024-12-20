/*{{{  includes */


#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/times.h>

#include "_aterm.h"
#include "afun.h"
#include "memory.h"
#include "util.h"
#include "gc.h"
#include "debug.h"

/*}}}  */

/*{{{  global variables */

static ATerm *stackBot = NULL;

#define PRINT_GC_TIME           1
#define PRINT_GC_STATS          2

#define TO_OLD_RATIO   65
#define TO_YOUNG_RATIO 25

static int     flags               = 0;

int at_gc_count			   = 0;
static int     stack_depth[3]      = { 0, MYMAXINT, 0 };
static int     reclaim_perc[3]     = { 0, MYMAXINT, 0 };
extern int     mark_stats[3];
#ifdef WITH_STATS
static clock_t sweep_time[3]       = { 0, MYMAXINT, 0 };
static clock_t mark_time[3]        = { 0, MYMAXINT, 0 };
extern int     nr_marks;
#endif
static FILE *gc_f = NULL;

extern ATprotected_block protected_blocks;

AFun at_parked_symbol = -1;

int gc_min_number_of_blocks;
int max_freeblocklist_size;
int min_nb_minor_since_last_major;
int good_gc_ratio;
int small_allocation_rate_ratio;
int old_increase_rate_ratio;

ATbool at_mark_young;

/*{{{  local functions */

void major_sweep_phase_old();
void major_sweep_phase_young();
void minor_sweep_phase_young();
void check_unmarked_block(unsigned int blocks);

/*{{{  void AT_initGC(int argc, char *argv[], ATerm *bottomOfStack) */

void AT_initGC(int argc, char *argv[], ATerm *bottomOfStack)
{
  int i;

  stackBot = bottomOfStack;
  gc_f = stderr;

  for(i=1; i<argc; i++) {
    if(streq(argv[i], "-at-print-gc-time"))
      flags |= PRINT_GC_TIME;
    else if(streq(argv[i], "-at-print-gc-info"))
      flags |= (PRINT_GC_TIME | PRINT_GC_STATS);
    else if(strcmp(argv[i], "-at-help") == 0) {
      fprintf(stderr, "    %-20s: print non-intrusive gc information "
	      "after execution\n", "-at-print-gc-time");
      fprintf(stderr, "    %-20s: print elaborate gc information "
	      "after execution\n", "-at-print-gc-info");
    }
  }
}

/*}}}  */

/**
 * This function can be used to change the bottom of the stack.
 * Note that we only have one application that uses this fuction:
 * the Java ATerm wrapper interface, because here the garbage collector
 * can be called from different (but synchronized) threads, so at
 * the start of any operation that could start the garbage collector,
 * the bottomOfStack must be adjusted to point to the stack of
 * the calling thread.
 */
void AT_setBottomOfStack(ATerm *bottomOfStack)
{
  stackBot = bottomOfStack;
}

ATerm *stack_top()
{
    return __builtin_frame_address(0);
}

static void mark_memory(ATerm *start, ATerm *stop)
{
  char *ptr;
  ATerm *cur, real_term;

    /*fprintf(stderr,"---> mark_memory phase [%x,%x]\n",start,stop);*/
    /* Traverse the stack */
  for(ptr=(char*)start; ptr<(char*)stop; ptr+=ALIGNOF_VOID_P) {
    cur = (ATerm*)ptr;
    if(AT_isPotentialTerm(*cur)) {
      real_term = AT_isInsideValidTerm(*cur);
      if (real_term != NULL) {
        if(!IS_MARKED((real_term)->header)) {
          AT_markTerm(real_term);
            /*printf("mark_memory: cur = %x\ttop sym = %s\n",cur,ATgetName(ATgetAFun(real_term)));*/
            /*nb_cell_in_stack++;*/
        }
      }
    } else if (AT_isValidSymbol((Symbol)*cur)) {
        /*fprintf(stderr,"mark_memory: AT_markSymbol(%d)\n",(Symbol)*cur);*/
      AT_markSymbol((Symbol)*cur);
        /*nb_cell_in_stack++;*/
    }
  }
}

static void mark_memory_young(ATerm *start, ATerm *stop)
{
  char *ptr;
  ATerm *cur, real_term;

    /*fprintf(stderr,"---> mark_memory_young phase [%x,%x]\n",start,stop);*/
    /* Traverse the stack */
  for(ptr=(char*)start; ptr<(char*)stop; ptr+=ALIGNOF_VOID_P) {
    cur = (ATerm*)ptr;
    if(AT_isPotentialTerm(*cur)) {
      real_term = AT_isInsideValidTerm(*cur);
      if (real_term != NULL) {
        if(!IS_MARKED(real_term->header)) {
          AT_markTerm_young(real_term);
            /*printf("mark_memory: cur = %x\ttop sym = %s\n",cur,ATgetName(ATgetAFun(real_term)));*/
            /*nb_cell_in_stack++;*/
        }
      }
    } else if (AT_isValidSymbol((Symbol)*cur)) {
        /*fprintf(stderr,"mark_memory_young: AT_markSymbol_young(%d)\n",(Symbol)*cur);*/
      AT_markSymbol_young((Symbol)*cur);
        /*nb_cell_in_stack++;*/
    }
  }
}

void ATmarkTerm(ATerm t)
{
  ATmarkArray(&t,1);
}

void ATmarkArray(ATerm *start, int size)
{
  if ( at_mark_young == ATtrue )
  {
	  mark_memory_young(start,start+size);
  } else {
	  mark_memory(start,start+size);
  }
}

void mark_phase()
{
  unsigned int i,j;
  unsigned long stack_size;
  ATerm *stackTop;
  ATerm *start, *stop;
  ProtEntry *prot;
  ATprotected_block pblock;

  _unused(stack_size);
  jmp_buf env;

  /* Traverse possible register variables */
  setjmp(env);

  start = (ATerm *)((char *)env);
  stop  = ((ATerm *)(((char *)env) + sizeof(jmp_buf)));
  mark_memory(start, stop);

  stackTop = stack_top();

  start = MIN(stackTop, stackBot);
  stop  = MAX(stackTop, stackBot);

  stack_size = stop - start;
  STATS(stack_depth, stack_size);

  fprintf(stdout,"stackTop = %p\n", stackTop);
  fprintf(stdout,"stackBot = %p\n", stackBot);

  fprintf(stdout,"start = %p\n", start);
  fprintf(stdout,"stop = %p\n", stop);

  mark_memory(start, stop);

  /* Traverse protected terms */
  for(i=0; i<at_prot_table_size; i++) {
    ProtEntry *cur = at_prot_table[i];
    while(cur) {
      for(j=0; j<cur->size; j++) {
	if(cur->start[j])
	  AT_markTerm(cur->start[j]);
      }
      cur = cur->next;
    }
  }

  for (prot=at_prot_memory; prot != NULL; prot=prot->next) {
    mark_memory((ATerm *)prot->start, (ATerm *)(((void *)prot->start) + prot->size));
  }

  for (pblock=protected_blocks; pblock != NULL; pblock=pblock->next) {
    if (pblock->protsize>0)
      mark_memory(pblock->term, &pblock->term[pblock->protsize]);
  }

  at_mark_young = ATfalse;
  for (i=0; i<at_prot_functions_count; i++)
  {
    at_prot_functions[i]();
  }

  AT_markProtectedSymbols();

  /* Mark 'parked' symbol */
  if (AT_isValidSymbol(at_parked_symbol)) {
    AT_markSymbol(at_parked_symbol);
  }
}

void mark_phase_young()
{
  unsigned int i,j;
  unsigned long stack_size;
  ATerm *stackTop;
  ATerm *start, *stop;
  ProtEntry *prot;
  ATprotected_block pblock;

  _unused(stack_size);

  jmp_buf env;

  /* Traverse possible register variables */
  setjmp(env);

  start = (ATerm *)((char *)env);
  stop  = ((ATerm *)(((char *)env) + sizeof(jmp_buf)));
  mark_memory_young(start, stop);

  stackTop = stack_top();
  start = MIN(stackTop, stackBot);
  stop  = MAX(stackTop, stackBot);

  stack_size = stop - start;
  STATS(stack_depth, stack_size);

  mark_memory_young(start, stop);

  /* Traverse protected terms */
  for(i=0; i<at_prot_table_size; i++) {
    ProtEntry *cur = at_prot_table[i];
    while(cur) {
      for(j=0; j<cur->size; j++) {
	if(cur->start[j])
	   AT_markTerm_young(cur->start[j]);
      }
      cur = cur->next;
    }
  }

  for (prot=at_prot_memory; prot != NULL; prot=prot->next) {
    mark_memory_young((ATerm *)prot->start, (ATerm *)(((void *)prot->start) + prot->size));
  }

  for (pblock=protected_blocks; pblock != NULL; pblock=pblock->next) {
    if (pblock->protsize>0)
      mark_memory_young(pblock->term, &pblock->term[pblock->protsize]);
  }

  at_mark_young = ATtrue;
  for (i=0; i<at_prot_functions_count; i++)
  {
    at_prot_functions[i]();
  }

  AT_markProtectedSymbols_young();

   /* Mark 'parked' symbol */
  if (AT_isValidSymbol(at_parked_symbol)) {
      /*fprintf(stderr,"mark_phase_young: AT_markSymbol_young(%d)\n",at_parked_symbol);*/
     AT_markSymbol_young(at_parked_symbol);
  }
}

#ifdef NDEBUG
#define CHECK_UNMARKED_BLOCK(blocks)
#else
#define CHECK_UNMARKED_BLOCK(blocks) check_unmarked_block(blocks)
#endif

/*{{{  void sweep_phase()  */

void sweep_phase()
{
  int size;

  for(size=MIN_TERM_SIZE; size<AT_getMaxTermSize(); size++) {
    terminfo[size].at_freelist = NULL;
  }
  old_bytes_in_young_blocks_after_last_major = 0;
  old_bytes_in_old_blocks_after_last_major = 0;

  /* Warning: freelist[size] is empty*/
  /* Warning: do not sweep fresh promoted block*/
  major_sweep_phase_old();
  major_sweep_phase_young();
  CHECK_UNMARKED_BLOCK(AT_BLOCK);
  CHECK_UNMARKED_BLOCK(AT_OLD_BLOCK);
}

void AT_init_gc_parameters(ATbool low_memory)
{
  if(low_memory) {
    gc_min_number_of_blocks = 2;
    max_freeblocklist_size  = 30;
    min_nb_minor_since_last_major = 2;

    good_gc_ratio = 50;
    small_allocation_rate_ratio = 25;
    old_increase_rate_ratio = 50;

  } else {
      /* 20MB for 10 sizes in average*/
    gc_min_number_of_blocks = 2*(20*1024*1024)/(10*sizeof(Block));
    max_freeblocklist_size  = 100;
    min_nb_minor_since_last_major = 10;
    good_gc_ratio = 50;
    small_allocation_rate_ratio = 75;
    old_increase_rate_ratio = 50;

#ifdef GC_VERBOSE
    fprintf(stderr,"gc_min_number_of_blocks = %d\n",gc_min_number_of_blocks);
#endif
  }
}

static void reclaim_empty_block(unsigned int blocks, int size, Block *removed_block, Block *prev_block)
{
  TermInfo* ti = &terminfo[size];

  ti->nb_reclaimed_blocks_during_last_gc++;

    /*
     * Step 1:
     *
     * remove cells from terminfo[size].at_freelist
     * remove the block from terminfo[size].at_blocks[AT_BLOCK]
     *
     */

#ifdef GC_VERBOSE
  fprintf(stdout,"block %x is empty\n",(unsigned int)removed_block);
#endif
  ti->at_nrblocks--;
  removed_block->size = 0;
  if(prev_block == NULL) {
      /*fprintf(stderr,"restore_block: remove first\n");*/
    ti->at_blocks[blocks] = removed_block->next_by_size;
    if(blocks==AT_BLOCK && ti->at_blocks[AT_BLOCK]) {
      ti->top_at_blocks = ti->at_blocks[AT_BLOCK]->end;
    }
  } else {
      /*fprintf(stderr,"restore_block: remove middle\n");*/
    prev_block->next_by_size = removed_block->next_by_size;
  }

  /*
   * Step 2:
   *
   * put the block into at_freeblocklist
   *
   */
  removed_block->next_by_size = at_freeblocklist;
  at_freeblocklist = removed_block;
  at_freeblocklist_size++;

  /*
   * Step 3:
   *
   * remove the block from block_table
   * free the memory
   *
   */
  if(at_freeblocklist_size > max_freeblocklist_size) {
    int idx, next_idx;
    Block *cur;
    Block *prev = NULL;

    assert(removed_block != NULL);

    idx = ADDR_TO_BLOCK_IDX(removed_block);
    next_idx = (idx+1)%BLOCK_TABLE_SIZE;
    for(cur=block_table[idx].first_after; cur ; prev=cur, cur=cur->next_after) {
      if(removed_block == cur) {
        break;
      }
    }
    if(!cur) {
      ATabort("### block %d not found\n",removed_block);
    }

    if(prev==NULL) {
      block_table[idx].first_after       = removed_block->next_after;
      block_table[next_idx].first_before = removed_block->next_after;
    } else {
      prev->next_after  = removed_block->next_after;
      prev->next_before = removed_block->next_before;
    }

    at_freeblocklist_size--;
    at_freeblocklist = at_freeblocklist->next_by_size;
#ifdef GC_VERBOSE
    fprintf(stderr,"free block %d\n",(int)removed_block);
#endif
    AT_free(removed_block);
  }
}

static void promote_block_to_old(int size, Block *block, Block *prev_block)
{
  TermInfo* ti = &terminfo[size];

#ifdef GC_VERBOSE
  printf("move block %x to old_blocks\n",(unsigned int)block);
#endif
  assert(block!=NULL);
  if(prev_block == NULL) {
    ti->at_blocks[AT_BLOCK] = block->next_by_size;
    if(ti->at_blocks[AT_BLOCK]) {
      ti->top_at_blocks = ti->at_blocks[AT_BLOCK]->end;
    }

  } else {
    prev_block->next_by_size = block->next_by_size;
  }
  block->next_by_size = ti->at_blocks[AT_OLD_BLOCK];
  ti->at_blocks[AT_OLD_BLOCK] = block;
}

static void promote_block_to_young(int size, Block *block, Block *prev_block)
{
  TermInfo* ti = &terminfo[size];

#ifdef GC_VERBOSE
  printf("move block %x to young_blocks\n",(unsigned int)block);
#endif
  assert(block!=NULL);
  if(prev_block == NULL) {
    ti->at_blocks[AT_OLD_BLOCK] = block->next_by_size;
  } else {
    prev_block->next_by_size = block->next_by_size;
  }
  if(ti->at_blocks[AT_BLOCK]) {
    block->next_by_size = ti->at_blocks[AT_BLOCK]->next_by_size;
    ti->at_blocks[AT_BLOCK]->next_by_size = block;
  } else {
    block->next_by_size = NULL;
    ti->at_blocks[AT_BLOCK] = block;
    ti->top_at_blocks = block->end;
    assert(ti->at_blocks[AT_BLOCK] != NULL);
  }
}

void check_unmarked_block(unsigned int blocks)
{
  int size;

  for(size=MIN_TERM_SIZE; size<AT_getMaxTermSize(); size++) {
    Block *block = terminfo[size].at_blocks[blocks];
    header_type *end = NULL;

    if(blocks == AT_BLOCK) {
      end = terminfo[size].top_at_blocks;
    } else {
      if(block) {
        end = block->end;
      }
    }

    while(block) {
      header_type *cur;
      for(cur=block->data ; cur<end ; cur+=size) {
	ATerm t = (ATerm)cur;

        if(IS_MARKED(t->header)) {
#ifdef GC_VERBOSE
          fprintf(stderr,"block = %p\tdata = %p\tblock->end = %p\tend = %p\n",block,block->data,block->end,end);
          fprintf(stderr,"type = %d\n",GET_TYPE(t->header));
          fprintf(stderr,"t = %p\n",t);
#endif
        }

        if(blocks==AT_OLD_BLOCK) {
          assert(GET_TYPE(t->header)==AT_FREE || IS_OLD(t->header));
        }

        assert(!IS_MARKED(t->header));
      }
      block = block->next_by_size;
      if(block) {
        end = block->end;
      }
    }
  }
}

void major_sweep_phase_old()
{
  int size, perc;
  int reclaiming = 0;
  int alive = 0;

  for(size=MIN_TERM_SIZE; size<AT_getMaxTermSize(); size++) {
    Block *prev_block = NULL;
    Block *next_block;

    Block *block = terminfo[size].at_blocks[AT_OLD_BLOCK];

    while(block) {
      /* set empty = 0 to avoid recycling*/
      int empty = 1;
      int alive_in_block = 0;
      int dead_in_block  = 0;
      int free_in_block  = 0;
      int capacity = ((block->end)-(block->data))/size;
      header_type *cur;

      assert(block->size == size);

      for(cur=block->data ; cur<block->end ; cur+=size) {
          /* TODO: Optimisation*/
	ATerm t = (ATerm)cur;
	if(IS_MARKED(t->header)) {
	  CLR_MARK(t->header);
          alive_in_block++;
          empty = 0;
          assert(IS_OLD(t->header));
	} else {
	  switch(ATgetType(t)) {
              case AT_FREE:
                assert(IS_YOUNG(t->header));
                free_in_block++;
                break;
              case AT_INT:
              case AT_REAL:
              case AT_APPL:
              case AT_LIST:
              case AT_PLACEHOLDER:
              case AT_BLOB:
                assert(IS_OLD(t->header));
                AT_freeTerm(size, t);
                t->header=FREE_HEADER;
                dead_in_block++;
                break;
              case AT_SYMBOL:
                assert(IS_OLD(t->header));
                AT_freeSymbol((SymEntry)t);
                t->header=FREE_HEADER;
                dead_in_block++;
                break;
              default:
                ATabort("panic in sweep phase\n");
	  }
	}
      }
      assert(alive_in_block + dead_in_block + free_in_block == capacity);

      next_block = block->next_by_size;

#ifndef NDEBUG
      if(empty) {
        for(cur=block->data; cur<block->end; cur+=size) {
          assert(ATgetType((ATerm)cur) == AT_FREE);
        }
      }
#endif

      if(empty) {
          /* DO NOT RESTORE THE FREE LIST: free cells have not been inserted*/
          /* terminfo[size].at_freelist = old_freelist;*/
        assert(terminfo[size].top_at_blocks < block->data || terminfo[size].top_at_blocks > block->end);
#ifdef GC_VERBOSE
        fprintf(stderr,"MAJOR OLD: reclaim empty block %p\n",block);
#endif
        reclaim_empty_block(AT_OLD_BLOCK, size, block, prev_block);
      } else if(0 && 100*alive_in_block/capacity <= TO_YOUNG_RATIO) {
        promote_block_to_young(size, block, prev_block);
        old_bytes_in_young_blocks_after_last_major += (alive_in_block*SIZE_TO_BYTES(size));
      } else {
        old_bytes_in_old_blocks_after_last_major += (alive_in_block*SIZE_TO_BYTES(size));

        /* DO NOT FORGET THIS LINE*/
        /* update the previous block*/
        prev_block = block;
      }

      block = next_block;
      alive += alive_in_block;
      reclaiming += dead_in_block;
    }
  }
  if(alive) {
    perc = (100*reclaiming)/alive;
    STATS(reclaim_perc, perc);
  }
}

void major_sweep_phase_young()
{
  int perc;
  int reclaiming = 0;
  int alive = 0;
  int size;

  old_bytes_in_young_blocks_since_last_major = 0;

  for(size=MIN_TERM_SIZE; size<AT_getMaxTermSize(); size++) {
    Block *prev_block = NULL;
    Block *next_block;
    ATerm old_freelist;
    TermInfo* ti = &terminfo[size];

    Block *block      = ti->at_blocks[AT_BLOCK];
    header_type *end  = ti->top_at_blocks;

    while(block) {
      int empty = 1;
      int alive_in_block = 0;
      int dead_in_block  = 0;
      int free_in_block  = 0;
      int old_in_block   = 0;
      int young_in_block = 0;
      int capacity = (end-(block->data))/size;
      header_type *cur;

      assert(block->size == size);

      old_freelist = ti->at_freelist;
      for(cur=block->data ; cur<end ; cur+=size) {
	ATerm t = (ATerm)cur;
	if(IS_MARKED(t->header)) {
	  CLR_MARK(t->header);
          alive_in_block++;
          empty = 0;
          if(IS_OLD(t->header)) {
            old_in_block++;
          } else {
            young_in_block++;
            INCREMENT_AGE(t->header);
          }
	} else {
	  switch(ATgetType(t)) {
              case AT_FREE:
                t->aterm.next = ti->at_freelist;
                ti->at_freelist = t;
                free_in_block++;
                break;
              case AT_INT:
              case AT_REAL:
              case AT_APPL:
              case AT_LIST:
              case AT_PLACEHOLDER:
              case AT_BLOB:
                AT_freeTerm(size, t);
                t->header = FREE_HEADER;
                t->aterm.next  = ti->at_freelist;
                ti->at_freelist = t;
                dead_in_block++;
                break;
              case AT_SYMBOL:
                AT_freeSymbol((SymEntry)t);
                t->header = FREE_HEADER;
                t->aterm.next = ti->at_freelist;
                ti->at_freelist = t;

                dead_in_block++;
                break;
              default:
                ATabort("panic in sweep phase\n");
	  }
	}
      }
      assert(alive_in_block + dead_in_block + free_in_block == capacity);

      next_block = block->next_by_size;

#ifndef NDEBUG
      if(empty) {
        for(cur=block->data; cur<end; cur+=size) {
          assert(ATgetType((ATerm)cur) == AT_FREE);
        }
      }
#endif

#ifdef GC_VERBOSE
        /*fprintf(stderr,"old_cell_in_young_block ratio = %d\n",100*old_in_block/capacity);*/
#endif

      if(end==block->end && empty) {
#ifdef GC_VERBOSE
        fprintf(stderr,"MAJOR YOUNG: reclaim empty block %p\n",block);
#endif
        ti->at_freelist = old_freelist;
	reclaim_empty_block(AT_BLOCK, size, block, prev_block);
      } else if(end==block->end && 100*old_in_block/capacity >= TO_OLD_RATIO) {
        if(young_in_block == 0) {
#ifdef GC_VERBOSE
          fprintf(stderr,"MAJOR YOUNG: promote block %p to old\n",block);
#endif
          ti->at_freelist = old_freelist;
          promote_block_to_old(size, block, prev_block);
          old_bytes_in_old_blocks_after_last_major += (old_in_block*SIZE_TO_BYTES(size));
        } else {
#ifdef GC_VERBOSE
          fprintf(stderr,"MAJOR YOUNG: freeze block %p\n",block);
#endif
          SET_FROZEN(block);
          old_bytes_in_young_blocks_after_last_major += (old_in_block*SIZE_TO_BYTES(size));
          ti->at_freelist = old_freelist;
          prev_block = block;
        }
      } else {
        old_bytes_in_young_blocks_after_last_major += (old_in_block*SIZE_TO_BYTES(size));
        prev_block = block;
      }

      block = next_block;
      if(block) {
        end = block->end;
      }

      alive += alive_in_block;
      reclaiming += dead_in_block;
    }

#ifndef NDEBUG
    if(ti->at_freelist) {
      ATerm data;
      for(data = ti->at_freelist ; data ; data=data->aterm.next) {
        assert(EQUAL_HEADER(data->header,FREE_HEADER));
        assert(ATgetType(data) == AT_FREE);
      }
    }
#endif

  }
  if(alive) {
    perc = (100*reclaiming)/alive;
    STATS(reclaim_perc, perc);
  }
}

void minor_sweep_phase_young()
{
  int size, perc;
  int reclaiming = 0;
  int alive = 0;

  old_bytes_in_young_blocks_since_last_major = 0;

  for(size=MIN_TERM_SIZE; size<AT_getMaxTermSize(); size++) {
    Block *prev_block = NULL;
    Block *next_block;
    ATerm old_freelist;
    TermInfo* ti = &terminfo[size];

    Block *block = ti->at_blocks[AT_BLOCK];
    header_type *end = ti->top_at_blocks;

      /* empty the freelist*/
    ti->at_freelist = NULL;

    while(block) {
        /* set empty = 0 to avoid recycling*/
      int empty = 1;
      int alive_in_block = 0;
      int dead_in_block  = 0;
      int free_in_block  = 0;
      int old_in_block  = 0;
      int capacity = (end-(block->data))/size;
      header_type *cur;

      assert(block->size == size);

      old_freelist = ti->at_freelist;
      for(cur=block->data ; cur<end ; cur+=size) {
	ATerm t = (ATerm)cur;
	if(IS_MARKED(t->header) || IS_OLD(t->header)) {
          if(IS_OLD(t->header)) {
            old_in_block++;
          }else{
          	INCREMENT_AGE(t->header);
          }
          CLR_MARK(t->header);
          alive_in_block++;
          empty = 0;
          assert(!IS_MARKED(t->header));
	} else {
	  switch(ATgetType(t)) {
              case AT_FREE:
                /* ti->at_freelist is not empty: so DO NOT ADD t*/
                t->aterm.next = ti->at_freelist;
                ti->at_freelist = t;
                free_in_block++;
                break;
              case AT_INT:
              case AT_REAL:
              case AT_APPL:
              case AT_LIST:
              case AT_PLACEHOLDER:
              case AT_BLOB:
                AT_freeTerm(size, t);
                t->header = FREE_HEADER;
                t->aterm.next   = ti->at_freelist;
                ti->at_freelist = t;

                dead_in_block++;
                break;
              case AT_SYMBOL:
                AT_freeSymbol((SymEntry)t);
                t->header = FREE_HEADER;
                t->aterm.next   = ti->at_freelist;
                ti->at_freelist = t;
                dead_in_block++;
                break;

              default:
                ATabort("panic in sweep phase\n");
	  }
          assert(!IS_MARKED(t->header));
	}
      }

      assert(alive_in_block + dead_in_block + free_in_block == capacity);
      next_block    = block->next_by_size;

#ifndef NDEBUG
      if(empty) {
        for(cur=block->data; cur<end; cur+=size) {
          assert(ATgetType((ATerm)cur) == AT_FREE);
        }
      }
#endif

      /* Do not reclaim frozen blocks */
      if(IS_FROZEN(block)) {
        ti->at_freelist = old_freelist;
      }

       /* TODO: create freeList Old*/
      if(0 && empty) {
        ti->at_freelist = old_freelist;
        reclaim_empty_block(AT_BLOCK, size, block, prev_block);
      } else if(0 && 100*old_in_block/capacity >= TO_OLD_RATIO) {
        promote_block_to_old(size, block, prev_block);
      } else {
        old_bytes_in_young_blocks_since_last_major += (old_in_block*SIZE_TO_BYTES(size));
        prev_block = block;
      }

      block = next_block;
      if(block) {
        end = block->end;
      }
      alive += alive_in_block;
      reclaiming += dead_in_block;
    }

#ifndef NDEBUG
    if(ti->at_freelist) {
      ATerm data;
      /*fprintf(stderr,"minor_sweep_phase_young: ensure empty freelist[%d]\n",size);*/
      for(data = ti->at_freelist ; data ; data=data->aterm.next) {
        if(!EQUAL_HEADER(data->header,FREE_HEADER)) {
          fprintf(stderr,"data = %p header = %x\n",data,(unsigned int) data->header);
        }
        assert(EQUAL_HEADER(data->header,FREE_HEADER));
        assert(ATgetType(data) == AT_FREE);
      }
    }
#endif

  }
  if(alive) {
    perc = (100*reclaiming)/alive;
    STATS(reclaim_perc, perc);
  }
}

void AT_collect()
{
  struct tms start, mark, sweep;
  clock_t user;
  FILE *file = gc_f;
  int size;

  /* snapshot*/
  for(size=MIN_TERM_SIZE; size<AT_getMaxTermSize(); size++) {
    TermInfo* ti = &terminfo[size];
    ti->nb_live_blocks_before_last_gc = ti->at_nrblocks;
    ti->nb_reclaimed_blocks_during_last_gc=0;
    ti->nb_reclaimed_cells_during_last_gc=0;
  }

  at_gc_count++;
  if (!silent)
  {
    fprintf(file, "collecting garbage..(%d)",at_gc_count);
    fflush(file);
  }
  times(&start);
  CHECK_UNMARKED_BLOCK(AT_BLOCK);
  CHECK_UNMARKED_BLOCK(AT_OLD_BLOCK);
  mark_phase();
  times(&mark);
  user = mark.tms_utime - start.tms_utime;
  STATS(mark_time, user);

  sweep_phase();

  times(&sweep);
  user = sweep.tms_utime - mark.tms_utime;
  STATS(sweep_time, user);

  if (!silent) {
    fprintf(file, "..\n");
  }
}

void AT_collect_minor()
{
  struct tms start, mark, sweep;
  clock_t user;
  FILE *file = gc_f;
  int size;

  _unused(user); // stats only

    /* snapshop */
  for(size=MIN_TERM_SIZE; size<AT_getMaxTermSize(); size++) {
    TermInfo* ti = &terminfo[size];
    ti->nb_live_blocks_before_last_gc = ti->at_nrblocks;
    ti->nb_reclaimed_blocks_during_last_gc=0;
    ti->nb_reclaimed_cells_during_last_gc=0;
  }

  at_gc_count++;
  if (!silent)
  {
    fprintf(file, "young collecting garbage..(%d)",at_gc_count);
    fflush(file);
  }
  times(&start);
  CHECK_UNMARKED_BLOCK(AT_BLOCK);
  CHECK_UNMARKED_BLOCK(AT_OLD_BLOCK);
    /*nb_cell_in_stack=0;*/
  mark_phase_young();
    /*fprintf(stderr,"AT_collect_young: nb_cell_in_stack = %d\n",nb_cell_in_stack++);*/
  times(&mark);
  user = mark.tms_utime - start.tms_utime;
  STATS(mark_time, user);

  minor_sweep_phase_young();
  CHECK_UNMARKED_BLOCK(AT_BLOCK);
  CHECK_UNMARKED_BLOCK(AT_OLD_BLOCK);

  times(&sweep);
  user = sweep.tms_utime - mark.tms_utime;
  STATS(sweep_time, user);

  if (!silent)
    fprintf(file, "..\n");
}

#define CLOCK_DIVISOR CLOCKS_PER_SEC

void AT_cleanupGC()
{
  FILE *file = gc_f;
  if(flags & PRINT_GC_TIME) {
    fprintf(file, "%d garbage collects,\n", at_gc_count);
#ifdef WITH_STATS
    fprintf(file, "(all statistics are printed min/avg/max)\n");
    if(at_gc_count > 0) {
      if(nr_marks > 0) {
	fprintf(file, "  mark stack needed: %d/%d/%d (%d marks)\n",
		mark_stats[IDX_MIN],
                mark_stats[IDX_TOTAL]/nr_marks,
		mark_stats[IDX_MAX], nr_marks);
      }
      fprintf(file, "  marking  took %.2f/%.2f/%.2f seconds, total: %.2f\n",
	      ((double)mark_time[IDX_MIN])/(double)CLOCK_DIVISOR,
	      (((double)mark_time[IDX_TOTAL])/(double)at_gc_count)/(double)CLOCK_DIVISOR,
	      ((double)mark_time[IDX_MAX])/(double)CLOCK_DIVISOR,
	      ((double)mark_time[IDX_TOTAL])/(double)CLOCK_DIVISOR);
      fprintf(file, "  sweeping took %.2f/%.2f/%.2f seconds, total: %.2f\n",
	      ((double)sweep_time[IDX_MIN])/(double)CLOCK_DIVISOR,
	      (((double)sweep_time[IDX_TOTAL])/(double)at_gc_count)/(double)CLOCK_DIVISOR,
	      ((double)sweep_time[IDX_MAX])/(double)CLOCK_DIVISOR,
	      ((double)sweep_time[IDX_TOTAL])/(double)CLOCK_DIVISOR);
    }
#endif
  }

  if(flags & PRINT_GC_STATS) {
    if(at_gc_count > 0) {
      fprintf(file, "\n  stack depth: %d/%d/%d words\n",
	      stack_depth[IDX_MIN],
	      stack_depth[IDX_TOTAL]/at_gc_count,
	      stack_depth[IDX_MAX]);
      fprintf(file, "\n  reclamation percentage: %d/%d/%d\n",
	      reclaim_perc[IDX_MIN],
	      reclaim_perc[IDX_TOTAL]/at_gc_count,
	      reclaim_perc[IDX_MAX]);
    }
  }
}
