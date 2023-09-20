#ifndef KLOAK_H
#define KLOAK_H


struct entry {
        struct input_event iev;
        long time;
        TAILQ_ENTRY(entry) entries;
        int device_index;
};

void sleep_ms(long int);
long current_time_ms(void);
long int random_between(long int, long int);
void set_rescue_keys(const char*);
int supports_event_type(int, int);
int supports_specific_key(int, unsigned int);
int is_keyboard(int);
int is_mouse(int);
void detect_devices();
void init_inputs();
void init_outputs();
void emit_event(struct entry *);
void main_loop();
void usage();
void banner();



#endif
