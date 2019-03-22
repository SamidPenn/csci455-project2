#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "test.h"
#include "kfc.h"

static int parent_first = -1;

static void *
thread2_main(void *arg)
{
	CHECKPOINT(parent_first ? 4 : 2);

	DPRINTF("hit Checkpoint 2 in thread_main2\n");
	kfc_yield();

	DPRINTF("about to hit Checkpoint 5 in thread_main2\n");
	CHECKPOINT(parent_first ? 7 : 5);

	DPRINTF("hit Checkpoint 5 in thread_main2\n");
	return NULL;
}

static void *
thread_main(void *arg)
{
	if (parent_first < 0)
		parent_first = 0;

	CHECKPOINT(1);

	DPRINTF("hit Checkpoint 1 in thread_main\n");
	THREAD(thread2_main);

	CHECKPOINT(parent_first ? 2 : 4);
	
	DPRINTF("hit Checkpoint 2 in thread_main\n");
	kfc_yield();

	CHECKPOINT(parent_first ? 5 : 7);
	kfc_yield();

	CHECKPOINT(parent_first ? 8 : 9);
	kfc_yield();

	return NULL;
}

int
main(void)
{
	INIT(1, 0);

	CHECKPOINT(0);
	DPRINTF("hit Checkpoint 0 in main\n");
	THREAD(thread_main);
	if (parent_first < 0) {
		parent_first = 1;
		kfc_yield();
	}

	CHECKPOINT(3);
	DPRINTF("hit Checkpoint 3 in main- about to yield\n");
	
	kfc_yield();

	CHECKPOINT(6);
	kfc_yield();

	CHECKPOINT(parent_first ? 9 : 8);
	kfc_yield();

	CHECKPOINT(10);
	kfc_yield();

	VERIFY(11);
	return 0;
}
