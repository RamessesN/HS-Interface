/**
 * F28HS CW2
 * PinCrack: button input of a sequence of numbers followed by cracking a secret pin
 * Uses interval timers for the timeout/delay function

 * Compile:    	      make cw2
 * Run (e.g):         sudo ./cw2 -d -e -s 112
 * Run (unit-test):   sudo ./cw2 -u -s 112 -r 121

 ***********************************************************************
 * The development of this code was heavily based on the wiringPi library by Gordon Henderson.
 * This instance of the code, however, does not depend directly on the wiringPi library any more.
 *
 * wiringPi:
 *	Arduino look-a-like Wiring library for the Raspberry Pi
 *	Copyright (c) 2012-2015 Gordon Henderson
 *	Additional code for pwmSetClock by Chris Hall <chris@kchall.plus.com>
 *
 *	Thanks to code samples from Gert Jan van Loo and the
 *	BCM2835 ARM Peripherals manual, however it's missing
 *	the clock section /grr/mutter/
 ***********************************************************************
 * This file is part of wiringPi:
 *	https://projects.drogon.net/raspberry-pi/wiringpi/
 *
 *    wiringPi is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as
 *    published by the Free Software Foundation, either version 3 of the
 *    License, or (at your option) any later version.
 *
 *    wiringPi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with wiringPi.
 *    If not, see <http://www.gnu.org/licenses/>.
 */

/* Config settings */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>

#include "cw2-config.h"
#include "cw2-aux.h"
#include "lcd-binary.h"
#include "lcd-fcts.h"

#define USEC_PER_SEC        1000000ULL // microseconds per second
#define USEC_PER_MSEC       1000ULL    // microseconds per miliseconds
#define SEC(x)              ((x) * USEC_PER_SEC)
#define MSEC(x)             ((x) * USEC_PER_MSEC)
#define POLL_DELAY_US       MSEC(10)
#define DEBOUNCE_DELAY_US   MSEC(50)
#define LCD_INIT_DELAY      MSEC(5)

#define LOG(s)              printf("%s", (s))
#define LOGF(fmt, ...)      printf(fmt, __VA_ARGS__)
#define ERROR(s)            fprintf(stderr, "%s", (s))
#define ERRORF(fmt, ...)    fprintf(stderr, fmt, __VA_ARGS__)

#define IS_VOWEL(c) ( \
    tolower((unsigned char)(c)) == 'a' || \
    tolower((unsigned char)(c)) == 'e' || \
    tolower((unsigned char)(c)) == 'i' || \
    tolower((unsigned char)(c)) == 'o' || \
    tolower((unsigned char)(c)) == 'u' )

/* Constants */
static int  digits = DIGITS;     // number of possible values per digit
static int  seqlen = SEQL;       // length of the pin sequence
static int* theSeq = NULL;   // secret pin sequence

volatile unsigned int gpiobase;  // base address of GPIO memory
volatile uint32_t *gpio ;        // pointer to mapped GPIO memory

static volatile sig_atomic_t timeout_flag = 0;     // flag set by timer interrupt when timeout occurs

const int IDLE_THRESHOLD = 5000000 / POLL_DELAY_US; // interval of timeout warning

typedef struct {
    int attempts;
    int submits;
    int found;
    int found_at;
    int *found_seq;
} SearchStats; // used to transfer parameters

#ifdef HAMM_ASM
// prototype for the Assembler fct; only needed for an Asm implementation
int hamming(const int *x, const int *y, int seqlen);
#endif

/**
 * @brief Get current timestamp in microseconds.
 * 
 * @return uint64_t Current time in microseconds.
 */
uint64_t timeInMicroseconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * USEC_PER_SEC + (uint64_t)tv.tv_usec;
}

/**
 * @brief Signal handler for interval timer.
 * 
 * @param signum Signal number.
 */
void timer_handler(int signum) {
    (void)signum;
    timeout_flag = 1;
}

/**
 * @brief Initialize the interval timer for timeout detection.
 * 
 * @param timeout Timeout value in microseconds.
 */
