#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/poll.h>

int is_valid_ipv4(const char*);


int main()
{
	char input_addr[16];			/* to store the ip address from the user */
	int port_min = 0, port_max = 65535;	/* port interval */
	int socket_fd;				/* socket file descriptor */
	struct sockaddr_in addr;		/* ip address */
	struct pollfd pfd;
	
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

	}while((port_min < 0 && port_max > 65535 ) || port_min > port_max);
	

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
		pfd.fd = socket_fd;
		pfd.events = POLLOUT;
		pfd.revents = 0;

		if( connect(socket_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1){	/* trying to connect */
			// connect() failed
		 	perror("connect");
			close(socket_fd);
		} else{
			poll(&pfd, 1, 1000);
			if(pfd.revents & POLLOUT){
				printf("port %d is open on %s\n\n", port, input_addr);
			}
			close(socket_fd);
		}
		
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
