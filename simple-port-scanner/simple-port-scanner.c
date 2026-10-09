#include <limits.h>
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

#define TOK_DELIM " ,-"
#define MAX_CAP   100
#define MIN_PORT  1
#define MAX_PORT  65535 

/*
 * TODO: i think i should separate some concepts and functionalities in here into a header for
 * more organization.
 */


/*
 * TODO: fix the port range function and the ip adress input stream(decide how the program should take the ip address)...
 */

enum prog_state{
    _ERR    = 01,               // error occured must terminate the program
    _EXIT   = 02,               // should exit the program due to an option like --help or -h(it should display exit)           
    _WAIT   = 04,               // program should wait
    _RESUME = 16                // program should continue executing 
};

struct config {
    int takes_args;
    long port_max;
    long port_min;
};


typedef int (*opthandler)(const char *arg, struct config *cfg);

typedef struct {
    char        *long_name;     // "help", "ports"
    char        short_name;     // 'h', 'p'
    int         takes_args;     // 0, 1
    opthandler  handler;        // option function
    const char *description;    // used to generate the help text
} option_entry;


/****** UTILS FUNCTIONS *****/
static int  is_valid_ipv4(const char*);                    // CHECK FOR A VALID IPv4 ADDRESS
static int  isthere_digits(const char*);                   // CHECK IF THERE IS DIGITS IN A STRING
static char *check_opt(char *);                            // RETURN THE OPTION CHARACTER
int         help_opt(const char *arg, struct config *cfg); // DISPLAYING OPTIONS
int         port_ran(const char *arg, struct config *cfg); // SETTING PORT RANGE    
int         parsing_func(char *,struct config *, char **); // PARSING THE CL ARGS
int         exec_opt(char*, char*,struct config *);

// defining options and their properties
option_entry opt_prop[3] = {

    {"-help", 'h', 0, &help_opt, "- Displaying options.\n. Usage: --help or -h\n"},
    {"-port", 'p', 1, &port_ran, "- Setting the port range needed to scan.\n. Usage: --port -p <int> <int>\n"},
    {NULL, 0, 0, NULL, NULL}

};


