#include <poll.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <time.h>
#include <sodium.h>
#include <sys/queue.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <ctype.h>
#include <math.h>

#include "keycodes.h"

#define BUFSIZE 256  // for device names and rescue key sequence
#define MAX_INPUTS 16  // number of devices to try autodetection
#define MAX_DEVICES 16 // max number of devices to read events from
#define MAX_RESCUE_KEYS 10  // max number of rescue keys to exit in case of emergency
#define MIN_KEYBOARD_KEYS 20  // need at least this many keys to be a keyboard
#define POLL_TIMEOUT_MS 1 // timeout to check for new events
#define DEFAULT_MAX_DELAY_MS 20  // upper bound on event delay
#define DEFAULT_MAX_NOISE 0
#define DEFAULT_STARTUP_DELAY_MS 500  // wait before grabbing the input device

#define panic(format, ...) do { fprintf(stderr, format "\n", ## __VA_ARGS__); exit(EXIT_FAILURE); } while (0)

#ifndef min
#define min(a, b) ( ((a) < (b)) ? (a) : (b) )
#endif

#ifndef max
#define max(a, b) ( ((a) > (b)) ? (a) : (b) )
#endif

static int interrupt = 0;  // flag to interrupt the main loop and exit
static int verbose = 0;  // flag for verbose output

static char rescue_key_seps[] = ", ";  // delims to strtok
static char rescue_keys_str[BUFSIZE] = "KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_ESC";
static int rescue_keys[MAX_RESCUE_KEYS];  // Codes of the rescue key combo
static int rescue_len;  // Number of rescue keys, set during initialization

static int max_delay = DEFAULT_MAX_DELAY_MS;  // lag will never exceed this upper bound
static int max_noise = DEFAULT_MAX_NOISE;
static int startup_timeout = DEFAULT_STARTUP_DELAY_MS;

static int device_count = 0;
static char named_inputs[MAX_INPUTS][BUFSIZE];

static int input_fds[MAX_INPUTS];
struct libevdev *output_devs[MAX_INPUTS];
struct libevdev_uinput *uidevs[MAX_INPUTS];

static int is_qubes = 0;

static struct option long_options[] = {
        {"read",    1, 0, 'r'},
        {"delay",   1, 0, 'd'},
        {"start",   1, 0, 's'},
        {"keys",    1, 0, 'k'},
        {"verbose", 0, 0, 'v'},
        {"help",    0, 0, 'h'},
        {"noise",   1, 0, 'n'},
        {0,         0, 0, 0}
};

TAILQ_HEAD(tailhead, entry) head;

struct entry {
        struct input_event iev;
        long time;
        TAILQ_ENTRY(entry) entries;
        int device_index;
};

void sleep_ms(long milliseconds) {
        struct timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = (milliseconds % 1000) * 1000000;
        nanosleep(&ts, NULL);
}

long current_time_ms(void) {
        struct timespec spec;
        clock_gettime(CLOCK_REALTIME, &spec);
        return (spec.tv_sec) * 1000 + (spec.tv_nsec) / 1000000;
}

long random_between(long lower, long upper) {
        // default to max if the interval is not valids
        if (lower >= upper)
                return upper;

        return lower + randombytes_uniform(upper+1);
}

void set_rescue_keys(char* rescue_keys_str) {
        char *_rescue_keys_str = malloc(strlen(rescue_keys_str) + 1);
        strncpy(_rescue_keys_str, rescue_keys_str, strlen(rescue_keys_str));
        char* token = strtok(_rescue_keys_str, rescue_key_seps);

        while (token != NULL) {
                int keycode = lookup_keycode(token);
                if (keycode < 0) {
                        panic("Invalid key name: '%s'\nSee keycodes.h for valid names", token);
                } else if (rescue_len < MAX_RESCUE_KEYS) {
                        rescue_keys[rescue_len] = keycode;
                        rescue_len++;
                } else {
                        panic("Cannot set more than %d rescue keys", MAX_RESCUE_KEYS);
                }
                token = strtok(NULL, rescue_key_seps);
        }
        free(_rescue_keys_str);
}

