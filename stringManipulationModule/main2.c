#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

/*
 * This program prompts user for input and then return type until user decides to exit and
 * will ask for additional return string formats
 */

int main(void) {
    int fd;
    char msg[256] = " ";
    char rmsg[256];
    unsigned int cmd = 0;

    fd = open("/dev/testChar", O_RDWR);
    if (fd == -1) {
        printf("Failed to open the device driver\n");
        exit(-1);
    }

    while (strcmp(msg, "done") != 0) {
        printf("To exit the program enter \"done\"\n");
        printf("Enter a string:\n");
        fgets(msg, sizeof(msg), stdin);
        msg[strlen(msg) - 1] = '\0';

        if (strcmp(msg, "done") == 0) {
            break;
        }

        if (write(fd, &msg, strlen(msg))) {
            printf("Error: could not write to buffer\n");
        }

        while (cmd != 4) {

            printf("Choose return type:\n");
            printf("1. All Lowercase\n2. All Uppercase\n3. Capitalized\n4. Enter new string\n");
            scanf("%d", &cmd);
            getchar();
        
            while ((cmd < 1) || (cmd > 4)){
                printf("Enter a valid number option\n");
                scanf("%d", &cmd);
                getchar();
            }

            if (cmd == 4) {
                break;
            }

            if (ioctl(fd, 0, cmd)) {
                printf("Error: could not change the return type of buffer\n");
            }

            if (read(fd, &rmsg, strlen(msg)) < 1) {
                printf("Error: no bytes have been read\n");
            }

            printf("\nRecieved: %s\n\n", rmsg);

        }

    }

    close(fd); 
    return 0;
}