int main(int argc, char *argv[])
{
    int    func_ret,index;
    enum   prog_state state = _RESUME;
    struct config cofg = { 0, 0L, 0L};
    char   *option = 0;
	char   *input_addr = NULL;			    /* to store the ip address from the user */
	int    socket_fd;				        /* socket file descriptor */
	struct sockaddr_in addr;		        /* ip address */
	struct pollfd pfd;			            /* for a non-blocking socket */
	int    expected_error = EINPROGRESS;	/* to identify that the connection is in progress */
	int    err;
	socklen_t len = sizeof(err);
	
    if (argc >= 2) {
        for(int j = 0;j < argc; ++j) {
            if((func_ret = is_valid_ipv4(argv[j])) != 1) {
                state |= _WAIT;
            } else if(func_ret == 1){
                input_addr = argv[j];
                state |= _RESUME;
                index = j;
                break;
            }
        }

        for(int i = 1;i < argc; ++i) {
            if(i == index)continue;
        
            if(parsing_func(argv[i], &cofg, &option) == -1){
                // parsing failed
                state &= 0;
                state |= _ERR;
            }

            if((state & _ERR) == _ERR) {
                // error has occurred the program must exit
                free(option);
                option = NULL;
                return -1;
            } else if((state & _WAIT) == _WAIT) {
                /* program should wait for options:
                 * - State's of the program must be checked for error (_ERR) for options that needs to resume the connection or 
                 *   other functionalities.
                 * - For options that needs to exit the program like (--help or -h)
                 */

                /* if the option takes arguments pass the next CL argument */
                func_ret = (cofg.takes_args) ? exec_opt(option, argv[++i], &cofg) : exec_opt(option, argv[i], &cofg);   
                
                if(func_ret == 1) {
                    // the program needs to exit
                    state |= _EXIT;
                } else if(func_ret == -1) {
                     // error occurred the program must exit
                    state &=  0;
                    state |= _ERR;
                } else {
                    state |= _RESUME;
                }
            }
                
            // Deciding what the program should do
            if((state & _ERR) == _ERR) {
                return -1;
            } else if ((state & _EXIT) == _EXIT) {
                return 1;
            } else {
                if (input_addr == NULL) {
                    return 1;
                } else {
                    state &= 0;
                    state |= _RESUME;
                }
            }
        
        }
    } else{
        fprintf(stderr, "too few arguments.");
        printf("Usage: %s <ipaddr> ...\n", argv[0]);
        printf("For options: %s --help\n", argv[0]);
        return 1;
    }
	
	printf("%s is a valid IPv4 address.\n",input_addr);

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
							// closed port.
							//printf("port %ld is closed on %s\n", port, input_addr);

						} else if(err == ETIMEDOUT){	// connection timed out
							
							printf("connection timed out on port %ld .\n", port);
						} else if(err == ENETUNREACH){  // network unreachable
							
							printf("network is unreachable on port: %ld.\n", port);
						} else{
							printf("The error is beyond these errors( ENETUNREACH, ETIMEDOUT, ECONNREFUSED)\n");		
						}
					}

				} else if(pfd.revents & POLLERR){	//check if any errors occured in pfd.revents
					
					printf("error occured.\n");
				
				} else if(return_val_poll == 0){	// the poll timeout ran out of time
					printf("connection timeout on port %ld \n", port);
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

static int  isthere_digits(const char *arg) 
{
    size_t len = strlen(arg);
    for(int i = 0; i < len; ++i) {
        if(isdigit(arg[i])) {
            return 0;
        }
    }
    return 1;
}

int exec_opt(char *option, char *arg, struct config *cofg) 
{
    if(option == NULL) {
        fprintf(stderr, "exec_opt: passed an empty string\n");
        free(option);
        return -1;
    }
    if(strlen(option) > 1){
       for(int i = 0;opt_prop[i].handler != NULL; ++i) {
  
           if(strcmp(option,opt_prop[i].long_name) == 0) 
           {
               free(option);
               option = NULL;
               return (opt_prop[i].takes_args) ? opt_prop[i].handler(arg, cofg) : 
                                                 opt_prop[i].handler("do not need args", cofg);
           }
       }
    } else{
        for(int i = 0;opt_prop[i].handler != NULL; ++i) {
  
           if(option[0] == opt_prop[i].short_name) 
           {
               free(option);
               option = NULL;
               return (opt_prop[i].takes_args) ? opt_prop[i].handler(arg, cofg) : 
                                                 opt_prop[i].handler("do not need args", cofg);
           }
       }
    }
       // if there isn't a match free the option
       free(option);
       option = NULL;
       fprintf(stderr, "Invalid option entered\n");
       fprintf(stderr,"for help: --help or -h.\n");
       return -1;
}

static char  *check_opt(char *arg)
{
    char option[MAX_CAP];
    int j = 0;
    if(arg[1] == '-') {
        
        option[j++] = '-';
        for(int i = 2; isalpha(arg[i]) && j < MAX_CAP-1; ++i) {
            option[j++] = arg[i];
        }
        option[j] = '\0';
    
    } else if(isalpha(arg[1])) {
    
        for(int i = 1; isalpha(arg[i]) && j < MAX_CAP-1; ++i) {
            option[j++] = arg[i];
        }
        option[j] = '\0';
    
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
    if(arg == NULL) {
        // default port range if nothing -p or --port has 0 arguments passed
        fprintf(stderr, "port_ran: arg passed empty.\nSetting port range to [20,80].\n");
        cfg->port_min = 20;
        cfg->port_max = 80;
        return 0;
    } else {
        if(isthere_digits(arg)) {
            fprintf(stderr, "You did not pass a valid port numbers.\n");
            return 1; 
        }
    }
   
    
    long p_max,p_min;
    char *str = strdup(arg);
    char *tokens[2];
    char *token;
    char **endptr = 0;
    int  i = 0;

    token = strtok(str, TOK_DELIM);
    while(token != NULL && i < 2) {
        tokens[i++] = token;    
        token = strtok(NULL, TOK_DELIM);
    }
    
    /* checking bounds of tokens */
    if(i > 2) {
        fprintf(stderr, "too many port numbers.\nUsage: -p or --port int,int\n");
        free(str);
        return -1;
    }

    /* checking if the tokens are valid to pass them to strtol() */
    for(int j = 0;j < i; ++j) {
        if(strtol(tokens[j], endptr, 10) == LONG_MAX ||
           strtol(tokens[j], endptr, 10) == LONG_MIN ||   
           strtol(tokens[j], endptr, 10) == 0      ) {
                return -1;
        }
    }
    if(i == 2){
        p_min = strtol(tokens[1], endptr, 10);
        p_max = strtol(tokens[0], endptr, 10);
        
        if(p_min < MIN_PORT) {      // Checking bounds
            p_min = MIN_PORT;
        } else if(p_max > MAX_PORT){
            p_max = MAX_PORT;
        }
    } else if(i == 1){
        if((p_min = p_max = strtol(tokens[0], endptr, 10)) == 0) {
            fprintf(stderr, "port_ran: passed an invalid port range.\n");
            fprintf(stderr, "Usage: --help or -h for Usage.\n");
            free(str);
            return -1;
        }
        
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


int parsing_func(char *argv, struct config *cofg, char **option)
{
    if(argv == NULL) {
        fprintf(stderr, "parsing_func: argv passed as NULL.\n");
        fprintf(stderr, "for help: --help or -h\n");
        return -1;
    }
    if(argv[0] == '-' && (isalpha(argv[1]) || argv[1] == '-')) {
            if((*option = check_opt(argv)) == NULL) {
                fprintf(stderr, "Invalid option.\n");
                free(option);
                return -1;
            }
            for(int i = 0;opt_prop[i].long_name != NULL; ++i) {     // check the option if it takes arguments
                
                if(opt_prop[i].short_name == *option[0] ||
                   strcmp(opt_prop[i].long_name,*option) == 0) {

                   cofg->takes_args = opt_prop[i].takes_args; 
                }
            }
    } else{
        fprintf(stderr, "parsing_func: Invalid option entered.\n");
        return -1;
    }
    
    return 0;
}
int is_valid_ipv4(const char *src)
{
	struct sockaddr_in dst;

	return inet_pton(AF_INET, src, &dst);
}
