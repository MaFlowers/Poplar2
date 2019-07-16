#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "pop_daemon.h"
#include "memory.h"

int daemon(int nochdir, int noclose)
{
	pid_t pid;

	pid = fork();
	if(pid < 0)
	{
		fprintf(stderr, "fork failed: %s\n", safe_strerror(errno));
		return -1;
	}

	if(pid != 0)
	{
		exit(0);
	}

	pid = setsid();
	if(pid < 0)
	{
		fprintf(stderr, "setsid failed: %s\n", safe_strerror(errno));
		return -1;
	}

	/*改变工作目录，使得进程不与任何文件系统联系*/
	if(!nochdir)
		chdir("/");

	if(!noclose)
	{
		int fd;
		fd = open("/dev/null", O_RDWR, 0);
		if(fd != 0)
		{
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if(fd > 2)
				close(fd);
		}
	}

	umask(0027);

	return 0;
}
