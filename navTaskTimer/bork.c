#include <stdio.h>
#include <unistd.h>

int main(void) {
    while(1) {
        printf(".\n");
        sleep(5);
    }

    return 0;
}
