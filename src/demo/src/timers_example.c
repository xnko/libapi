/*
 * Demonstration of sleep, idle and timer emulation
 */

#include <stdio.h>

#include "../../api/include/api.h"

void timer(api_loop_t* loop, void* arg)
{
	int timeout;
	
	srand((unsigned int)api_time_current());

	while (1)
	{
		timeout = rand() % 5000;
		api_loop_sleep(loop, timeout);
		printf("%s elapsed %d ms\r\n", arg, timeout);
	}
}

void idle(api_loop_t* loop, void* arg)
{
	api_loop_post(loop, timer, "timer one", 0);
	api_loop_post(loop, timer, "timer two", 0);

	while (1)
	{
		api_loop_idle(loop, 2000);
		printf("loop was in idle for 2 second\r\n");
	}
}

int main(int argc, char *argv[])
{
	api_init();

	if (API_OK != api_loop_run(idle, 0, 0))
	{
		return 1;
	}

	return 0;
}