void initITimer(uint64_t timeout) {
    struct itimerval it_val;
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = timer_handler;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction failed");
        exit(EXIT_FAILURE);
    }

    it_val.it_value.tv_sec  = timeout / USEC_PER_SEC;
    it_val.it_value.tv_usec = timeout % USEC_PER_SEC;

    it_val.it_interval.tv_sec  = 0;
    it_val.it_interval.tv_usec = 0;

    // start timer
    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        perror("error calling setitimer()");
        exit(1);
    }
}

/**
 * @brief Initialise the secret pin sequence with random values.
 * 
 * @param seqlen Length of the sequence.
 * @param digits Maximum value per digit.
 */
bool initSeq(int seqlen, int digits) {
    unsigned long value, r;

    if (theSeq == NULL) {
        return false;
    }

    srand((unsigned int) time(NULL));

    for (int i = 0; i < seqlen; i++) {
        r = rand();
        value = (r % digits) + 1;
        theSeq[i] = value;
    }

    return true;
}

/**
 * @brief Print a sequence to stdout.
 * 
 * @param seq       Sequence array.
 * @param seqlen    Length of sequence.
 */
void showSeq(const int *seq, int seqlen) {
    LOGF("Contents of the sequence (of length %d): ", seqlen);
    for (int i = 0; i < seqlen; i++) {
        LOGF(" %d", seq[i]);
    }
    LOG("\n");
}

/**
 * @brief Convert integer into digit sequence.
 * 
 * @param seq       Output sequence array.
 * @param seqlen    Length of sequence.
 * @param val       Integer to parse.
 */
bool readSeq(int *seq, int seqlen, int val) {
    char valStr[32];
    size_t strLen;

    snprintf(valStr, sizeof(valStr), "%d", val);
    strLen = strlen(valStr);

    if ((int)strLen > seqlen) {
        ERROR("Input sequence is too long\n");
        return false;
    }

    for (int i = 0; i < (int)strLen; i++) {
        int digit = valStr[i] - '0';

        if (digit < 1 || digit > digits) {
            ERRORF("Invalid digit %d (allowed range: 1-%d)\n", digit, digits);
            return false;
        }

        seq[i] = digit;
    }

    for (int i = strLen; i < seqlen; i++) {
        seq[i] = 1;
    }

    return true;
}

/**
 * @brief Write value to LED pin.
 * 
 * @param gpio  GPIO base pointer.
 * @param pin   Pin number.
 * @param value HIGH or LOW.
 */
static inline void write_LED(volatile uint32_t *gpio, int pin, int value) {
  digital_write (gpio, pin, value);
}

/**
 * @brief Blink LED multiple times.
 * 
 * @param gpio  GPIO base pointer.
 * @param led   LED pin.
 * @param c     Number of blinks.
 */
void blinkN(volatile uint32_t *gpio, int led, int c) {
    for (int i = 0; i < c; i++) {
        write_LED(gpio, led, HIGH);
        usleep(DELAY);
        write_LED(gpio, led, LOW);
        usleep(DELAY);
    }
}

#ifndef HAMM_ASM
/**
 * @brief Compute the Hamming distance between two integer sequences.
 *
 * The Hamming distance is the number of digits at which the corresponding elements of the two sequences are different.
 *
 * @param x       Pointer to the 1st integer sequence.
 * @param y       Pointer to the 2nd integer sequence.
 * @param seqlen  Length of both sequences.
 *
 * @return int    The Hamming distance between the two sequences.
 */
int hamming(const int *x, const int *y, int seqlen) {
    int distance = 0;

    for (int i = 0; i < seqlen; i++) {
        if (x[i] != y[i]) {
            distance++;
        }
    }
    return distance;
}
#endif

/**
 * @brief Print the Hamming distance between two sequences.
 *
 * This prints the computed Hamming distance to the terminal.
 *
 * @param code  The computed Hamming distance.
 * @param seq1  Pointer to the 1st sequence (unused).
 * @param seq2  Pointer to the 2nd sequence (unused).
 */
