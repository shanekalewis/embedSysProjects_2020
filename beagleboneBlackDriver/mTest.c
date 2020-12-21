#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

int main(int argc, char **argv) {
	int fd;
	int rc = 0;
	char *rd_buf[16];
	
	printf("%s: entered\n", argv[0]);
	printf("Writing: %s\n", argv[1]);
	
	//open the device
	fd = open("/dev/moroseCodeDriver", O_RDWR);
	if (fd == -1) {
		perror("open failed\n");
		rc = fd;
		exit(-1);
	}
	
	printf("%s: open: successful\n", argv[0]);
	
	rc = write(fd, argv[1], strlen(argv[1]));
	if ( rc == -1 ) {
		perror("write failed");
		close(fd);
		exit(-1);
	}

	printf("%s: write: successful\n", argv[0]);

 	close(fd);
 	return 0;
}
	