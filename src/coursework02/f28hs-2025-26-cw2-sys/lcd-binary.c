#include <stdio.h> // debugging only
#include "gpio.h"
#include "lcd-binary.h"
#include "cw2-aux.h"

#ifndef GPIO_GPLEV0
#define GPIO_GPLEV0 13 // GPIO Pin Level Register 0 offset
#endif

/**
 * @brief Configure the mode of a GPIO pin.
 *
 * This function sets the function select bits of a given GPIO pin using inline ARM assembly. Each GPIO pin is controlled by 3 bits within a GPFSEL register. The function clears the existing bits and sets the new mode.
 *
 * @param gpio  Pointer to the base address of the GPIO register map.
 * @param pin   GPIO pin number.
 * @param mode  Pin mode (Input/Output).
 */
void pin_mode(volatile uint32_t *gpio, int pin, int mode) {
    volatile uint32_t *fsel_reg = gpio + (pin / 10);
    int fsel_shift = (pin % 10) * 3;

    __asm__ volatile (
        "MOV r2, %[reg] \n\t"       // load target register addr
        "LDR r1, [r2] \n\t"         // read curr register val
        "MOV r3, #7 \n\t"           // mask for 3 bits (0b111)
        "LSL r3, r3, %[shift] \n\t"
        "BIC r1, r1, r3 \n\t"       // clear existing func bits
        "MOV r3, %[mode] \n\t"
        "LSL r3, r3, %[shift] \n\t"
        "ORR r1, r1, r3 \n\t"       // set new mode
        "STR r1, [r2] \n\t"         // write back to reg
        :
        : [reg] "r" (fsel_reg), [shift] "r" (fsel_shift), [mode] "r" (mode)
        : "r1", "r2", "r3", "memory"
    );
}

/**
 * @brief Write a digital value to a GPIO pin.
 *
 * This function sets/clears a GPIO pin by writing to the GPSET/GPCLR register using inline ARM assembly. Writing a `1` to the corresponding bit position will set/clear the pin.
 *
 * @param gpio   Pointer to the base address of the GPIO register map.
 * @param pin    GPIO pin number.
 * @param value  Value to write (HIGH or LOW).
 */
void digital_write (volatile uint32_t *gpio, int pin, int value) {
    int reg_offset = (value == HIGH) ? GPIO_GPSET0 : GPIO_GPCLR0;

    volatile uint32_t *setclr_reg = gpio + reg_offset + (pin / 32);
    int bit_shift = pin % 32;

    __asm__ volatile (
        "MOV r2, %[reg] \n\t"       // load target register addr
        "MOV r1, #1 \n\t"
        "LSL r1, r1, %[shift] \n\t" // create bit mask
        "STR r1, [r2] \n\t"         // write to GPSET/GPCLR
        :
        : [reg] "r" (setclr_reg), [shift] "r" (bit_shift)
        : "r1", "r2", "memory"
    );
}

/**
 * @brief Read the digital state of the button GPIO pin.
 *
 * This function reads the level of a GPIO pin by accessing the GPLEV register. The relevant bit is shifted to the least significant position and masked.
 *
 * @param gpio    Pointer to the base address of the GPIO register map.
 * @param button  GPIO pin number connected to the button.
 *
 * @return int    Returns HIGH-1 if the pin is set, LOW-0 otherwise.
 */
int read_button(volatile uint32_t *gpio, int button) {
    volatile uint32_t *lev_reg = gpio + GPIO_GPLEV0 + (button / 32);
    int bit_shift = button % 32;
    int pin_state;

    __asm__ volatile (
        "MOV r2, %[reg] \n\t"       // load target register addr
        "LDR r1, [r2] \n\t"         // read curr register val
        "LSR r1, r1, %[shift] \n\t" // shift target bit to LSB
        "AND %[res], r1, #1 \n\t"   // mask least significant bit
        : [res] "=r" (pin_state)
        : [reg] "r" (lev_reg), [shift] "r" (bit_shift)
        : "r1", "r2", "memory"
    );
    
    return pin_state;
}