void showHamm(int code, const int *seq1, const int *seq2) {
    (void)seq1;
    (void)seq2;

    LOGF("Hamming Distance between the two sequences is: %d\n", code);
}

/**
 * @brief Increase a sequence composed of multiple numbers by 1.
 * 
 * This is to add 1 to a sequence of multiple numbers, taking into account the carry case.
 * 
 * @param seq     Pointer to the sequence array to be incremented.
 * @param seqlen  Length of the sequence.
 * @param digits  Maximum value per digit (base).
 */
static inline void incseq(int *seq, int seqlen,  int digits) {
    for (int i = seqlen - 1; i >= 0; i--) {
        seq[i]++;

        if (seq[i] <= digits) {
            break;
        }

        seq[i] = 1;
    }
}

/**
 * @brief Submit a possible pin sequence for verification.
 * 
 * This function compares the attempted sequence against the secret sequence by computing the Hamming distance.
 * 
 * @param attSeq       Pointer to the attempted pin sequence.
 * @param seqlen       Length of the sequence.
 * @param submitDelay  Artificial delay (in microseconds) before evaluation.
 * 
 * @return int Returns the Hamming distance between the attempted sequence and the secret sequence.
 */
int submit_pin(const int *attSeq, int seqlen, int submitDelay) {
    usleep(submitDelay); // simulating a slow submit action
    return hamming(theSeq, attSeq, seqlen);
}

/**
 * @brief Generates different digit combinations based on known varying positions (Task 5 inner recursion)
 * 
 * @param depth        Current recursion depth.
 * @param code         Target Hamming distance.
 * @param positions    Array storing the index positions that need to be modified.
 * @param guess_seq    Buffer used to store the currently generated guess sequence.
 * @param input_seq    The initial reference input sequence.
 * @param digits       Maximum allowed value for each digit.
 * @param seqlen       Total length of the PIN sequence.
 * @param submitDelay  Simulated delay time in microseconds for submission verification.
 * @param stats         Pointer to structure storing search statistics and results.
 * @param exhaustive   Boolean flag indicating whether to perform an exhaustive search.
 */
void generate_values(
    int depth,
    int code,
    int *positions,
    int *guess_seq,
    int *input_seq,
    int digits,
    int seqlen,
    int submitDelay,
    SearchStats *stats,
    bool exhaustive
) {
    if (stats->found && !exhaustive)
        return;

    if (depth == code) {
        stats->attempts++;
        stats->submits++;

        int result = submit_pin(guess_seq, seqlen, submitDelay);

        if (result == 0) {
            stats->found = 1;

            if (stats->found_at == 0) {
                stats->found_at = stats->attempts;

                memcpy(stats->found_seq, guess_seq, seqlen * sizeof(int));

                LOG("PIN found: ");
                showSeq(guess_seq, seqlen);   
            }
        }

        return;
    }

    int pos = positions[depth];
    int original = input_seq[pos];

    for (int v = 1; v <= digits; v++) {
        if (v == original)
            continue;

        guess_seq[pos] = v;

        generate_values(
            depth + 1,
            code,
            positions,
            guess_seq,
            input_seq,
            digits,
            seqlen,
            submitDelay,
            stats,
            exhaustive
        );

        guess_seq[pos] = original;

        if (stats->found && !exhaustive)
            return;
    }
}

/**
 * @brief Selects the positions to be modified from the sequence (Task 5 outer recursion)
 * 
 * @param depth        Current recursion depth.
 * @param start        Current starting index for selection.
 * @param code         Target Hamming distance.
 * @param seqlen       Total length of the PIN sequence.
 * @param positions    Array used to store the selected index positions.
 * @param guess_seq    Buffer for the guess sequence passed down to the inner function.
 * @param input_seq    The initial reference input sequence.
 * @param digits       Maximum allowed value for each digit.
 * @param submitDelay  Simulated delay time in microseconds for submission verification.
 * @param stats        Pointer to structure storing search statistics and results.
 * @param exhaustive   Boolean flag indicating whether to perform an exhaustive search.
 */
