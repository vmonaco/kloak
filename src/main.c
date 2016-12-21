#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <linux/input.h>
#include <linux/uinput.h>

#define MAX_INPUTS 1  // For now, just one. Future will allow multiple input devices
#define DEFAULT_MAX_delay_MS 100  // 100 ms is short enough to not greatly affect usability
#define DEFAULT_STARTUP_MS 500
#define TIMER_INTERVAL_US (10000000 / 100) // 1/100 sec.

#ifndef max
#define max(a, b) ( ((a) > (b)) ? (a) : (b) )
#endif

static int rescue_keys[] = {
        KEY_RIGHTSHIFT,
        KEY_RIGHTCTRL,
};

#define rescue_len (sizeof(rescue_keys) / sizeof(int))

static struct option long_options[] = {
        {"start",   1, 0, 's'},
        {"delay",   1, 0, 'd'},
        {"read",    1, 0, 'r'},
        {"write",   1, 0, 'w'},
        {"verbose", 0, 0, 'v'},
        {"help",    0, 0, 'h'},
        {0,         0, 0, 0}
};

static int max_delay = DEFAULT_MAX_delay_MS;
static int startup_timeout = DEFAULT_STARTUP_MS;
static int verbose = 0;
static int input_fds[MAX_INPUTS];
static int input_count = 0;
static int output_fd = -1;
static int input_fd = -1;
static int interrupt = 0;

static char *output_device = NULL;
static char *input_device = NULL;
static char input_name[256] = "Unknown";

static struct input_event ev;
static struct uinput_user_dev dev;
static long previous_time;

struct input_event syn = {.type = EV_SYN, .code = 0, .value = 0};

// An input event with a timestamp when it should occur
struct timed_event {
    struct input_event iev;
    long time;
};

