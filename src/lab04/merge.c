#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc != 4) {
        printf("Usage %s <file1> <fil2> <outfile>\n", argv[0]);
        return 1; // check the right number of arguments have been given
    }
    
    // Open the two input files
    FILE *fin1 = fopen(argv[1], "r");
    FILE *fin2 = fopen(argv[2], "r");

    // Open the output file
    FILE *fout = fopen(argv[3], "w");

    if (fin1 == NULL || fin2 == NULL || fout == NULL) {
        printf("Error: Could not open one or more files.\n");
        return 1; // check all the files have been opened successfully */
    } 

    // Read the first value from both input files
    int val1, val2;
    int scan1 = fscanf(fin1, "%i", &val1);
    int scan2 = fscanf(fin2, "%i", &val2);

    while (1) {
        if (scan1 == EOF) {
            /* End of file 1 */
            while (scan2 != EOF) {
                fprintf(fout, "%d ", val2);
                scan2 = fscanf(fin2, "%i", &val2); // copy the rest of file 2 into the output file
            }

            break;
        }

        if (scan2 == EOF) {
            /* End of file 2 */
            while (scan2 != EOF) {
                fprintf(fout, "%d", val1);
                scan1 = fscanf(fin1, "%i", &val1); // copy the rest of file 1 into the output file
            }

            break;
        }

        if (val1 < val2) {
            /* val1 is smaller */
            fprintf(fout, "%d ", val1);
            scan1 = fscanf(fin1, "%i", &val1); // write val1 to the output file, read the next value from file 1 (setting scan1)
        } else {
            /* val2 is smaller, or the two values are equal */
            fprintf(fout, "%d ", val2);
            scan2 = fscanf(fin2, "%i", &val2); // write val2 to the output file, read the next value from file 2 (setting scan2)
        }
    }

    /* Close the files */
    fclose(fin1);
    fclose(fin2);
    fclose(fout);

    return 0;
}
