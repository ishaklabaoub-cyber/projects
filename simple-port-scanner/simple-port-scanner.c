#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>

#define TOK_DELIM " ,"
#define MAX_CAP 100
#define MIN_PORT 1
#define MAX_PORT 65535 

/*
 * TODO: i think i should separate some concepts and functionalities in here into a header for
 * more organization.
 */


/*
 * TODO: fix the port range function and the ip adress input stream and check portmax-min
 */


struct config {
    int takes_args;
    long port_max;
    long port_min;
};


typedef int (*opthandler)(const char *arg, struct config *cfg);

typedef struct {
    char *long_name;     // "help", "ports"
    char short_name;     // 'h', 'p'
    int  takes_args;     // 0, 1
    opthandler handler;  // option function
    const char *description; // used to generate the help text
} option_entry;


/****** UTILS FUNCTIONS *****/
static int is_valid_ipv4(const char*);
static char *check_opt(char *);         // RETURN THE OPTION CHARACTER
int  help_opt(const char *arg, struct config *cfg); // DISPLAYING OPTIONS
int  port_ran(const char *arg, struct config *cfg); // SETTING PORT RANGE    
int  parsing_func(char *,struct config *, char **); // PARSING THE CL ARGS
int  exec_opt(char*, char*,struct config *);

// defining options and their properties
option_entry opt_prop[3] = {

    {"-help", 'h', 0, &help_opt, "- Displaying options.\n. Usage: --help or -h\n"},
    {"-port", 'p', 1, &port_ran, "- Setting the port range needed to scan.\n. Usage: --port -p <int> <int>\n"},
    {NULL, 0, 0, NULL, NULL}

};


int main(int argc, char *argv[])
{
    struct config cofg = { 0, 0L, 0L};
    char *option = 0;
	char *input_addr;			/* to store the ip address from the user */
	//int port_min = 1, port_max = 65535;	/* port interval */
	int socket_fd;				/* socket file descriptor */
	struct sockaddr_in addr;		/* ip address */
	struct pollfd pfd;			/* for a non-blocking socket */
	int    expected_error = EINPROGRESS;	/* to identify that the connection is in progress */
	int    err;
	socklen_t len = sizeof(err);
	
    /********************** CHANGING , SO THE INPUT IS GET FROM THE PROGRAM'S ARGUMENTS **********************/
    if (argc >= 2) {
        for(int i = 1;i < argc; ++i) {
            if (is_valid_ipv4(argv[i]) != 1) {
                printf("IP address passed is invalid.\n");
                printf("Usage: %s <ipaddr> ...\n", argv[0]);
                printf("For options: %s --help\n", argv[0]);
                return 1;
            } else{
            // VALID ip address
            input_addr = argv[i];
            i++;
            }  

            if(parsing_func(argv[i], &cofg, &option) == 1){
                // parsing failed
                return 1;
            }
            /* if the option takes arguments pass the next CL argument */
            printf("\t\t1DEBUG: option[%s]\n", option);
            int ret = (cofg.takes_args) ? exec_opt(option, argv[++i], &cofg) : exec_opt(option, argv[i], &cofg);   
            if(ret == 1) {
                // the program needs to exit
                return 1;
            } else if(ret == -1) {
                // error occurred the program must exit
                return 1;
            }
        }
        if (is_valid_ipv4(argv[1]) != 1) {
            printf("IP address passed is invalid.\n");
            printf("Usage: %s <ipaddr> ...\n", argv[0]);
            printf("For options: %s --help\n", argv[0]);
            return 1;
        } else{
            // VALID ip address
            input_addr = argv[1];
        }
    } else{
        fprintf(stderr, "too few arguments.");
        printf("Usage: %s <ipaddr> ...\n", argv[0]);
        printf("For options: %s --help\n", argv[0]);
        return 1;
    }
	
	printf("%s is a valid IPv4 address\n",input_addr);
    /*do{		// Getting port range
		printf("\nUsage : <int>-<int> (example : 20-80)\n");
		printf("Please Enter the range of ports: ");
		
        if( scanf(" %d-%d", &port_min, &port_max) != 2){
			printf("scanf: FAILED\n");
			port_min = -1;
			port_max = 65536;
		}

	}while(port_min < 0 || port_max > 65535  || port_min > port_max);*/
	
    printf("\t\t DEBUG: portmin[%ld] portmax[%ld]",cofg.port_min,cofg.port_max);
	for(long int port = cofg.port_min; port <= cofg.port_max; ++port){
		
		socket_fd = socket(AF_INET,SOCK_STREAM,0);		/* creating an endpoint */
		if(socket_fd == -1){
			// socket() failed
			perror("socket");
			close(socket_fd);
			continue;
		}

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);	
		if( inet_pton(AF_INET, input_addr, &addr.sin_addr) != 1){
			fprintf(stderr,"INVALID IP ADDRESS : %s\n",input_addr);
			perror("inet_pton");
			close(socket_fd);
		}

		/***************** must add the poll() function for non-blocking sockets*******************/ 
		fcntl(socket_fd, F_SETFL, O_NONBLOCK);
		pfd.fd = socket_fd;	
		pfd.events = POLLOUT;
		pfd.revents = 0;
		
		errno = 0;
		int ret = connect(socket_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));		/* trying to connect */
		
		if(ret == -1){
			int current_error = errno;
			if(expected_error == current_error){	// connection in progress

				int return_val_poll = poll(&pfd, 1, 300);
				if(pfd.revents & POLLOUT){	// the socket is writable and O_NONBLOCK must be set
					
					if(getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1){
						// getsockopt() failed
						perror("getsockopt");
					} else{
						if(err == 0){	// connection succeeded
							printf("port %ld is open on %s\n", port, input_addr);

						} else if(err == ECONNREFUSED){ // connection refused
							
							printf("port %ld is closed on %s\n", port, input_addr);

						} else if(err == ETIMEDOUT){	// connection timed out
							
							printf("connection timed out on port %ld .\n", port);
						} else if(err == ENETUNREACH){  // network unreachable
							
							printf("network is unreachable.\n");
						} else{
							printf("The error is beyond these errors( ENETUNREACH, ETIMEDOUT, ECONNREFUSED)\n");		
						}
					}

				} else if(pfd.revents & POLLERR){	//check if any errors occured in pfd.revents
					
					printf("error occured.\n");
				
				} else if(return_val_poll == 0){	// the poll timeout ran out of time
					printf("connection timeout.\n");
				}
			} else if(current_error == ECONNREFUSED){
				printf("port %ld is closed on %s\n", port, input_addr);
			} else{
				perror("connect");
			}		
		} else{
			// connection succeed
			printf("port %ld is open on %s\n", port, input_addr);
		}
			close(socket_fd);
	}

	return 0;
}

