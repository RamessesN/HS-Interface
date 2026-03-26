#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

/* The base address of the GPIO peripheral (ARM Physical Address) varies
 * between Raspberry Pi versions. This is for the Raspberry Pi 4. */
#define GPIO_BASE       0xFE200000UL
#define GPIO_LENGTH     (4*1024)

#define GPIO_GPFSEL0    0
#define GPIO_GPFSEL1    1
#define GPIO_GPFSEL2    2
#define GPIO_GPFSEL3    3
#define GPIO_GPFSEL4    4
#define GPIO_GPFSEL5    5

#define GPIO_GPSET0     7
#define GPIO_GPSET1     8

#define GPIO_GPCLR0     10
#define GPIO_GPCLR1     11

/* The registers and bits used to control the LED GPIO pin.
 * See lecture04e-reading-gpio.pdf for details.
 */
#define LED_GPFSEL      GPIO_GPFSEL1
#define LED_GPFBIT      6
#define LED_GPSET       GPIO_GPSET0
#define LED_GPCLR       GPIO_GPCLR0
#define LED_GPIO_BIT    12

/* Pointer to the mapped GPIO registers */
volatile uint32_t *gpio;

int main(int argc, char *argv[])
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        printf("can't open /dev/mem\n");
        return 1;
    }

    gpio = mmap(NULL, GPIO_LENGTH, PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, GPIO_BASE);
    if (gpio == (void *) -1) {
        printf("can't mmap\n");
        return 1;
    }

    /* Enable the LED GPIO pin as an output */
    gpio[LED_GPFSEL] |= 1 << LED_GPFBIT;

    /* This loop runs forever */
    while (1) {
        /* Set the LED GPIO pin low */
        gpio[LED_GPCLR] = 1 << LED_GPIO_BIT;

        sleep(1);

        /* Set the LED GPIO pin high */
        gpio[LED_GPSET] = 1 << LED_GPIO_BIT;

        sleep(1);
    }
}
