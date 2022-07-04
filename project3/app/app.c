#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

int main(void){
	int fd;
	int retn;
	char buf[3] = {0,};

	fd = open("/dev/stopwatch", O_RDWR);
	if(fd < 0) {
		perror("/dev/stopwatch error");
		exit(-1);
	}
	
	retn = write(fd, buf, 3);
	close(fd);

	return 0;
}