void sleep_ms(long milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

long current_time_ms(void) {
    long ms; // Milliseconds
    time_t s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s = spec.tv_sec;
    ms = round(spec.tv_nsec / 1.0e6);
    return (spec.tv_sec) * 1000 + (spec.tv_nsec) / 1000000;
}

unsigned int rand_int(unsigned int min, unsigned int max) {
    int r;
    const unsigned int range = 1 + max - min;
    const unsigned int buckets = RAND_MAX / range;
    const unsigned int limit = buckets * range;

    /* Create equal size buckets all in a row, then fire randomly towards
     * the buckets until you land in one of them. All buckets are equally
     * likely. If you land off the end of the line of buckets, try again. */
    do {
        r = rand();
    } while (r >= limit);

    return min + (r / buckets);
}

void init_input(char *file) {
    int one = 1;

    if ((input_fd = open(file, O_RDONLY)) < 0) {
        fprintf(stderr, "Could not open input device %s: %s\n", file, strerror(errno));
        exit(1);
    }
    // Grad the input device so that all events are redirected to the output
    if (ioctl(input_fd, EVIOCGRAB, (void *) 1) < 0) {
        fprintf(stderr, "Unable to grab device '%s' : %s\n", file, strerror(errno));
        exit(1);
    }
    if (ioctl(input_fd, FIONBIO, (void *) &one) < 0) {
        fprintf(stderr, "Unable to set device '%s' to non-blocking mode : %s\n", file, strerror(errno));
        exit(1);
    }
    if (ioctl(input_fd, EVIOCGNAME(sizeof(input_name)), input_name) < 0) {
        fprintf(stderr, "Unable to determine device name : %s\n", strerror(errno));
        // Don't exit, not a fatal error
    }
    // TODO: handle multiple input devices with possibly different delays
    if (input_count < MAX_INPUTS) {
        input_fds[input_count++] = input_fd;
    } else {
        fprintf(stderr, "Maximum number of input devices exceeded (%d)\n", MAX_INPUTS);
        exit(1);
    }
}

void init_output(char *file) {
    int i;

    output_fd = open(file, O_WRONLY | O_NDELAY);
    if (output_fd < 0) {
        fprintf(stderr, "Could not open output device %s: %s\n", file, strerror(errno));
        exit(1);
    }
    // We only care about key and syn events
    if (ioctl(output_fd, UI_SET_EVBIT, EV_KEY) < 0) {
        fprintf(stderr, "Error setting EV_KEY at line %d: %s\n", __LINE__, strerror(errno));
        exit(1);
    }
    if (ioctl(output_fd, UI_SET_EVBIT, EV_SYN) < 0) {
        fprintf(stderr, "Error setting EV_SYN at line %d: %s\n", __LINE__, strerror(errno));
        exit(1);
    }
    for (i = 1; i < KEY_UNKNOWN; i++) {
        if (ioctl(output_fd, UI_SET_KEYBIT, i) < 0) {
            fprintf(stderr, "Error registering key %d at line %d: %s\n", i, __LINE__, strerror(errno));
            exit(1);
        }
    }

    snprintf(dev.name, UINPUT_MAX_NAME_SIZE, "kloak");
    if (write(output_fd, &dev, sizeof(dev)) < 0) {
        fprintf(stderr, "error at line %d: %s\n", __LINE__, strerror(errno));
        exit(1);
    }

    if (ioctl(output_fd, UI_DEV_CREATE, 0) < 0) {
        fprintf(stderr, "error at line %d: %s\n", __LINE__, strerror(errno));
        exit(1);
    }
}

// Meant to be called with pthread_create,
// releases an event at some time in the future
void *emit_event(void *arg) {
    int res, delay;

    // arg should be a pointer to a timed_event
    struct timed_event *e = (struct timed_event *) arg;

    // Sleep until the event is ready to be released
    // unless the scheduled time already passed
    delay = (int) (e->time - current_time_ms());
    sleep_ms(delay);

    // Don't do anything, waiting to exit
    if (interrupt) {
        return NULL;
    }

    if ((res = write(output_fd, &(e->iev), sizeof(struct input_event)) < 0)) {
        fprintf(stderr, "write() failed: %s\n", strerror(errno));
        exit(1);
    }

    // Since the SYN events are ignored below, write a SYN event
    if ((res = (write(output_fd, &(syn), sizeof(struct input_event)) < 0)) < 0) {
        fprintf(stderr, "write() failed: %s\n", strerror(errno));
        exit(1);
    }

    if (verbose) {
        printf("-Released event at time: %ld.  Type: %*d,  "
                       "Code: %*d,  Value: %*d,  Actual delay %*d ms \n",
               e->time, 3, e->iev.type, 3, e->iev.code, 3, e->iev.value, 4, delay);
    }

    free(e); // Done with this event
    return NULL;
}

void main_loop() {
    int i, res;
    int nfds = -1;
    int rescue_state[rescue_len];
    long current_time;
    unsigned int lower_bound, random_delay;
    struct timeval timeout;
    struct timed_event *e;
    fd_set read_fds;
    pthread_t emission_thread;

    // initialize the rescue state
    for (i = 0; i < rescue_len; i++)
        rescue_state[i] = 0;

    for (i = 0; i < input_count; i++)
        if (input_fds[i] > nfds)
            nfds = input_fds[i];
    nfds++;

    /* Main loop breaks when the rescue keys are detected
     * On each iteration, wait for input from the input devices
     * If the event is a key press/release, then schedule for
     * release in the future by generating a random delay. The
     * range of the delay depends on the previous event generated
     * so that events are always scheduled in the order they
     * arrive (FIFO).
     */

    while (!interrupt) {
        struct input_event *ptr = &ev;

        timeout.tv_sec = 0;
        timeout.tv_usec = TIMER_INTERVAL_US;

        FD_ZERO(&read_fds);
        for (i = 0; i < input_count; i++) {
            FD_SET(input_fds[i], &read_fds);
        }

        // Wait for next input event
        res = select(nfds, &read_fds, NULL, NULL, &timeout);

        if (res == -1) {
            fprintf(stderr, "select() failed: %s\n", strerror(errno));
            exit(1);
        }

        // select() timed out, do nothing
        if (res == 0) {
            continue;
        }

        for (i = 0; i < input_count; i++) {
            if (FD_ISSET(input_fds[i], &read_fds)) {
                res = read(input_fds[i], ptr, sizeof(ev));

                if (res <= 0)
                    continue;

                // check for rescue sequence.
                if (ev.type == EV_KEY) {
                    int all = 1;
                    for (i = 0; i < rescue_len; i++) {
                        if (rescue_keys[i] == ev.code)
                            rescue_state[i] = (ev.value == 0 ? 0 : 1);
                        all = all && rescue_state[i];
                    }
                    if (all)
                        interrupt = 1;
                }

                // Ignore events other than EV_KEY
                if (ev.type != EV_KEY) {
                    continue;
                }

                // Ignore key repeat events (1 is press, 0 is release, 2 is repeat)
                if (ev.type == EV_KEY && ev.value == 2) {
                    continue;
                }

                // Received a key press or release event. Schedule the event
                // to be release sometime in the future
                current_time = current_time_ms();

                // Lower bound must be at *least* the different between last scheduled and now
                // Cannot schedule an event in the past, so must also be gte
                lower_bound = (unsigned int) max(previous_time - current_time, 0);

                random_delay = (long) rand_int(lower_bound, max_delay);

                e = malloc(sizeof(struct timed_event));
                e->time = current_time + (long) random_delay;
                e->iev = ev;

                previous_time = e->time;

                if (verbose) {
                    printf("+Received event at time: %ld.  Type: %*d,  "
                                   "Code: %*d,  Value: %*d,  Scheduled delay %*ld ms \n",
                           e->time, 3, e->iev.type, 3, e->iev.code, 3, e->iev.value,
                           4, previous_time - current_time);
                    if (lower_bound > 0) {
                        printf("Lower bound raised to: %*d ms\n", 4, lower_bound);
                    }
                }

                if ((res = pthread_create(&emission_thread, NULL, emit_event, e)) != 0) {
                    fprintf(stderr, "pthread_create() failed: %s\n", strerror(errno));
                }
            }
        }
    }
}

void usage() {
    fprintf(stderr, "Usage: kloak [options]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -r device file: read given input device (multiple allowed)\n");
    fprintf(stderr, "  -w device file: write to the given uinput device (mandatory option)\n");
    fprintf(stderr, "  -d delay: maximum delay (in milliseconds) of released events. Default 100.\n");
    fprintf(stderr, "  -s startup timeout: time to wait (in milliseconds) before startup. Default 100.\n");
    fprintf(stderr, "  -v: verbose mode\n");
}

void banner() {
    printf("********************************************************************************\n"
                   "* Started kloak: Keystroke-level Online Anonymizing Kernel\n"
                   "* Reading from  : %s (%s)\n"
                   "* Writing to    : %s\n"
                   "* Maximum delay : %d ms\n"
                   "* In case the keyboard becomes unresponsive, the rescue keys to exit are:\n"
                   "*                           Right Shift + Right Ctrl\n"
                   "********************************************************************************\n",
           input_device, input_name, output_device, max_delay);
}

int main(int argc, char **argv) {
    int c, i, res;
    int option_index = 0;

    while (1) {
        c = getopt_long(argc, argv, "s:d:r:w:vh", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
            case 'r': {
                if (input_device != NULL) {
                    fprintf(stderr, "Multiple -r options are not allowed\n");
                    usage();
                    exit(1);
                }
                input_device = optarg;
                break;
            }

            case 'w':
                if (output_device != NULL) {
                    fprintf(stderr, "Multiple -w options are not allowed\n");
                    usage();
                    exit(1);
                }
                output_device = optarg;
                break;

            case 'd':
                max_delay = atoi(optarg);
                if (max_delay < 0) {
                    fprintf(stderr, "Maximum delay must be >= 0\n");
                    exit(1);
                }
                break;

            case 's':
                startup_timeout = atoi(optarg);
                if (startup_timeout < 0) {
                    fprintf(stderr, "Startup timeout must be >= 0\n");
                    exit(1);
                }
                break;

            case 'v':
                verbose = 1;
                break;

            case 'h':
                usage();
                exit(0);
                break;

            default:
                fprintf(stderr, "getopt returned unexpected char code: 0%o\n", c);
                exit(1);
        }
    }

    if (argc - optind != 0) {
        fprintf(stderr, "Extra parameters detected: %d %d.\n", argc, optind);
        usage();
        exit(1);
    }

    if ((getuid()) != 0)
        printf("You are not root! This may not work...\n");

    // Initialize the output device first, then wait to initialize input
    // This allows any keystroke events (e.g., releasing the Return key)
    // to finish before grabbing the input device
    init_output(output_device);

    printf("Waiting %d ms...\n", startup_timeout);
    sleep_ms(startup_timeout);

    init_input(input_device);
    banner();
    main_loop();

    for (i = 0; i < input_count; i++)
        close(input_fds[i]);

    close(output_fd);
}