int supports_event_type(int device_fd, int event_type) {
        unsigned long evbit = 0;
        // Get the bit field of available event types.
        ioctl(device_fd, EVIOCGBIT(0, sizeof(evbit)), &evbit);
        return evbit & (1 << event_type);
}

int supports_specific_key(int device_fd, unsigned int key) {
        size_t nchar = KEY_MAX/8 + 1;
        unsigned char bits[nchar];
        // Get the bit fields of available keys.
        ioctl(device_fd, EVIOCGBIT(EV_KEY, sizeof(bits)), &bits);
        return bits[key/8] & (1 << (key % 8));
}


int is_keyboard(int fd) {
        int key;
        int num_supported_keys;

        // Only check devices that support EV_KEY events
        if (supports_event_type(fd, EV_KEY)) {
                num_supported_keys = 0;

                // Count the number of KEY_* events that are supported
                for (key = 0; key <= KEY_MAX; key++) {
                        if (supports_specific_key(fd, key)) {
                                num_supported_keys += 1;
                        }
                }
        }

        return (num_supported_keys > MIN_KEYBOARD_KEYS);
}

int is_mouse(int fd) {
        return (supports_event_type(fd, EV_REL) || supports_event_type(fd, EV_ABS));
}



// extracts device number from device_path and puts it into dev_num
void get_device_number(char *dev_num, char *device_path) {

        snprintf(dev_num, 3, device_path + 16);

}





int change_qubes_input_sender(char *systemd_command, char *device_path) {
        
        const int MAX_COMMAND_LEN = 155;
        char command[MAX_COMMAND_LEN];

        char dev_num[3];

        get_device_number(dev_num, device_path);

        int status = 0;


        // if starting or stopping the service, see if it is exists first
        if(strncmp(systemd_command, "stop", 4) == 0 || strncmp(systemd_command, "start", 5) == 0) {
                status = change_qubes_input_sender("status", device_path);

                if((status == 0 && strncmp(systemd_command, "stop", 4) == 0) || strncmp(systemd_command, "start", 5) == 0) {
                        snprintf(command, MAX_COMMAND_LEN, "sudo systemctl -q --no-pager %s qubes-input-sender-keyboard@event%s.service 2> /dev/null > /dev/null", systemd_command , dev_num);

                        if(verbose) {
                                printf("Executing: %s\n", command);
                        }
                        return system(command);

                } // see if keyboard-mouse service exists
                else {
                        snprintf(command, MAX_COMMAND_LEN, "sudo systemctl -q --no-pager status qubes-input-sender-keyboard-mouse@event%s.service 2> /dev/null > /dev/null", dev_num);
                        if(verbose) {
                                printf("Executing: %s\n", command);
                        }

                        status = system(command);
                        
                        if((status == 0 && strncmp(systemd_command, "stop", 4) == 0) || (status == 3 && strncmp(systemd_command, "start", 5) == 0)) {
                                snprintf(command, MAX_COMMAND_LEN, "sudo systemctl -q --no-pager %s qubes-input-sender-keyboard-mouse@event%s.service 2> /dev/null > /dev/null", systemd_command , dev_num);
                                if(verbose) {
                                        printf("Executing: %s\n", command);
                                }
                                
                                return system(command);
                        } else {
                                // neither exists
                                return 0;
                        }
                }

        } else {
                snprintf(command, MAX_COMMAND_LEN , "sudo systemctl -q --no-pager %s qubes-input-sender-keyboard@event%s.service 2> /dev/null > /dev/null", systemd_command , dev_num);

                if(verbose) {
                        printf("Executing: %s\n", command);
                }
                
                return system(command);
        }


}


void restart_all_qubes_input_sender() {
        for(int i = 0; i < device_count; i++) {
                change_qubes_input_sender("start", named_inputs[i]);
        }
}



