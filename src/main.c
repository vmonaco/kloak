#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <time.h>

#include <sys/queue.h>

#include <linux/input.h>
#include <linux/uinput.h>

#include "keycodes.h"

#define BUFSIZE 256  // For device names and rescue key sequence
#define MAX_INPUTS 1  // For now, just one. Future will allow multiple input devices
#define MAX_RESCUE_KEYS 10  // Maximum number of rescue keys to exit in case of emergency
#define DEFAULT_MAX_DELAY_MS 100  // 100 ms is short enough to not greatly affect usability
#define DEFAULT_STARTUP_DELAY_MS 500  // Wait before grabbing the input device
#define DEFAULT_POLLING_INTERVAL_MS 8  // Polling interval between writing events, quantizes output times
#define READ_TIMEOUT_US (10000000 / 100) // Timeout to check for new events

#ifndef max
#define max(a, b) ( ((a) > (b)) ? (a) : (b) )
#endif

static int rescue_keys[MAX_RESCUE_KEYS];
static int rescue_len;

static int max_delay = DEFAULT_MAX_DELAY_MS;  // Maximum delay
static int startup_timeout = DEFAULT_STARTUP_DELAY_MS;
static int polling_interval = DEFAULT_POLLING_INTERVAL_MS;
static int input_fds[MAX_INPUTS];
static int input_count = 0;
static int output_fd = -1;
static int input_fd = -1;
static int interrupt = 0;  // Flag to interrupt the main loop and exit
static int verbose = 0;  // Flag for verbose output

static char input_device[BUFSIZE] = "";
static char input_name[BUFSIZE] = "Unknown";
static char output_device[BUFSIZE] = "/dev/uinput";
static char rescue_keys_str[BUFSIZE] = "KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_ESC";
static char rescue_key_seps[] = ", ";  // delims to strtok

static struct input_event ev;
static struct uinput_user_dev dev;
static long previous_time;

static struct option long_options[] = {
        {"read",    1, 0, 'r'},
        {"write",   1, 0, 'w'},
        {"delay",   1, 0, 'd'},
        {"start",   1, 0, 's'},
        {"keys",    1, 0, 'k'},
        {"verbose", 0, 0, 'v'},
        {"help",    0, 0, 'h'},
        {0,         0, 0, 0}
};

TAILQ_HEAD(tailhead, entry) head;  // Head of the keyboard event buffer

struct entry {
    struct input_event iev;
    long time;
    TAILQ_ENTRY(entry) entries;
} *n1, *n2, *np;

struct input_event syn = {.type = EV_SYN, .code = 0, .value = 0};
struct timeval timeout = {.tv_sec = 0, .tv_usec = READ_TIMEOUT_US};

// Helper function to sleep for a duration given in milliseconds
void sleep_ms(long milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

// Helper function to return current time in milliseconds
long current_time_ms(void) {
    long ms; // Milliseconds
    time_t s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s = spec.tv_sec;
    ms = round(spec.tv_nsec / 1.0e6);
    return (spec.tv_sec) * 1000 + (spec.tv_nsec) / 1000000;
}

// Assumes 0 <= max <= RAND_MAX
// Returns in the closed interval [0, max]
// Credits: https://stackoverflow.com/questions/2509679/how-to-generate-a-random-integer-number-from-within-a-range
long random_at_most(long max) {
  unsigned long
    // max <= RAND_MAX < ULONG_MAX, so this is okay.
    num_bins = (unsigned long) max + 1,
    num_rand = (unsigned long) RAND_MAX + 1,
    bin_size = num_rand / num_bins,
    defect   = num_rand % num_bins;

  long x;
  do {
   x = random();
  }
  // This is carefully written not to overflow
  while (num_rand - defect <= (unsigned long)x);

  // Truncated division is intentional
  return x/bin_size;
}

long random_between(long min, long max) {
    return min + random_at_most(max - min);
}

// Finds the first device with "keyboard" in the name
int detect_keyboard(char* out) {
    int i;
    int fd;
    char name[256];
    char device[256];

    // Look for an input device with "keyboard" in the name
    for (i = 0; i < 8; i++) {
        sprintf(device, "/dev/input/event%d", i);
        if ((fd = open(device, O_RDONLY)) == -1) {
            continue;
        }
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        close(fd);

        if(strstr(name, "keyboard") != NULL) {
            printf("Found keyboard at: %s\n", device);
            strncpy(out, device, BUFSIZE-1);
            return 0;
        }
    }
    return -1;
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

// Write an event to the output device
void emit_event(struct entry *e) {
    int res, delay;

    // Don't do anything, waiting to exit
    if (interrupt) {
        return;
    }

    delay = (int) (e->time - current_time_ms());

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
                       "Code: %*d,  Value: %*d,  Missed target %*d ms \n",
               e->time, 3, e->iev.type, 3, e->iev.code, 3, e->iev.value, 4, delay);
    }
}

