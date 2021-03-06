#include <assert.h>
#include <sys/types.h>
#include <ucontext.h>
#include <stdlib.h>


#include "queue.h"


#include "kfc.h"

static int inited = 0;


struct tcb
{
	int used;
	tid_t tid;
	ucontext_t tcont;
	int hasret;
	void *this_ret;
	int waiting;
	tid_t waitingfor;
};

ucontext_t jump;

struct tcb TCBs[KFC_MAX_THREADS];

struct tcb *current; 
//struct tcb *waiting; 

queue_t fcfs;


/**
 * Initializes the kfc library.  Programs are required to call this function
 * before they may use anything else in the library's public interface.
 *
 * @param kthreads    Number of kernel threads (pthreads) to allocate
 * @param quantum_us  Preemption timeslice in microseconds, or 0 for cooperative
 *                    scheduling
 *
 * @return 0 if successful, nonzero on failure
 */
int
kfc_init(int kthreads, int quantum_us)
{
	assert(!inited);
	TCBs[0].tid=0;	
	TCBs[0].used=1;	
	current = &TCBs[0];
 	queue_init(&fcfs);
	getcontext(&jump);

	jump.uc_stack.ss_sp=malloc(KFC_DEF_STACK_SIZE);
	jump.uc_stack.ss_size=KFC_DEF_STACK_SIZE;

	makecontext(&jump, kfc_context_switch, 0);
	
	inited = 1;
	return 0;
}

/**
 * Cleans up any resources which were allocated by kfc_init.  You may assume
 * that this function is called only from the main thread, that any other
 * threads have terminated and been joined, and that threading will not be
 * needed again.  (In other words, just clean up and don't worry about the
 * consequences.)
 *
 * I won't test this function, since it wasn't part of the original assignment;
 * it is provided as a convenience to you if you are using Valgrind to test
 * (which I heartily encourage).
 */
void
kfc_teardown(void)
{
	assert(inited);

	inited = 0;
}

/**
 * Creates a new user thread which executes the provided function concurrently.
 * It is left up to the implementation to decide whether the calling thread
 * continues to execute or the new thread takes over immediately.
 *
 * @param ptid[out]   Pointer to a tid_t variable in which to store the new
 *                    thread's ID
 * @param start_func  Thread main function
 * @param arg         Argument to be passed to the thread main function
 * @param stack_base  Location of the thread's stack if already allocated, or
 *                    NULL if requesting that the library allocate it
 *                    dynamically
 * @param stack_size  Size (in bytes) of the thread's stack, or 0 to use the
 *                    default thread stack size KFC_DEF_STACK_SIZE
 *
 * @return 0 if successful, nonzero on failure
 */
int
kfc_create(tid_t *ptid, void *(*start_func)(void *), void *arg,
		caddr_t stack_base, size_t stack_size)
{
	assert(inited);
	ucontext_t contextID;
	getcontext(&contextID);
	//struct tcb *oldthread;
	

	if(stack_base == NULL){
		if(stack_size==0){ 
			stack_base = malloc(KFC_DEF_STACK_SIZE);
			stack_size = KFC_DEF_STACK_SIZE;
		}
		else
			stack_base=malloc(stack_size);
	}
	

	tid_t i;
	for(i=0; i < KFC_MAX_THREADS; i++){
		if(TCBs[i].used==0){ 
			TCBs[i].tid = i;
			TCBs[i].used = 1;
			TCBs[i].hasret=0;
			TCBs[i].waiting=0;
			break;
		}
	}
	*ptid = i;
	contextID.uc_stack.ss_sp=stack_base;
	contextID.uc_stack.ss_size=stack_size;
	
	contextID.uc_link= &jump;
	//queue_enqueue(&fcfs, current);
	TCBs[i].tcont = contextID;
	
	//oldthread=current;
	
//	current = &TCBs[i];
	
	makecontext(&TCBs[i].tcont, (void (*)())kfc_context_controller,
										2, (void (*)())start_func, arg);	
	queue_enqueue(&fcfs, &TCBs[i]);	
	DPRINTF("Create:enqueing new thread: %d\n", i);
	
//	swapcontext(&oldthread->tcont, &current->tcont);

	
	return 0;
}

void
kfc_context_switch()
{
	assert(inited);
	current = queue_dequeue(&fcfs);
	DPRINTF("Thread just got dequeued: %d\n", current->tid);
	setcontext(&current->tcont);


}


int
kfc_context_controller(void *(*start_func)(void *), void *arg)
{
	kfc_exit(start_func(arg));	

	return 0;

}

void
kfc_exit(void *ret)
{
	assert(inited);
//	TCBs[current->tid].used = 0;
	current->hasret=1;
	current->this_ret=ret;

	DPRINTF("Current thread exiting: %d\n ", current->tid);
	for(int i =0; i < KFC_MAX_THREADS; i++){
		
		if(current->tid==TCBs[i].waitingfor && TCBs[i].waiting==1){
			
			DPRINTF("Current thread that is waiting for me: %d\n ", i);
			current = &TCBs[i];

		//	kfc_join(current->waitingfor, ret);	
				
			queue_enqueue(&fcfs, current);
			
			DPRINTF("Exit: enqueing waiting thread: %d\n", i);
			break;
		}


	}
//	current = queue_dequeue(&fcfs);
	setcontext(&jump);

}

int
kfc_join(tid_t tid, void **pret)
{
	assert(inited);
	DPRINTF("join has been called, waiting on: %d\n", tid);
	if(TCBs[tid].hasret==0){
		DPRINTF("Join tid was not ready: %d\n", tid);
		//DPRINTF("The current return value: %d\n", TCBs[tid].this_ret);
		DPRINTF("-----------------\n");
		current->waiting=1;
		current->waitingfor = tid;
		//current = queue_dequeue(&fcfs);
		//swapcontext(&current->tcont);
		swapcontext(&current->tcont, &jump);
	}
		
	DPRINTF("the return value: %p\n", TCBs[tid].this_ret);
	*pret=TCBs[tid].this_ret;
	TCBs[tid].used=0;
	current->waiting=0;
//	queue_enqueue(&fcfs, current);
	return 0;
}

/**
 * Returns a small integer which identifies the calling thread.
 *
 * @return Thread ID of the currently executing thread
 */
tid_t
kfc_self(void)
{	
	return current->tid;
}

/**
 * Causes the calling thread to yield the processor voluntarily.  This may often
 * result in another thread being scheduled, but it does not preclude the
 * possibility of the same caller continuing if re-chosen by the scheduler.
 */
void
kfc_yield(void)
{
	assert(inited);
//	struct tcb *oldthread;
//	oldthread = current;
	queue_enqueue(&fcfs, current);
	DPRINTF("enqueing thread in yield tid: %d\n",current->tid);

//	current= queue_dequeue(&fcfs);
	swapcontext(&current->tcont, &jump);

}

int
kfc_sem_init(kfc_sem_t *sem, int value)
{
	assert(inited);
	return 0;
}

int
kfc_sem_post(kfc_sem_t *sem)
{
	assert(inited);
	return 0;
}

int
kfc_sem_wait(kfc_sem_t *sem)
{
	assert(inited);
	return 0;
}

void
kfc_sem_destroy(kfc_sem_t *sem)
{
	assert(inited);
}
