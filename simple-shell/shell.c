#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE    100
#define TOK_BUFF_SIZE  64
#define TOK_DELIM      " \t\r\n\a"


// BUILT-IN FUNCTIONS
int    sh_cd(char**);
int    sh_help(char**);
int    sh_exit(char**);
/**********************/

int    sh_num_builtin();
char** readtok(char*);
char*  readline();
int    sh_launch(char**);
int    sh_execute(char**);
void   sh_loop(void);

char*  line;			/* line of commands */
char** args;			/* arguments parsed from the line */
int    status;			/* status of the program */
/*
  List of builtin commands, followed by their corresponding functions.
 */
char* builtin_str[] = 
{	
	"cd",
	"help",
	"exit",	
};

int (*builtin_func[])(char**) = 
{
	&sh_cd,
	&sh_help,
	&sh_exit
};

// SIGNALS
int  signal_terminate = 0;


int main()
{
	sh_loop();

	return 0;
}
int sh_num_builtin()
{
	return sizeof(builtin_str) / sizeof(char *);
}
/*
 BUILT_IN FUNCTIONS IMPLIMINTATION
 */
int sh_cd(char** args)
{
	if(args[1] == NULL){
		fprintf(stderr,"shell : expected arguments to \"cd\"\n");
	} else{
		if(chdir(args[1]) != 0){
			perror("sh");
		}
	}
	return 1;
}
int sh_help(char** args)
{
	int i;
	printf("MMOUCH's SHell PROGRAM\n");
	printf("Try typing the program's names and hit ENTER\n");
	printf("Here's builtin programs:\n");

	for(i = 0;i < sh_num_builtin();i++){
		printf("- %s\n",builtin_str[i]);
	}
	printf("Use the man command for information on other programs.\n");
	return 1;
}
int sh_exit(char** args)
{
	return 0;
}
/*************************************************/

int sh_execute(char** args)
{
	int i;

	if(args[0] == NULL){
		//empty command entered
		return 1;
	}

	for(i = 0;i < sh_num_builtin();i++){
		if( strcmp(args[0],builtin_str[i]) == 0){
			return (*builtin_func[i])(args);
		}
	}
	return sh_launch(args);
}
int sh_launch(char** args)
{
	pid_t pid,wpid;
	int status;

	pid = fork();
	if(pid == 0){
		//Child process
		if( execvp( args[0], args) == -1 ){ 	/* giving the child process a new process to run via execvp() */
			perror("shell");
		}
		exit(EXIT_FAILURE);
	} else if(pid < 0){
		//Error forking
		perror("shell");
	} else{
		//Parent process
		do{
			wpid = waitpid(pid, &status, WUNTRACED);

		}while( !WIFEXITED(status) && !WIFSIGNALED(status) );/* waiting for a child process to terminate normally or by signal*/
	}
	return 1;
}

char* readline()
{
	int   c;
	char* buf;
	int   bufp = 0;
	int   bufsize = BUFFER_SIZE;
	
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
			buf = realloc(buf,sizeof(char) * bufsize);	
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
	char** tokens  = malloc(bufsize * sizeof(char*)); 	/* Storing tokens to use them after */
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
			tokens = realloc(tokens,bufsize * sizeof(char*));
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
		
		line = readline();		/* Get line from user */
		args = readtok(line);		/* tokenize the line to arguments */
		status = sh_execute(args);	/* status of the program to continue performing or no */

		if(signal_terminate == 1){
			// EOF
			return ;
		}
		free(line);
		free(args);
	}while(status);
	
}
