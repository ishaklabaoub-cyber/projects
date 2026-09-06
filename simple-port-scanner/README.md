## Description:
- This is a CLI simple TCP port scanner written in C language using non-blocking sockets.

## WHY I BUILT IT:
- For several reasons :
1. I found Learning by Doing is the most effective approach i can have to grasp a concept more deeply. 
2. Implementing my networking knowledge , as i am learning it.
3. understanding the UNIX socket API.
4. Practicing more on the C language and getting more experienced.

## HOW IT WORKS:
(1) validate IP/port input.
(2) create socket.
(3) non-blocking connect.
(4) poll + getsockopt to resolve final state.

## KNOWN LIMITATIONS:
- This program supports only the IPv4 address family.
- This program is single-threaded, so large port ranges may be slow.
- Some errno cases beyond ECONNREFUSED/ETIMEDOUT/ENETUNREACH aren't distinguished yet.

## key notes:
- I learned a lot of new functions and concepts in this project including the socket API especially the non-blocking socket, where i learned the poll() function and its concepts like the POLLOUT flag that shows just the writability of the socket so you got to check with getsockopt() function to identify the states of the connection.
- Reading manual pages sometimes get hard especially tracking the errors and flags but you have to be patient with it that's all.

## USAGE:
- Compile with: 
```
gcc -Wall -Wextra -Werror  simple-port-scanner.c   -o simple-port-scanner
```
- when running the program: 
1. Please enter a valid IPv4 address of the target you want to scan.
2. Enter a port range to scan.
Example:
``` 
Please Enter IPv4 address that you want to scan: 192.168.1.1
Please Enter the range of ports: 20-100
```
3. Results will pop up in your terminal

***HAVE FUN SCANNING :)***
