#include <stdio.h>

/* Counts how many characters are in the `fin` file */
int count(FILE * fin)
{
    int chars = 0;

    while (1) {
        int ch;

        ch = fgetc(fin); // get the next character into ch

        if (ch == EOF) {
            break; // if end of file was reached (ch == EOF), exit the loop
        }

        chars++; // increment chars
    }

    return chars;
}

int main(int argc, char *argv[])
{
    FILE *fin;

    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1; // check that argc is correct
    }

    fin = fopen(argv[1], "r"); // open file, using the filename from the 1st command argument

    if (fin == NULL) {
        printf("Error: Could not open file %s\n", argv[1]);
        return 1; // check that the file was opened successfully (fin != NULL)
    }
    
    int total_chars = count(fin); // call count with the opened file, and print the result
    printf("%d\n", total_chars);
    
    fclose(fin); // close the file

    return 0;
}
