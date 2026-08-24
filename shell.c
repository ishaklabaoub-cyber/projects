#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE    100
#define TOK_BUFF_SIZE  64
#define TOK_DELIM      ' \t\r\n\a'

char** readtok(char*);
char*  readlines();
void   sh_loop(void);

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
			bufsize += BUFFER_SIZE;
			buf = realloc(buf,sizeof(char) * BUFFER_SIZE);	
			if(!buf){
				fprintf(stderr,"realloc: failed allocation...\n");
				exit(EXIT_FAILURE);
			}		
		}
	}
}

char** readtok(char* line)
{
	int    bufsize = TOK_BUFF_SIZE , position = 0;
	char** tokens  = malloc(bufsize * sizeof(char)); 	/* Storing tokens to use them after */
	char*  token;						/* Storing tokens from strtok() */

	if( !tokens){
		fprintf(stderr,"malloc : failed allocation...!\n");
		exit(EXIT_FAILURE);
	}

	token = strtok(line,TOK_DELIM);
	while( token != NULL)
	{
		tokens[position] = token;
		position++;
	
		if(position >= bufsize){			/* Checking for limits */
			bufsize += TOK_BUFF_SIZE;
			tokens = realloc(tokens,bufsize * sizeof(char));
			if( !tokens){
				fprintf(stderr, "malloc : failed allocation...!\n");
				exit( EXIT_FAILURE);
			}
		}
		token = strtok(NULL,TOK_DELIM);
	}
	tokens[position] = NULL;	
	
	return tokens;
}

void sh_loop()
{
	do{
		printf("My shell> ");
		line = readline();
		
		if(signal_terminate == 1){
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
