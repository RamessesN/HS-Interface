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
	
	/* Set BCM #10, #11, #13 as output */
	gpio[1] &= 0xFFFFF1C0;
	gpio[1] |= 0x209;
	
	/* Set BCM #26 as input */
	gpio[2] &= 0xFFE3FFFF;
	
	gpio[10] = 0x2C00; // Clear: Turn off all lights
	sleep(1);
	
	while(1) {
		/* 1. RED ON */
		gpio[10] = 0x2C00; // Clear
		gpio[7] = 0x400;
		sleep(5);
		
		/* 2. RED ON, YELLOW ON */
		gpio[10] = 0x2C00; // Clear
		gpio[7] = 0xC00;
		sleep(1);
		
		/* 3. GREEN ON */
		gpio[10] = 0x2C00; // Clear
		// gpio[10] = 0xC00; // Red off, Yellow off
		gpio[7] = 0x2000; // Green on
		
		/* 4. PAUSE OR BUTTON PRESS */
		for (int i = 0; i < 100; i++) {
			if ((gpio[13] & 0x4000000) != 0) {
				break;
			}
			usleep(50000);
		}
		
		/* 5. YELLOW BLINKING */
		gpio[10] = 0x2C00; // Clear
		// gpio[10] = 0x2000; // Green off
		
		for (int i = 0; i < 3; i++) {
			gpio[7] = 0x800; // Yellow on
			usleep(500000);
			
			gpio[10] = 0x800; // Yellow off
			usleep(500000);
		}
	}
	
	return 0;
}