int exec_opt(char *option, char *arg, struct config *cofg) 
{
    if(option == NULL) {
        fprintf(stderr, "exec_opt: passed an empty string\n");
        return -1;
    }
    if(strlen(option) > 1){
       for(int i = 0;opt_prop[i].handler != NULL; ++i) {
  
           if(strcmp(option,opt_prop[i].long_name) == 0) 
           {
               free(option);
               return (opt_prop[i].takes_args) ? opt_prop[i].handler("do not need args", cofg) : 
                                                 opt_prop[i].handler(arg, cofg);
           }
       }
    } else{
        for(int i = 0;opt_prop[i].handler != NULL; ++i) {
  
           if(option[0] == opt_prop[i].short_name) 
           {
               free(option);
               return (opt_prop[i].takes_args) ? opt_prop[i].handler("do not need args", cofg) : 
                                                 opt_prop[i].handler(arg, cofg);
           }
       }
    }
       // if there isn't a match free the option
       free(option);
       fprintf(stderr, "Invalid option entered\n");
       return -1;
}

static char  *check_opt(char *arg)
{
    char option[MAX_CAP];
    int j = 0;
    if(arg[1] == '-') {
        option[j++] = '-';
        for(int i = 2; isalpha(arg[i]) && j < MAX_CAP; ++i) {
            option[j++] = arg[i];
        }
        option[j] = '\0';
    } else if(isalpha(arg[1])) {
        option[0] = arg[1];
        option[1] = '\0';
    } else {
        return NULL;
    }
    return strdup(option);
}

int help_opt(const char *arg, struct config *cfg) 
{
    printf("\n\t\t***HELP is COMING***\nIshak's port scanner options:\n");
    for(int i = 0;opt_prop[i].description != NULL; ++i) {
        printf("%s", opt_prop[i].description);
    }
    return 1;
}

int  port_ran(const char *arg, struct config *cfg)
{
    /* may be use strtok() to tokenize numbers that are in the argument then passe them to strtol() */        
    if(arg == NULL) {
        fprintf(stderr, "port_ran: passed an empty arg.\n");
        return -1;
    }
    long p_max,p_min;
    char *str = strdup(arg);
    char *tokens[2];
    char **endptr = 0;
    int  i;

    i = 0;
    strtok(str, TOK_DELIM);
    while(str != NULL && i < 2) {
        tokens[i++] = str;    
        strtok(NULL, TOK_DELIM);
    }
    printf("\t\tDEBUG: i[%d]\n",i);
    if(i > 2) {
        fprintf(stderr, "too many port numbers.\nUsage: -p or --port int,int\n");
        free(str);
        return -1;
    } else if(i == 1){
        p_min = strtol(tokens[1], endptr, 10);
        p_max = strtol(tokens[0], endptr, 10);
        
        if(p_min < MIN_PORT) {      // Checking bounds
            p_min = MIN_PORT;
        } else if(p_max > MAX_PORT){
            p_max = MAX_PORT;
        }
    } else{
        p_min = p_max = strtol(tokens[0], endptr, 10);
        if(p_min < MIN_PORT) {
            p_min = p_max = MIN_PORT;
        } else if(p_max > MAX_PORT){
            p_min = p_max = MAX_PORT;
        } 
    }
    if(p_min > p_max) {
            long temp = p_min;
            p_min = p_max;
            p_max = temp;
    }
    cfg->port_min = p_min;
    cfg->port_max = p_max;
    free(str);
    return 0;
}

int is_valid_ipv4(const char *src)
{
	struct sockaddr_in dst;

	return inet_pton(AF_INET, src, &dst);
}
int parsing_func(char *argv, struct config *cofg, char **option)
{
        if(argv[0] == '-') {
            if((*option = check_opt(argv)) == NULL) {
                fprintf(stderr, "Invalid option.\n");
                printf("\t\t2DEBUG: option[%s]\n", *option);
                free(option);
                return 1;
            }
            printf("\t\t3DEBUG: option[%s]\n", *option);
            for(int i = 0;opt_prop[i].long_name != NULL; ++i) {     // check the option if it takes arguments
                
                if(opt_prop[i].short_name == *option[0] ||
                   strcmp(opt_prop[i].long_name,*option) == 0) {

                   cofg->takes_args = opt_prop[i].takes_args; 
                }
            }
        }
    
    return 0;
}
