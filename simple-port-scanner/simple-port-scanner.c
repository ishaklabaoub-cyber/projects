#include <netinet/in.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>

#define MAX_CAP 100



/****** UTILS FUNCTIONS *****/
static int is_valid_ipv4(const char*);
static char check_option(char *);         // RETURN THE OPTION CHARACTER
void help_options();                    // DISPLAYING OPTIONS
int  parsing_func(int, char *[]);
void executing_option(char);

char *options[2] = {
    "h",
    NULL
};

void (*option_func[])() = 
{
    &help_options
};



int main(int argc, char *argv[])
{
	char *input_addr;			/* to store the ip address from the user */
	int port_min = 1, port_max = 65535;	/* port interval */
	int socket_fd;				/* socket file descriptor */
	struct sockaddr_in addr;		/* ip address */
	struct pollfd pfd;			/* for a non-blocking socket */
	int    expected_error = EINPROGRESS;	/* to identify that the connection is in progress */
	int    err;
	socklen_t len = sizeof(err);
	
    /********************** CHANGING , SO THE INPUT IS GET FROM THE PROGRAM'S ARGUMENTS **********************/
    if (argc >= 2) {
        if(parsing_func(argc, argv) == 1){
            return 1;
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
	
    do{		// Getting port range
		printf("\nUsage : <int>-<int> (example : 20-80)\n");
		printf("Please Enter the range of ports: ");
		
        if( scanf(" %d-%d", &port_min, &port_max) != 2){
			printf("scanf: FAILED\n");
			port_min = -1;
			port_max = 65536;
		}

	}while(port_min < 0 || port_max > 65535  || port_min > port_max);
	

	for(int port = port_min; port <= port_max; ++port){
		
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
			printf("INVALID IP ADDRESS : %s\n",input_addr);
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
				if(pfd.revents & POLLOUT){	// the socket is writable
					
					if(getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1){
						// getsockopt() failed
						perror("getsockopt");
					} else{
						if(err == 0){	// connection succeeded
							printf("port %d is open on %s\n", port, input_addr);

						} else if(err == ECONNREFUSED){ // connection refused
							
							printf("port %d is closed on %s\n", port, input_addr);

						} else if(err == ETIMEDOUT){	// connection timed out
							
							printf("connection timed out on port %d .\n", port);
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
				printf("port %d is closed on %s\n", port, input_addr);
			} else{
				perror("connect");
			}		
		} else{
			// connection succeed
			printf("port %d is open on %s\n", port, input_addr);
		}
			close(socket_fd);
	}

	return 0;
}

void executing_option(char option) 
{
       for(int i = 0;options[i] != NULL; ++i) {
           if(option == options[i][0]) {
            return (*option_func[i])();
           }
       }
       fprintf(stderr, "Invalid option entered\n");
}

char  check_option(char *arg)
{
    char option[MAX_CAP];
    int j = 0;
    if(arg[1] == '-') {
        for(int i = 2; isalpha(arg[i]) && j < MAX_CAP; ++i) {
            option[j++] = arg[i];
        }
        option[j] = '\0';
        if(strcmp(option, "help") != 0)
            return 0;
    } else if(isalpha(arg[1]) && arg[1] != 'h') {
        option[0] = arg[1];
        option[1] = '\0';
    } else {
        return 0;
    }
    return option[0];
}

void help_options() 
{
    printf("\n\t\t***HELP is COMING***\nIshak's port scanner options:\n");
    printf("-p <int> <int> for specifying port ranges.\n");
    printf("...\n");
}

int is_valid_ipv4(const char *src)
{
	struct sockaddr_in dst;

	return inet_pton(AF_INET, src, &dst);
}
int parsing_func(int argc, char *argv[])
{
    char option;
    for(int i = 1, j;i < argc; ++i) {
        j = 0;
        while(isspace(argv[i][j++]))          // skipping white spaces
            ;
        
        if(argv[i][0] == '-') {
            if((option = check_option(argv[i])) == 0) {
                fprintf(stderr, "Invalid option.\n");
                printf("Usage: %s --help for options.\n", argv[0]);
                return 1;
            }
            executing_option(option);
        }
    }
    return 0;
}