void choose_positions(
    int depth,
    int start,
    int code,
    int seqlen,
    int *positions,
    int *guess_seq,
    int *input_seq,
    int digits,
    int submitDelay,
    SearchStats *stats,
    bool exhaustive
) {
    if (stats->found && !exhaustive)
        return;

    if (depth == code) {
        generate_values(
            0,
            code,
            positions,
            guess_seq,
            input_seq,
            digits,
            seqlen,
            submitDelay,
            stats,
            exhaustive
        );

        return;
    }

    for (int i = start; i < seqlen; i++) {
        positions[depth] = i;

        choose_positions(
            depth + 1,
            i + 1,
            code,
            seqlen,
            positions,
            guess_seq,
            input_seq,
            digits,
            submitDelay,
            stats,
            exhaustive
        );

        if (stats->found && !exhaustive)
            return;
    }
}

/**
 * @brief Main func.
 * 
 * @param argc Argument count.
 * @param argv Argument vector.
 * 
 * @return int Exit status.
 */
int main(int argc, char **argv){
    uint64_t search_space;
    int code = 0, refCode = 0;

    int ret = EXIT_SUCCESS;

    // count the number of comparisons in total, found after how many attempts, total submits
    SearchStats stats = {
        .attempts = 0,
        .submits = 0,
        .found = 0,
        .found_at = 0,
        .found_seq = NULL
    };

    int *attemptSeq = NULL, *ref_seq = NULL; 
    uint64_t start_time, stop_time;
    
    int *guess_seq = NULL, *positions = NULL, *bf_seq = NULL;

    int pin_led_green = LED, pin_led_red = LED2, pin_button = BUTTON; // variables holding pin numbers for LEDs and button
    int fd = -1; // int fSel, shift, pin, clrOff, setOff, off, res;

    gpio = NULL;

    // strings for temporary usage (e.g. writing to LCD display)
    // char str1[32];
    // char str2[32];

    // useful for interval timers
    // struct timeval t1, t2;

    // variables for command-line processing
    // command-line options
    bool opt_e = false, opt_l = false;
    int opt_m = 0, opt_n = 0, opt_S = 0, opt_s = 0, opt_r = 0;

    // variables derived from command line options
    bool verbose = false, help = false, debug = false, unit_test = false;

    int submitDelay = SUBMIT_DELAY;
    int task_mode = 5;

    // process command-line arguments
    {
        int opt;
        while ((opt = getopt(argc, argv, "hvdeluS:s:r:m:n:t:")) != -1) {
            switch (opt) {
                case 'v': verbose = true;                            break;
                case 'h': help = true;                               break;
                case 'd': debug = true;                              break;
                case 'e': opt_e = true;                              break;
                case 'l': opt_l = true;                              break; // LCD test only
                case 'u': unit_test = true;                          break;
                case 'S': opt_S = atoi(optarg); submitDelay = opt_S; break;
                case 's': opt_s = atoi(optarg);                      break;
                case 'r': opt_r = atoi(optarg);                      break;
                case 'm': opt_m = atoi(optarg); if (opt_m < 1) { 
                        ERRORF("Digits must be at least 1 (got %d)\n", opt_m);
                        ret = EXIT_FAILURE;
                        goto cleanup;
                    } digits = opt_m;                                break;
                case 'n': opt_n = atoi(optarg); seqlen = opt_n;      break;
                case 't': task_mode = atoi(optarg);                  break; // task 4/5
                default: ERRORF(
                    "Usage: %s [-h] [-v] [-d] [-e] [-m <maxval> ] [-n <seqlen>] [-u <seq1> <seq2>] [-s <secret seq>] [-r <reference seq>]\n", 
                    argv[0]
                ); 

                ret = EXIT_FAILURE;
                goto cleanup;
            }
        }
    }

    if (help) {
        ERROR("PinCrack program, running on a Raspberry Pi, with connected LED, button and LCD display\n"); 
        ERROR("Use the button for input of numbers. The LCD display will show the matches with the secret sequence.\n"); 
        ERROR("For full specification of the program see: https://www.macs.hw.ac.uk/~hwloidl/Courses/F28HS/F28HS_CW2_2026.pdf\n"); 
        ERRORF("Usage: %s [-h] [-v] [-d] [-e] [-u <seq1> <seq2>] [-s <secret seq>] [-r <reference seq>]  \n", argv[0]);

        ret = EXIT_SUCCESS;
        goto cleanup;
    }

    if (verbose) {
        LOG("Settings for running the program\n");
        LOGF("Verbose is %s\n", (verbose ? "ON" : "OFF"));
        LOGF("Debug is %s\n", (debug ? "ON" : "OFF"));
        LOGF("Unittest is %s\n", (unit_test ? "ON" : "OFF"));
        LOGF("Exhaustive search is %s\n", (opt_e ? "ON" : "OFF"));
        LOGF("Submit delay is %d\n", submitDelay);
        LOGF("Task mode is %d\n", task_mode);

        if (opt_s) LOGF("Secret sequence set to %d\n", opt_s);
        if (opt_r) LOGF("Reference sequence set to %d\n", opt_r);
    }

    if (verbose) {
        LOGF("Hint: remember to compute the Hamming distance in each iteration and assign it to variable code; current (unused) value: %d\n", code);
        LOGF("Code style requirement: collect the values of the input sequence in the variable attemptSeq; current (unused) value: %p\n", attemptSeq);
    }

    if (opt_s) { // if `-s` option is given, use the sequence as SECRET sequence
        theSeq = calloc(seqlen, sizeof(int));

        if (theSeq == NULL) {
            ERROR("calloc failed\n");

            ret = EXIT_FAILURE;
            goto cleanup;
        }

        if (!readSeq(theSeq, seqlen, opt_s)) {
            ret = EXIT_FAILURE;
            goto cleanup;
        }
    }

    if (opt_r) { // if `-r` option is given, use the sequence as REFERENCE sequence
        ref_seq = calloc(seqlen, sizeof(int));

        if (ref_seq == NULL) {
            ERROR("calloc failed\n");

            ret = EXIT_FAILURE;
            goto cleanup;
        }

        if (!readSeq(ref_seq, seqlen, opt_r)) {
            ret = EXIT_FAILURE;
            goto cleanup;
        }
    }

    /* Configuration of the LCD display */
    int bits, rows, cols ;

    // hard-coded: 16x2 display, using a 4-bit connection
    bits = 4;
    cols = 16;
    rows = 2;

    LOGF("Raspberry Pi configuration: red LED: %d; green LED: %d; button: %d\n", pin_led_red, pin_led_green, pin_button);
    LOGF("Raspberry Pi LCD driver for a %dx%d display (%d-bit wiring) \n", cols, rows, bits);

    /* Check for root priveleges (needed for controlling LEDs etc) */
    if (geteuid() != 0) {
        ERROR("setup: Must be root. (Did you forget sudo?)\n");
        
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* constants for RPi2/3. NOTE: RPi4 needs a different base address */
    // RPi2/3
    // gpiobase = 0x3F200000;
    // RPi4
    gpiobase = 0xFE200000;
    
    // memory mapping 
    // Open the master /dev/memory device
    fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);

    if (fd < 0) {
        ERRORF("setup: Unable to open /dev/mem: %s\n", strerror(errno));

        ret = EXIT_FAILURE;
        goto cleanup;
    }

    // GPIO:
    gpio = mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, gpiobase);
    if (gpio == MAP_FAILED) {
        ERRORF("setup: mmap(GPIO) failed: %s\n", strerror(errno));

        gpio = NULL;

        ret = EXIT_FAILURE;
        goto cleanup;
    }

    // Setting mode of pins
    pin_mode(gpio, pin_led_green, OUTPUT);
    pin_mode(gpio, pin_led_red, OUTPUT);
    pin_mode(gpio, pin_button, INPUT);

    pin_mode(gpio, LCD_PIN_RS, OUTPUT);
    pin_mode(gpio, LCD_PIN_E,  OUTPUT);
    pin_mode(gpio, LCD_PIN_D4, OUTPUT);
    pin_mode(gpio, LCD_PIN_D5, OUTPUT);
    pin_mode(gpio, LCD_PIN_D6, OUTPUT);
    pin_mode(gpio, LCD_PIN_D7, OUTPUT);

    // Initialize the LCD display
    lcd_init(gpio);
    lcd_clear(gpio);

    usleep(LCD_INIT_DELAY);

    if (opt_l) {
        lcd_write_row(gpio, 1, "Hello, World");
        lcd_write_row(gpio, 0, "Good bye, World");

        ret = 2;
        goto cleanup;
    }

    if (!opt_s) {
        theSeq = calloc(seqlen, sizeof(int));

        if (theSeq == NULL) {
            ERROR("calloc failed\n");
            
            ret = EXIT_FAILURE;
            goto cleanup;
        }

        if (!initSeq(seqlen, digits)) { // Initialize the secret sequence
            ERROR("calloc failed\n");
            
            ret = EXIT_FAILURE;
            goto cleanup;
        }
    }

    // Use the debugging option like this for extra messages
    if (debug) {
        LOG("Secret sequence is: ");
        showSeq(theSeq,seqlen);
    }

    // Unit testing: check the Hamming distance between two given sequences
    if (unit_test) { // unit test: just print the Hamming distance
        if (!opt_r) {
            ERROR("Need to use both -s and -r for unit testing (with -u)\n");
            
            ret = EXIT_FAILURE;
            goto cleanup;
        }

        // output to terminal
        refCode = hamming(theSeq, ref_seq, seqlen);
        showSeq(theSeq,seqlen);
        showSeq(ref_seq,seqlen);
        showHamm(refCode, theSeq, ref_seq); 

        // output to LCD display
        lcd_clear(gpio);
        usleep(LCD_INIT_DELAY);
        lcd_write_row(gpio, 1, "Hamming dist:");

        char dist_str[16];
        snprintf(dist_str, sizeof(dist_str), "%d", refCode);
        lcd_write_row(gpio, 0, dist_str);

        ret = EXIT_SUCCESS;
        goto cleanup;
    }  

    /* Print Greetings Message on LCD display */
    const char *surname = "ZHAOY";
    char display_name[6] = {0};

    int len = strlen(surname);
    int limit = (len < 5) ? len : 5;
    strncpy(display_name, surname, limit);

    lcd_write_row(gpio, 1, "Greeting:");
    lcd_write_row(gpio, 0, display_name);

    for (int i = 0; i < limit; i++) {
        if (isalpha(display_name[i])) {
            if (IS_VOWEL(display_name[i])) {
                blinkN(gpio, pin_led_green, 1);
            } else {
                blinkN(gpio, pin_led_red, 1);
            }
        }
    }

    stats.found_seq = calloc(seqlen, sizeof(int));

    if (stats.found_seq == NULL) {
        ERROR("calloc failed\n");
        
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    if (!opt_r && !unit_test)
        waitForEnter(); // wait for `ENTER` key before continuing

    attemptSeq = calloc(seqlen, sizeof(int));
    if (attemptSeq == NULL) {
        ERROR("Memory allocation failed\n");

        ret = EXIT_FAILURE;
        goto cleanup;
    }

    if (!opt_r) {
        lcd_clear(gpio);
        usleep(LCD_INIT_DELAY);
        lcd_write_row(gpio, 1, "Enter PIN:");

        char input_display[17] = {0}; // demonstrate the input sequence

        // PHASE 1: sequence input
        // Iterate over all elements of the sequence
        for (int i = 0; i < seqlen; i++) {
            int press_count = 0; // number of presses for current digit
            timeout_flag = 0;    // reset timeout flag before input 

            LOGF("Please enter digit %d:\n", i + 1);

            // wait button released first
            while (read_button(gpio, pin_button) == HIGH) {
                usleep(POLL_DELAY_US);
            }

            int idle_counter = 0;

            // wait first press
            while (read_button(gpio, pin_button) == LOW) {
                usleep(POLL_DELAY_US);
                idle_counter++;

                if (idle_counter >= IDLE_THRESHOLD) {
                    LOGF("Please enter digit %d (allowed 1-%d).\n", i + 1, digits);
                    idle_counter = 0; // reset the counter
                }
            }

            initITimer(TIMEOUT); // constrain user press button within TIMEOUT

            while (!timeout_flag) {
                if (read_button(gpio, pin_button) == HIGH) {
                    write_LED(gpio, pin_led_green, HIGH);
                    press_count++;

                    while (read_button(gpio, pin_button) == HIGH) {
                        usleep(POLL_DELAY_US); // wait the user until leave the button
                    }

                    write_LED(gpio, pin_led_green, LOW);
                    initITimer(TIMEOUT);

                    usleep(DEBOUNCE_DELAY_US); // debounce delay
                }
                usleep(POLL_DELAY_US); // polling delay
            }

            if (press_count < 1 || press_count > digits) {
                LOGF("Invalid input: %d (allowed 1-%d). Re-input digit.\n", press_count, digits);

                blinkN(gpio, pin_led_red, 3);

                i--;
                continue;
            
            }
            attemptSeq[i] = press_count;
            LOGF("Digit %d recorded as: %d\n", i + 1, press_count);

            // add the latest inputted number to the LCD display and refresh
            char temp_char[16];
            snprintf(temp_char, sizeof(temp_char), "%d", press_count);
            strncat(input_display, temp_char, sizeof(input_display) - strlen(input_display) - 1);
            lcd_write_row(gpio, 0, input_display);

            blinkN(gpio, pin_led_red, 1); // red blink * 1
            blinkN(gpio, pin_led_green, press_count); // green blink * press_count
        }

        blinkN(gpio, pin_led_red, 2); // red blink * 2
    }

    // PHASE 2: Main Task: full search
    // Print the version of the code this is running; set values in `cw2-config.h`
    LOG("--------------------- \n");
    LOGF(">> Version %d: %s with %d digits and %d sequence length\n", VERSION, VERSION_STR, digits, seqlen);
#ifdef HAMM_ASM
    LOG(">> HAMM_ASM version: Hamming distance in ARM Assembler\n");
#else
    LOG(">> Hamming in C version\n");
#endif
    LOG("--------------------- \n");

    if (debug) {
        LOG("Debug mode\nThe secret sequence is:");
        showSeq(theSeq,seqlen);
    }

    start_time = timeInMicroseconds();
    int *input_seq = opt_r ? ref_seq : attemptSeq;

    if (task_mode == 4) { // Task 4
        LOG("Running Task 4 brute-force search\n");

        int target_code = submit_pin(input_seq, seqlen, submitDelay);
        stats.attempts++;
        stats.submits++;

        LOGF("Computed target Hamming distance: %d\n", target_code);

        search_space = 1;
        for (int i = 0; i < seqlen; i++) {
            search_space *= digits;
        }

        bf_seq = calloc(seqlen, sizeof(int));
        if (bf_seq == NULL) {
            ERROR("calloc failed\n");

            ret = EXIT_FAILURE;
            goto cleanup;
        }

        for (int i = 0; i < seqlen; i++) {
            bf_seq[i] = 1;
        }

        for (uint64_t n = 0; n < search_space; n++) {
            stats.attempts++;

            if (hamming(input_seq, bf_seq, seqlen) == target_code) {
                int result = submit_pin(bf_seq, seqlen, submitDelay);
                stats.submits++;

                if (result == 0) {
                    stats.found = 1;
                    if (stats.found_at == 0) { // `found_at` uses 1-based indexing
                        stats.found_at = stats.attempts;
                        memcpy(stats.found_seq, bf_seq, seqlen * sizeof(int));

                        LOG("PIN found: ");
                        showSeq(bf_seq, seqlen);
                    }
                }
                if (!opt_e && stats.found) break;
            }
            incseq(bf_seq, seqlen, digits);
        }
    } else { // Task 5
        LOG("Running Task 5 optimized search\n");

        code = submit_pin(input_seq, seqlen, submitDelay);
        stats.attempts++;
        stats.submits++;  // initial reference sequence submission counts as first attempt

        uint64_t comb = 1;

        for (int i = 0; i < code; i++) {
            comb = comb * (seqlen - i) / (i + 1);
        }

        search_space = 1;

        for (int i = 0; i < code; i++) {
            search_space *= (digits - 1);
        }

        search_space *= comb;
        if (code > 0) {
            search_space += 1; // include initial reference sequence submission
        }

        LOGF("Computed Hamming distance: %d\n", code);

        guess_seq = calloc(seqlen, sizeof(int));

        if (guess_seq == NULL) {
            ERROR("calloc failed\n");

            ret = EXIT_FAILURE;
            goto cleanup;
        }

        memcpy(guess_seq, input_seq, seqlen * sizeof(int));

        if (code > 0) {
            positions = calloc(code, sizeof(int));
            if (positions == NULL) {
                ERROR("calloc failed\n");

                ret = EXIT_FAILURE;
                goto cleanup;
            }
        }

        if (code == 0) {
            stats.found = 1;
            stats.found_at = stats.attempts;

            memcpy(stats.found_seq, input_seq, seqlen * sizeof(int));

            LOG("PIN found: ");
            showSeq(input_seq, seqlen);
        } else {
            choose_positions(
                0,
                0,
                code,
                seqlen,
                positions,
                guess_seq,
                input_seq,
                digits,
                submitDelay,
                &stats,
                opt_e
            );
        }
    }

    stop_time = timeInMicroseconds();

    LOGF("Runtime: %.6f secs\n", (double)(stop_time - start_time) / 1000000.0);
    LOGF("Sequence %s\n", stats.found ? "found" : "not found");

    double percentage = 0.0;

    if (stats.found_at > 0) {
        percentage = ((float)stats.found_at / (float)search_space) * 100.0f;
    }

    LOGF("%s search finished for %d digits and %d seqlen (expect %llu):\n"
        "%d attempts (found at %d i.e. %.2f %%), %d submits\n", 
        (opt_e ? "Exhaustive" : "Non-exhaustive"), 
        digits, 
        seqlen, 
        (unsigned long long)search_space, 
        stats.attempts, 
        stats.found_at, 
        percentage, 
        stats.submits);
    LOG("Secret sequence was: ");
    showSeq(theSeq, seqlen);

    lcd_clear(gpio);
    usleep(LCD_INIT_DELAY);

    if (stats.found) {
        blinkN(gpio, pin_led_green, 2); // green blinks twice to show a result found
        lcd_write_row(gpio, 1, "PIN found");
        
        char pin_str[17] = {0};
        char temp[12];

        for (int i = 0; i < seqlen; i++) {
            snprintf(temp, sizeof(temp), "%d", stats.found_seq[i]);
            strncat(pin_str, temp, sizeof(pin_str) - strlen(pin_str) - 1);
        }
        lcd_write_row(gpio, 0, pin_str);
    } else {
        lcd_write_row(gpio, 1, "PIN not found");
    }

cleanup:
    /* free memory */
    free(theSeq);
    free(ref_seq);
    free(attemptSeq);
    free(stats.found_seq);
    free(guess_seq);
    free(positions);
    free(bf_seq);

    if (gpio != NULL && gpio != MAP_FAILED) {
        write_LED(gpio, pin_led_green, LOW);
        write_LED(gpio, pin_led_red, LOW);
        munmap((void *)gpio, BLOCK_SIZE);
    }

    if (fd >= 0) {
        close(fd);
    }

    return ret;
}
