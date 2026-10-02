#include <limits.h>
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char *argv[])
{
    long n;
    char **info = 0;
    if(argc >= 1) {
        for(int i = 0; i < argc; ++i) {
            printf("%s \n", argv[i]);
        }
        if((n = strtol(argv[1], info, 10)) == LONG_MAX ||
                (n = strtol(argv[1], info, 10)) == LONG_MIN) {
            fprintf(stderr, "error\n");
        }
        printf("n = %ld, err = ", n);
    } else{
        fprintf(stderr, "too few args\n");
    }
    return 0;
}
    
