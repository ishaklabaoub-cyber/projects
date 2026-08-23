#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 100

int   getch();
void  ungetch(int);
char* readlines();
void  sh_loop(void);

char *line;			/* line of commands */
// SIGNALS
int  signal_terminate = 0;


int main()
{
	sh_loop();

	return 0;
}
char* readline(){
	int c;
	char *buf;
	int bufp = 0;
	int bufsize = BUFFER_SIZE;
	
	buf = malloc( sizeof(char) * BUFFER_SIZE);	
	if(!buf){
		fprintf(stderr,"malloc: failed allocation...\n");
		exit(EXIT_FAILURE);
	}
	buf[0] = '\0';
	
	while(1){
		c = getchar();
		if(c == '\n'){
			buf[bufp] = '\0';
			return buf;
		} else if(c == EOF){
			signal_terminate = 1;
			return NULL;
		} else{
			buf[bufp] = c;
			bufp++;
		}
		if(bufp >= bufsize){				/* if the buffer position get exceeded , realloc*/
			buf = realloc(buf,sizeof(char) * BUFFER_SIZE);	
			if(!buf){
				fprintf(stderr,"realloc: failed allocation...\n");
				exit(EXIT_FAILURE);
			}		
			bufsize += BUFFER_SIZE;
		}
	}

}
void sh_loop()
{
	do{
		printf("My shell> ");
		line = readline();
		
		if(line == NULL){
			// EOF
			return ;
		}
		if( !strcmp(line,"exit") ){
			// EXIT PROGRAM
			free(line);
			return ;
		} else if( strcmp(line,"ls") ){
			fprintf(stderr,"shell : there is no such command\n");
		}else{
			//just testing
			printf("%s\n",line);
		}
		free(line);
	}while(1);
	
}
