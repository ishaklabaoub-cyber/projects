#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>

int is_valid_ipv4(const char*);


int main()
{
	char input_addr[16];			/* to store the ip address from the user */
	int port_min = 0, port_max = 65535;	/* port interval */
	int socket_fd;				/* socket file descriptor */
	struct sockaddr_in addr;		/* ip address */
	struct pollfd pfd;			/* for a non-blocking socket */
	int    expected_error = EINPROGRESS;
	int    err;
	socklen_t len = sizeof(err);
	
	do{		// Getting IP address
		printf("Please Enter IPv4 address that you want to scan: ");
		scanf("%15s", input_addr);
	
	}while(is_valid_ipv4(input_addr) == 0);
	
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
			return 1;
		}

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);	
		if( inet_pton(AF_INET, input_addr, &addr.sin_addr) != 1){
			printf("INVALID IP ADDRESS : %s\n",input_addr);
			perror("inet_pton");
			close(socket_fd);
			return 1;
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
							printf("some error\n");			/*****************************FIIIIIIIIIIIIIIIIIXXXXXXXXXXX**********/
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

int is_valid_ipv4(const char *src)
{
	struct sockaddr_in dst;

	if(inet_pton(AF_INET, src, &dst) == 1){
		return 1;
	}
	return 0;
}
