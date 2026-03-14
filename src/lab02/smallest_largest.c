#include <stdio.h>

int main(int argc, char *argv[])
{
    int i;

    printf("Enter an integer (0 to stop): ");
    scanf("%d", &i); // read integer as an int

    int smallest = i;
    int largest = i;

    while (1) {
        if (i == 0) {
            break; // loop forever until '0' is typed in
        }
        
        if (i < smallest) {
            smallest = i; // if new number is smaller than smallest, update smallest
        }
        
        if (i > largest) {
            largest = i; // if new number is larger than largest, update largest
        }

        scanf("%d", &i); // read the next number as an int into i
    }

    printf("smallest: %d, largest: %d\n", smallest, largest);

    return 0;
}
