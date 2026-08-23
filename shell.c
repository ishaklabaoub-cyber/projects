#include <stdio.h>
#include <string.h>

#define MAXTOKEN 1000

int main()
{
	char token[MAXTOKEN];
	do{
		printf("My shell> ");
		scanf("%999s",token);
	}while( strcmp("exit",token) );
	return 0;
}