void main_loop() {
    int i, res;
    int nfds = -1;
    int rescue_state[rescue_len];
    long current_time, lower_bound, random_delay;
    fd_set read_fds;

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
        // Emit any events exceeding the current time
        current_time = current_time_ms();
        while ((np = TAILQ_FIRST(&head)) && (current_time >= np->time)) {
            emit_event(np);
            TAILQ_REMOVE(&head, np, entries);
            free(np);
        }

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
                res = read(input_fds[i], &ev, sizeof(ev));

                if (res <= 0)
                    continue;

                // check for the rescue sequence.
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

                // Schedule the keyboard event to be released sometime in the future.
                // Lower bound must be at *least* the difference between last scheduled and now
                current_time = current_time_ms();
                lower_bound = max(previous_time - current_time, 0);
                random_delay = random_between(lower_bound, max_delay);

                // Buffer the keyboard event
                n1 = malloc(sizeof(struct entry));	/* Insert at the head. */
                n1->time = current_time + (long) random_delay;
                n1->iev = ev;
                TAILQ_INSERT_TAIL(&head, n1, entries);

                previous_time = n1->time;

                if (verbose) {
                    printf("Bufferred event at time: %ld.  Type: %*d,  "
                                   "Code: %*d,  Value: %*d,  Scheduled delay %*ld ms \n",
                           n1->time, 3, n1->iev.type, 3, n1->iev.code, 3, n1->iev.value,
                           4, previous_time - current_time);
                    if (lower_bound > 0) {
                        printf("Lower bound raised to: %*ld ms\n", 4, lower_bound);
                    }
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
    fprintf(stderr, "  -k rescue keys: csv list of rescue key names to exit kloak in case the\n"
            "     keyboard becomes unresponsive. Default is 'KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_ESC'.\n");
    fprintf(stderr, "  -v: verbose mode\n");
}

void banner() {
    int i;

    printf("********************************************************************************\n"
                   "* Started kloak : Keystroke-level Online Anonymizing Kernel\n"
                   "* Reading from  : %s (%s)\n"
                   "* Writing to    : %s\n"
                   "* Maximum delay : %d ms\n",
           input_device, input_name, output_device, max_delay);
    printf("* Rescue keys   : %s", lookup_keyname(rescue_keys[0]));
    for (i = 1; i < rescue_len; i++) { printf(" + %s", lookup_keyname(rescue_keys[i])); }
    printf("\n");
    printf("********************************************************************************\n");
}

int main(int argc, char **argv) {
    int c, i, res, keycode;
    int option_index = 0;
    char *token, *_rescue_keys_str;

    while (1) {
        c = getopt_long(argc, argv, "r:w:d:s:k:vh", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
            case 'r': {
                if (input_device != NULL) {
                    fprintf(stderr, "Multiple -r options are not allowed\n");
                    usage();
                    exit(1);
                }
                strncpy(input_device, optarg, BUFSIZE-1);
                break;
            }

            case 'w':
                if (output_device != NULL) {
                    fprintf(stderr, "Multiple -w options are not allowed\n");
                    usage();
                    exit(1);
                }
                strncpy(output_device, optarg, BUFSIZE-1);
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
            case 'k':
                strncpy(rescue_keys_str, optarg, BUFSIZE-1);
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

    if ((strlen(input_device) == 0) && (detect_keyboard(input_device) == -1)) {
        fprintf(stderr, "Unable to find a keyboard. Specify which input device to use with the -r parameter");
        exit(1);
    }

    // Set the rescue keys
    _rescue_keys_str = malloc(strlen(rescue_keys_str) + 1);
    strncpy(_rescue_keys_str, rescue_keys_str, strlen(rescue_keys_str));
    token = strtok(_rescue_keys_str, rescue_key_seps);
    while (token != NULL) {
        keycode = lookup_keycode(token);
        if (keycode < 0) {
            fprintf(stderr, "Invalid key name: '%s'\nSee keycodes.h for valid names.\n", token);
            exit(1);
        } else if (rescue_len < MAX_RESCUE_KEYS) {
            rescue_keys[rescue_len] = keycode;
            rescue_len++;
        } else {
            fprintf(stderr, "Cannot set more than %d rescue keys.\n", MAX_RESCUE_KEYS);
            exit(1);
        }
        token = strtok(NULL, rescue_key_seps);
    }
    free(_rescue_keys_str);

    // Initialize the output device first, then wait to initialize input
    // This allows any keystroke events (e.g., releasing the Return key)
    // to finish before grabbing the input device
    init_output(output_device);

    printf("Waiting %d ms...\n", startup_timeout);
    sleep_ms(startup_timeout);

    init_input(input_device);
    banner();

    TAILQ_INIT(&head);
    main_loop();

    for (i = 0; i < input_count; i++)
        close(input_fds[i]);

    close(output_fd);
}