void detect_devices() {
        int fd;
        char device[256];

        for (int i = 0; i < MAX_DEVICES; i++) {
                sprintf(device, "/dev/input/event%d", i);

                // if in qubes, need to stop associated qubes-input-sender service before ioctls work on the device file
                if(is_qubes) {
                        // stop the service
                        change_qubes_input_sender("stop", device);
                }


                if ((fd = open(device, O_RDONLY)) < 0) {
                        continue;
                }

                if (is_keyboard(fd)) {
                        strncpy(named_inputs[device_count++], device, BUFSIZE-1);
                        if (verbose)
                                printf("Found keyboard at: %s\n", device);
                } else if (is_mouse(fd)) {
                        strncpy(named_inputs[device_count++], device, BUFSIZE-1);
                        if (verbose)
                                printf("Found mouse at: %s\n", device);
                } else if (is_qubes && (change_qubes_input_sender("status", device) == 3)) { 
                        
                        // if not keyboard and mouse, in case qubes-input-sender was stopped, restart it. Don't restart if keyboard or mouse
                        change_qubes_input_sender("start", device);
                }

                close(fd);

                if (device_count >= MAX_INPUTS) {
                        if (verbose)
                                printf("Warning: ran out of inputs while detecting devices\n");
                        break;
                }
        }
}



void init_inputs() {
        int fd;
        int one = 1;


        for (int i = 0; i < device_count; i++) {


                // in qubes, need to kill the associated qubes-input-sender systemd service if still running before you can grab the device
                if(is_qubes) {

                        // stop the service
                        change_qubes_input_sender("stop", named_inputs[i]);

                        // check status to make sure the service it was definitely stopped
                        if(change_qubes_input_sender("status", named_inputs[i]) == 0) {
                                panic("Failed to stop qubes-input-sender service for %s", named_inputs[i]);
                        }

                }



                if ((fd = open(named_inputs[i], O_RDONLY)) < 0) {

                        // if in qubes, make sure qubes-input-sender service is restarted before panic
                        if(is_qubes) {
                                restart_all_qubes_input_sender();
                        }

                        panic("Could not open: %s", named_inputs[i]);
                }


                // set the device to nonblocking mode
                if (ioctl(fd, FIONBIO, &one) < 0) {

                        // if in qubes, make sure qubes-input-sender service is running before panic
                        if(is_qubes) {
                                restart_all_qubes_input_sender();
                        }

                        panic("Could set to nonblocking: %s", named_inputs[i]);
                }

                


                // grab the input device
                if (ioctl(fd, EVIOCGRAB, &one) < 0) {

                        // if in qubes, make sure qubes-input-sender service is running before panic
                        if(is_qubes) {
                                restart_all_qubes_input_sender();
                        }

                        panic("Could not grab: %s", named_inputs[i]);
                }


                input_fds[i] = fd;
        }
}

void init_outputs() {
        for (int i = 0; i < device_count; i++) {
                int err = libevdev_new_from_fd(input_fds[i], &output_devs[i]);

                if (err != 0){
                        // if in qubes, make sure qubes-input-sender service is running before panic
                        if(is_qubes) {
                                restart_all_qubes_input_sender();
                        }

                        panic("Could not create evdev for input device: %s", named_inputs[i]);
                }

                err = libevdev_uinput_create_from_device(output_devs[i], LIBEVDEV_UINPUT_OPEN_MANAGED, &uidevs[i]);

                if (err != 0) {
                        // if in qubes, make sure qubes-input-sender service is running before panic
                        if(is_qubes) {
                                restart_all_qubes_input_sender();
                        }

                        panic("Could not create uidev for input device: %s", named_inputs[i]);
                }
        }
}

void emit_event(struct entry *e) {
        int res, delay;
        long now = current_time_ms();
        delay = (int) (e->time - now);

        libevdev_uinput_write_event(uidevs[e->device_index], e->iev.type, e->iev.code, e->iev.value);

        if (verbose) {
                printf("Released event at time : %ld. Device: %d,  Type: %*d,  "
                       "Code: %*d,  Value: %*d,  Missed target:  %*d ms \n",
                       e->time, e->device_index, 3, e->iev.type, 5, e->iev.code, 5, e->iev.value, 5, delay);
        }
}

