#include "event_ctl.h"
#include "popd.h"
#include <signal.h>

struct pop_gl_config pop_config;
struct pop_gl_config *popd = &pop_config;

/*epoll master*/
extern struct event_master *master;
   
int main(int argc, char **argv)
{
	struct event_node fetch;
	/*create master of event*/
	master = event_master_create(POP_EXPECTED_SIZE, MAXEVENTS);
	signal(SIGPIPE, SIG_IGN);
	pop_init();
	while(event_waiting_process(master, &fetch))
		event_call(&fetch);

	return 0;
}
