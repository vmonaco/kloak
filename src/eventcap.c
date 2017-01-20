#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <signal.h>

#include <linux/input.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>

void usage() {
    fprintf(stderr, "Usage: eventcap <device>\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    struct input_event ev;
    int fd;
    char name[256] = "Unknown";
    char *device = NULL;

    if (argv[1] == NULL) {
        usage();
    }

    if ((getuid()) != 0)
        printf("You are not root! This may not work...\n");

    if (argc > 1)
        device = argv[1];

    //Open Device
    if ((fd = open(device, O_RDONLY)) == -1) {
        fprintf(stderr, "%s is not a valid device\n", device);
        exit(1);
    }

    //Print Device Name
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    printf("Reading From : %s (%s)\n", device, name);

    while (1) {
        if (read(fd, &ev, sizeof(struct input_event)) <= 0) {
            perror("read()");
            exit(1);
        }
        printf("Type: %*d    Code: %*d    Value: %*d\n", 3, ev.type, 3, ev.code, 3, ev.value);
    }
}
