#ifndef KLOAK_H_
#define KLOAK_H_


void sleep_ms(long );
long current_time_ms(void);
long int random_between(long int, long int);
void set_rescue_keys(char* );
int supports_event_type(int , int );
int supports_specific_key(int, unsigned int);
int is_keyboard(int );
int is_mouse(int );
void get_device_number(char *, char *);
int change_qubes_input_sender(char *, char *);
void restart_all_qubes_input_sender();
void detect_devices();
void cleanup_device(char * );
void init_new_input(char * );
void init_inputs();
void init_outputs();


void main_loop();
void usage();

void banner();
void init_new_input(char *);
void cleanup_device(char *);
void print_device_name(char *);



#endif
