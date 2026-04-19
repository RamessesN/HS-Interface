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
static int* secret_seq = NULL;   // secret pin sequence

volatile unsigned int gpiobase;  // base address of GPIO memory
volatile uint32_t *gpio ;        // pointer to mapped GPIO memory

static volatile sig_atomic_t timeout_flag = 0;     // flag set by timer interrupt when timeout occurs

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
    timeout_flag = 1;
}

/**
 * @brief Initialize the interval timer for timeout detection.
 * 
 * @param timeout Timeout value in microseconds.
 */
void initITimer(uint64_t timeout) {
    struct itimerval it_val;

    // register signal handler for timer
    signal(SIGALRM, timer_handler); 

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
void initSeq(int seqlen, int digits) {
    unsigned long value, r;

    if (secret_seq == NULL) {
        secret_seq = calloc(seqlen, sizeof(int));
        if (secret_seq == NULL) {
            failure(true, "calloc failed");
        }
    }

    srand((unsigned int) time(NULL));
    for (int i = 0; i < seqlen; i++) {
        r = rand();
        value = (r % digits) + 1;
        secret_seq[i] = value;
    }
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
void readSeq(int *seq, int seqlen, int val) {
    char valStr[32];
    int i;
    size_t strLen;

    snprintf(valStr, sizeof(valStr), "%d", val);
    strLen = strlen(valStr);

    for (i = 0; i < seqlen && i < (int)strLen; i++) {
        seq[i] = valStr[i] - '0';
        if (seq[i] < 1 || seq[i] > digits) {
            seq[i] = 1;
        }
    }

    // pad with 1 values if necessary
    for (; i < seqlen; i++) {
        seq[i] = 1;
    }
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
 * @return int Returns 1 if the pin is correct, 0 otherwise.
 */
int submit_pin(const int *attSeq, int seqlen, int submitDelay) {
    int found = 0;

    // debugging only (needs additional arguments!):
    // showSeq(attSeq,seqlen);   
    // showHamm(code, ref_seq, attSeq);

    // submits++;         // now done at caller side
    usleep(submitDelay);  // simulating a slow submit action
    found = hamming(secret_seq, attSeq, seqlen) == 0;
    return found;
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
    int found = 0, code = 0, refCode = 0;

    // use these to count: number of comparisons in total, found after how many attempts, total number of submits
    int attempts = 0, found_at = 0, submits = 0;
    int *attempt_seq = NULL, *ref_seq = NULL; 
    double start_time, stop_time;

    
    int pin_led_green = LED, pin_led_red = LED2, pin_button = BUTTON; // variables holding pin numbers for LEDs and button
    int fd; // int fSel, shift, pin, clrOff, setOff, off, res;

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

    // process command-line arguments
    {
        int opt;
        while ((opt = getopt(argc, argv, "hvdeluS:s:r:m:n:")) != -1) {
            switch (opt) {
                case 'v': verbose = true;       break;
                case 'h': help = true;          break;
                case 'd': debug = true;         break;
                case 'e': opt_e = true;         break;
                case 'l': opt_l = true;         break; // LCD test only
                case 'u': unit_test = true;     break;
                case 'S': opt_S = atoi(optarg); submitDelay = opt_S; break;
                case 's': opt_s = atoi(optarg); break;
                case 'r': opt_r = atoi(optarg); break;
                case 'm': opt_m = atoi(optarg); digits = opt_m; break;
                case 'n': opt_n = atoi(optarg); seqlen = opt_n; break;
                default: fprintf(stderr, "Usage: %s [-h] [-v] [-d] [-e] [-m <maxval> ] [-n <seqlen>] [-u <seq1> <seq2>] [-s <secret seq>] [-r <reference seq>]  \n", argv[0]); exit(EXIT_FAILURE);
            }
        }
    }

    if (help) {
        ERROR("PinCrack program, running on a Raspberry Pi, with connected LED, button and LCD display\n"); 
        ERROR("Use the button for input of numbers. The LCD display will show the matches with the secret sequence.\n"); 
        ERROR("For full specification of the program see: https://www.macs.hw.ac.uk/~hwloidl/Courses/F28HS/F28HS_CW2_2026.pdf\n"); 
        ERRORF("Usage: %s [-h] [-v] [-d] [-e] [-u <seq1> <seq2>] [-s <secret seq>] [-r <reference seq>]  \n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    if (verbose) {
        LOG("Settings for running the program\n");
        LOGF("Verbose is %s\n", (verbose ? "ON" : "OFF"));
        LOGF("Debug is %s\n", (debug ? "ON" : "OFF"));
        LOGF("Unittest is %s\n", (unit_test ? "ON" : "OFF"));
        LOGF("Exhaustive search is %s\n", (opt_e ? "ON" : "OFF"));
        LOGF("Submit delay is %d\n", submitDelay);
        if (opt_s) LOGF("Secret sequence set to %d\n", opt_s);
        if (opt_r) LOGF("Reference sequence set to %d\n", opt_r);
    }

    if (verbose) {
        LOGF("Hint: remember to compute the Hamming distance in each iteration and assign it to variable code; current (unused) value: %d\n", code);
        LOGF("Code style requirement: collect the values of the input sequence in the variable attempt_seq; current (unused) value: %p\n", attempt_seq);
    }

    if (opt_s) { // if `-s` option is given, use the sequence as SECRET sequence
        if (secret_seq == NULL) {
            secret_seq = calloc(seqlen, sizeof(int));
            if (secret_seq == NULL) {
                failure(true, "calloc failed");
            }
        }
        readSeq(secret_seq, seqlen, opt_s);
        if (verbose) {
            fprintf(stderr, "Running program with secret sequence:\n");
            showSeq(secret_seq,seqlen);
        }
    }

    if (opt_r) { // if `-r` option is given, use the sequence as REFERENCE sequence
        if (ref_seq == NULL) {
            ref_seq = calloc(seqlen, sizeof(int));
            if (ref_seq == NULL) {
                failure(true, "calloc failed");
            }
        }
        readSeq(ref_seq, seqlen, opt_r);
        if (verbose) {
            ERROR("Running program with secret sequence:\n");
            showSeq(ref_seq,seqlen);
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
        exit(EXIT_FAILURE);
    }

    /* constants for RPi2/3. NOTE: RPi4 needs a different base address */
    // RPi2/3
    // gpiobase = 0x3F200000;
    // RPi4
    gpiobase = 0xFE200000;

    // memory mapping 
    // Open the master /dev/memory device
    if ((fd = open ("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC) ) < 0)
        return failure(false, "setup: Unable to open /dev/mem: %s\n", strerror(errno)) ;

    // GPIO:
    gpio = mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, gpiobase) ;
    if ((int32_t)gpio == -1)
        return failure(false, "setup: mmap (GPIO) failed: %s\n", strerror(errno)) ;

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
        lcd_write_row(gpio, 0, "Hello, World");
        lcd_write_row(gpio, 1, "Good bye, World");

        exit(2);
    }

    if (!opt_s) initSeq(seqlen, digits); // Initialize the secret sequence

    // Use the debugging option like this for extra messages
    if (debug) {
        LOG("Secret sequence is: ");
        showSeq(secret_seq,seqlen);
    }

    // Unit testing: check the Hamming distance between two given sequences
    if (unit_test) { // unit test: just print the Hamming distance
        if (!opt_r) {
            ERROR("Need to use both -s and -r for unit testing (with -u)\n");
            exit(EXIT_FAILURE);
        }

        // output to screen
        refCode = hamming(secret_seq, ref_seq, seqlen);
        showSeq(secret_seq,seqlen);
        showSeq(ref_seq,seqlen);
        showHamm(refCode, secret_seq, ref_seq); 
        exit(EXIT_SUCCESS);
    }  

    /* Print Greetings Message on LCD display */
    const char *surname = "ZHAO";
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

    waitForEnter(); // wait for `ENTER` key before continuing

    attempt_seq = (int *) calloc(seqlen, sizeof(int));
    if (attempt_seq == NULL) {
        failure(true, "Allocate memory for attempt_seq failed.");
    }

    // PHASE 1: sequence input
    // Iterate over all elements of the sequence
    for (int i = 0; i < seqlen; i++) {
        int press_count = 0; // number of presses for current digit
        timeout_flag = 0;    // reset timeout flag before input 

        LOGF("Please enter digit %d:\n", i + 1);

        while (read_button(gpio, pin_button) == LOW) {
            usleep(POLL_DELAY_US); // polling delay
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

        if (press_count > digits) press_count = digits;
        attempt_seq[i] = press_count;
        LOGF("Digit %d recorded as: %d\n", i + 1, press_count);

        blinkN(gpio, pin_led_red, 1); // red blink * 1
        blinkN(gpio, pin_led_green, press_count); // green blink * press_count
    }

    blinkN(gpio, pin_led_red, 2); // red blink * 2

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
        showSeq(secret_seq,seqlen);
    }

    // calculate the total range of possible sequences
    unsigned long bound = powl(digits, seqlen);

    // time-stamp
    start_time = clock();

    int *input_seq = opt_r ? ref_seq : attempt_seq;

    code = hamming(secret_seq, input_seq, seqlen);

    int *guess_seq = (int *) calloc(seqlen, sizeof(int));
    for (int i = 0; i < seqlen; i++) {
        guess_seq[i] = 1;
    }

    for (unsigned long i = 0; i < bound; i++) {
        attempts++;

        if (hamming(guess_seq, input_seq, seqlen) == code) {
            submits++;

            if (submit_pin(guess_seq, seqlen, submitDelay)) {
                found = 1;
                found_at = attempts;
                break;
            }
        }

        incseq(guess_seq, seqlen, digits);
    }

    stop_time = clock();

    LOGF("Runtime; %f secs\n", (stop_time - start_time) / CLOCKS_PER_SEC);
    LOGF("Sequence %s\n", found ? "found" : "not found");
    LOGF("%s search finished for %d digits and %d seqlen (expect %ld):\n%d attempts (found at %d i.e. %.2f %%), %d submits\n", (opt_e ? "Exhaustive" : "Non-exhaustive"), digits, seqlen, bound, attempts, found_at, (float)found_at / ((float)bound / 100.0), submits);
    LOG("Secret sequence was: ");
    showSeq(secret_seq, seqlen);

    lcd_clear(gpio);
    usleep(LCD_INIT_DELAY);
    
    if (found) {
        lcd_write_row(gpio, 0, "Pin found");
        char found_str[16];
        snprintf(found_str, 16, "In %d submits", submits);
        lcd_write_row(gpio, 1, found_str);
    } else {
        lcd_write_row(gpio, 0, "Pin not found");
    }

    /* free memory */
    free(secret_seq);
    free(ref_seq);
    free(attempt_seq);
    free(guess_seq);

    return 0;
}