void main_loop() {
        int err;
        long
                prev_release_time = 0,
                current_time = 0,
                lower_bound = 0,
                random_delay = 0;
        struct input_event ev;
        struct entry *n1, *np;
        
        // only used on mouse moves
        struct input_event ev2, ev3, ev4, ev5;
        struct entry *n2 = NULL, *n3 = NULL, *n4 = NULL, *n5 = NULL;


        // initialize the rescue state
        int rescue_state[rescue_len];
        for (int i = 0; i < rescue_len; i++) {
                rescue_state[i] = 0;
        }

        // load input file descriptors for polling
        struct pollfd *pfds = calloc(device_count, sizeof(struct pollfd));
        for (int j = 0; j < device_count; j++) {
                pfds[j].fd = input_fds[j];
                pfds[j].events = POLLIN;
        }

        // the main loop breaks when the rescue keys are detected
        // On each iteration, wait for input from the input devices
        // If the event is a key press/release, then schedule for
        // release in the future by generating a random delay. The
        // range of the delay depends on the previous event generated
        // so that events are always scheduled in the order they
        // arrive (FIFO).
        while (!interrupt) {
                // Emit any events exceeding the current time
                current_time = current_time_ms();
                while ((np = TAILQ_FIRST(&head)) && (current_time >= np->time)) {
                        emit_event(np);
                        TAILQ_REMOVE(&head, np, entries);
                        free(np);
                }

                // Wait for next input event
                if ((err = poll(pfds, device_count, POLL_TIMEOUT_MS)) < 0) {
                        // if in qubes, make sure qubes-input-sender service is running before panic
                        if(is_qubes) {
                                restart_all_qubes_input_sender();
                        }

                        panic("poll() failed: %s\n", strerror(errno));
                }

                // timed out, do nothing
                if (err == 0)
                        continue;

                // An event is available, mark the current time
                current_time = current_time_ms();

                // Buffer the event with a random delay
                for (int k = 0; k < device_count; k++) {
                        if (pfds[k].revents & POLLIN) {
                                if ((err = read(pfds[k].fd, &ev, sizeof(ev))) <= 0) {
                                        // if in qubes, make sure qubes-input-sender service is running before panic
                                        if(is_qubes) {
                                                restart_all_qubes_input_sender();
                                        }

                                        panic("read() failed: %s", strerror(errno));
                                }

                                // check for the rescue sequence.
                                if (ev.type == EV_KEY) {
                                        int all = 1;
                                        for (int j = 0; j < rescue_len; j++) {
                                                if (rescue_keys[j] == ev.code)
                                                        rescue_state[j] = (ev.value == 0 ? 0 : 1);
                                                all = all && rescue_state[j];
                                        }
                                        if (all)
                                                interrupt = 1;
                                }

                                // schedule the keyboard event to be released sometime in the future.
                                // lower bound must be bounded between time since last scheduled event and max delay
                                // preserves event order and bounds the maximum delay
                                lower_bound = min(max(prev_release_time - current_time, 0), max_delay);

                                // syn events are not delayed
                                if (ev.type == EV_SYN) {
                                        random_delay = lower_bound;
                                } else {
                                        random_delay = random_between(lower_bound, max_delay);
                                }


                                if(ev.type == EV_REL && ev.value != 0 && max_noise != 0) {
                                        if(ev.code == REL_X) {

                                                // select a random midpoint to add the perpendicular move
                                                int mid_point = random_between(1, abs(ev.value));

                                                int final_move = abs(ev.value) - mid_point;

                                                if(ev.value < 0) {
                                                        mid_point *= -1;
                                                        final_move *= -1;
                                                }

                                                int pixels_y = random_between(1, max_noise);

                                                // randomly decide whether y move will be up or down
                                                if(random_between(0, 1)) {
                                                        pixels_y *= -1;
                                                }


                                                ev.type = EV_REL;
                                                ev.code = REL_X;
                                                ev.value = mid_point;

                                                ev2.type = EV_REL;
                                                ev2.code = REL_Y;
                                                ev2.value = pixels_y;

                                                ev3.type = EV_SYN;
                                                ev3.code = 0;
                                                ev3.value = 0;

                                                ev4.type = EV_REL;
                                                ev4.code = REL_Y;
                                                ev4.value = pixels_y * -1;

                                                ev5.type = EV_REL;
                                                ev5.code = REL_X;
                                                ev5.value = final_move;

                                        } else if(ev.code == REL_Y) {
                                                // select a random midpoint to add the perpendicular move
                                                int mid_point = random_between(1, abs(ev.value));

                                                int final_move = abs(ev.value) - mid_point;

                                                if(ev.value < 0) {
                                                        mid_point *= -1;
                                                        final_move *= -1;
                                                }

                                                int pixels_x = random_between(1, max_noise);

                                                // randomly decide whether y move will be up or down
                                                if(random_between(0, 1)) {
                                                        pixels_x *= -1;
                                                }



                                                ev.type = EV_REL;
                                                ev.code = REL_Y;
                                                ev.value = mid_point;

                                                ev2.type = EV_REL;
                                                ev2.code = REL_X;
                                                ev2.value = pixels_x;

                                                ev3.type = EV_SYN;
                                                ev3.code = 0;
                                                ev3.value = 0;

                                                ev4.type = EV_REL;
                                                ev4.code = REL_X;
                                                ev4.value = pixels_x * -1;

                                                ev5.type = EV_REL;
                                                ev5.code = REL_Y;
                                                ev5.value = final_move;
                                        }
                                }

                                int last_event_time = 0;



                                // Buffer the event
                                n1 = malloc(sizeof(struct entry));
                                n1->time = current_time + (long) random_delay;
                                n1->iev = ev;
                                n1->device_index = k;
                                TAILQ_INSERT_TAIL(&head, n1, entries);


                                last_event_time = n1->time;


                                // if mouse move, buffer the extra events
                                if(ev.type == EV_REL && max_noise != 0 && ev.value != 0 && (ev.code == REL_X || ev.code == REL_Y)) {


                                        // if the times these are given are actually incremental (n2 = n1 + rand, n3 = n2 + rand, etc) it seems to break the cursor movement obfuscation for some reason
                                        random_delay = random_between(lower_bound, max_delay);
                                        n2 = malloc(sizeof(struct entry));
                                        n2->time = current_time + (long) random_delay;
                                        n2->iev = ev2;
                                        n2->device_index = k;
                                        TAILQ_INSERT_TAIL(&head, n2, entries);

                                        random_delay = random_between(lower_bound, max_delay);
                                        n3 = malloc(sizeof(struct entry));
                                        n3->time = current_time + (long) random_delay;
                                        n3->iev = ev3;
                                        n3->device_index = k;
                                        TAILQ_INSERT_TAIL(&head, n3, entries);

                                        random_delay = random_between(lower_bound, max_delay);
                                        n4 = malloc(sizeof(struct entry));
                                        n4->time = current_time + (long) random_delay;
                                        n4->iev = ev4;
                                        n4->device_index = k;
                                        TAILQ_INSERT_TAIL(&head, n4, entries);

                                        random_delay = random_between(lower_bound, max_delay);
                                        n5 = malloc(sizeof(struct entry));
                                        n5->time = current_time + (long) random_delay;
                                        n5->iev = ev5;
                                        n5->device_index = k;
                                        TAILQ_INSERT_TAIL(&head, n5, entries);
                                }



                                // Keep track of the previous scheduled release time
                                prev_release_time = n1->time;

                                // on mouse moves
                                if(ev.type == EV_REL && ev.value != 0 && (ev.code == REL_X || ev.code == REL_Y)) {
                                        prev_release_time = n5->time;
                                }

                                if (verbose) {
                                        printf("Bufferred event at time: %ld. Device: %d,  Type: %*d,  "
                                               "Code: %*d,  Value: %*d,  Scheduled delay: %*ld ms \n",
                                               n1->time, k, 3, n1->iev.type, 5, n1->iev.code, 5, n1->iev.value,
                                               4, random_delay);
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
        fprintf(stderr, "  -r filename: device file to read events from. Can specify multiple -r options.\n");
        fprintf(stderr, "  -d delay: maximum delay (milliseconds) of released events. Default 100.\n");
        fprintf(stderr, "  -s startup_timeout: time to wait (milliseconds) before startup. Default 100.\n");
        fprintf(stderr, "  -k csv_string: csv list of rescue key names to exit kloak in case the\n"
                "     keyboard becomes unresponsive. Default is 'KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_ESC'.\n");
        fprintf(stderr, "  -v: verbose mode\n");
        fprintf(stderr, "  -n: max noise added to mouse movements in pixels. Default %d\n, can fully disable by setting to 0", max_noise);
}

void banner() {
        printf("********************************************************************************\n"
               "* Started kloak : Keystroke-level Online Anonymizing Kernel\n"
               "* Maximum delay : %d ms\n"
               "* Reading from  : %s\n",
               max_delay, named_inputs[0]);

        for (int i = 1; i < device_count; i++) {
                printf("*                 %s\n", named_inputs[i]);
        }

        printf("* Rescue keys   : %s", lookup_keyname(rescue_keys[0]));
        for (int i = 1; i < rescue_len; i++) {
                printf(" + %s", lookup_keyname(rescue_keys[i]));
        }

        printf("\n");
        printf("********************************************************************************\n");
}

int main(int argc, char **argv) {
        if (sodium_init() == -1) {
                panic("sodium_init failed");
        }



        if ((getuid()) != 0)
                printf("You are not root! This may not work...\n");

        while (1) {
                int c = getopt_long(argc, argv, "r:d:s:k:n:vh", long_options, NULL);

                if (c < 0)
                        break;

                switch (c) {
                case 'r':
                        if (device_count >= MAX_INPUTS)
                                panic("Too many -r options: can read from at most %d devices\n", MAX_INPUTS);
                        strncpy(named_inputs[device_count++], optarg, BUFSIZE-1);
                        break;

                case 'd':
                        if ((max_delay = atoi(optarg)) < 0)
                                panic("Maximum delay must be >= 0\n");
                        break;

                case 's':
                        if ((startup_timeout = atoi(optarg)) < 0)
                                panic("Startup timeout must be >= 0\n");
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
                
                case 'n':
                        if((max_noise = atoi(optarg)) < 0)
                                panic("Maximum noise must be >= 0");
                        break;

                default:
                        usage();
                        panic("Unknown option: %c \n", optopt);
                }
        }


        

        // see if running in qubes
        if(system("stat /var/run/qubes 2> /dev/null > /dev/null") == 0) {
                is_qubes = 1;
        }



        
        // autodetect devices if none were specified
        if (device_count == 0)
                detect_devices();

        // autodetect failed
        if (device_count == 0) {
                // if in qubes, make sure qubes-input-sender service is running before panic
                if(is_qubes) {
                        restart_all_qubes_input_sender();
                }

                panic("Unable to find any keyboards or mice. Specify which input device(s) to use with -r");
        }

        // set rescue keys from the default sequence or -k arg
        set_rescue_keys(rescue_keys_str);

        // wait for pending events to finish, avoids keys being "held down"
        printf("Waiting %d ms...\n", startup_timeout);
        sleep_ms(startup_timeout);

        // open the input devices and create the output devices
        init_inputs();
        init_outputs();

        // initialize the event queue
        TAILQ_INIT(&head);

        banner();
        main_loop();

        // close everything
        for (int i = 0; i < device_count; i++) {
                libevdev_uinput_destroy(uidevs[i]);
                libevdev_free(output_devs[i]);
                close(input_fds[i]);



                // if in qubes, restart the associated qubes-input-sender service for each of the input devices
                if(is_qubes) {
                        change_qubes_input_sender("start", named_inputs[i]);
                }
        }

        exit(EXIT_SUCCESS);
}
