***Description:
- This is a CLI TCP port scanner written in C language using non-blocking sockets.

**WHY I BUILT IT:
- For several reasons :
1. I found Learning by Doing is the most effective approach i can have to aknowledge a concept more deeply. 
2. To implement my networking knowledge , as i am learning it.
3. To understand the UNIX socket API.
4. Practice more on the C language and get more experienced.

**HOW IT WORKS:
- After getting the valid IP address of the target that the user want to scan AND the valid port range of the scan, then the program creates successfully an end-point for communication using socket() function, after that i used the connect() + poll() combo for a non-blocking sockets rather than a blocking-sockets because of the speed, the blocking-sockets connects would make a full port range scan painfully slow.

**KNOWN LIMITATIONS:
- This program supports only the IPv4 address family.
- This program is single-threaded, so large port ranges may be slow.
- Some errno cases beyond ECONNREFUSED/ETIMEDOUT/ENETUNREACH aren't distinguished yet.

***key notes:
- I learned a lot of new functions and concepts in this project including the socket API especially the non-blocking socket, where i learned the poll() function and it's concepts like the POLLOUT flag that shows just the writability of the socket so you got to check with getsockopt() function to identify the states of the connection.
- Reading manual pages sometimes get hard especially the errors and flags but you have to be patient with it that's all.

***USAGE:
- Compile with: gcc -Wall -Wextra -Werror  simple-port-scanner.c   -o simple-port-scanner
- when running the program: 
1. Please enter a valid IPv4 address of the target you want to scan.
2. Enter a port range to scan.
3. Results will pop up in your terminal

***HAVE FUN SCANNING***
