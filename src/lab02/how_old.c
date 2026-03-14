#include <stdio.h>
#include <time.h>

int main(int argc, char *argv[])
{
    // int current_year = 2024;
    int birth_year;
    int had_birthday; // used as boolean

    int current_year = 1970 + (time(NULL) / (365 * 24 * 60 * 60)); 
    
    printf("What year were you born in? ");
    scanf("%d", &birth_year); // ask what year the user was born in

    printf("Have you had your birthday yet this year? (1 for yes, 0 for no): ");
    scanf("%d", &had_birthday); // ask whether the user has had their birthday yet this year

    int age = current_year - birth_year; // compute and display the user's age
    
    if(had_birthday == 0) {
        age -= 1;
    }
    
    printf("You are %d years old.\n", age);
    
    return 0;
}
