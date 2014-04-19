/* client.c - code for example client program that uses TCP */
#ifndef unix
#define WIN32
#include <windows.h>
#include <winsock.h>
#else
#define closesocket close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define PROTOPORT 36790 /* default protocol port number */
#define QLEN 6 /* size of request queue */
extern int errno;
typedef enum { false, true } bool;
char localhost[] = "localhost"; /* default host name */
/*------------------------------------------------------------------------
* Program: piggy1
*
* Purpose: Network middleware where a client can connect to the left side
* of piggy and piggy can also connect to a server with an address specified
* by -raddr option. Alternatively if -noright option is set then piggy will
* echo what is read from the left.  
*
* Syntax: piggy -option 
*
* Options:
* laddr   - specifies what address the server can accept. 
* raddr   - specifies what address piggy should connect to.
* noleft  - flags that piggy should use stdin for it's left side.
* noright - flags that piggy should use stdout for it's right side.
*
* The address for laddr and raddr can be a dotted IP address or
* a DNS name and the value must directly follow the argument.
*
* Note: No default address for right side so raddr value or noright
* must be set. Laddr defaults to accepting any left side connection
* which is equivalent to setting it's value to "*".  
*
* The default port is 36790
*------------------------------------------------------------------------
*/
main(int argc,char *argv[])
{
	struct protoent *ptrp; /* pointer to a protocol table entry */
	#ifdef WIN32
	WSADATA wsaData;
	WSAStartup(0x0101, &wsaData);
	#endif

	/* loop through arguments and set values */
	bool no_left = false; /* holds value of command line argument -noleft */
	bool no_right = false; /* Holds value of command line argument -no_right */
	char *laddr = NULL; /* Holds the address of the left connect as either an IP address or DNS name*/
	char *raddr = NULL; /* Holds the address of the left connect as either an IP address or DNS name*/
	unsigned short lacctport;
	unsigned short luseport;

	int arg_i;
	for (arg_i = 1; arg_i < argc; arg_i++){
		if (strcmp(argv[arg_i],"-noleft") == 0)
			no_left = true;
		else if (strcmp(argv[arg_i], "-noright") == 0)
			no_right = true;
		else if (strcmp(argv[arg_i], "-laddr") == 0 && (arg_i + 1) < argc)
			laddr = argv[++arg_i]; /* add check to this. Note that this will also move arg_i past val */
		else if (strcmp(argv[arg_i], "-raddr") == 0 && (arg_i + 1) < argc)
			raddr = argv[++arg_i]; /* add check to this. Note that this will also move arg_i past val */
		else if (strcmp(argv[arg_i], "-lacctport") == 0 && (arg_i + 1) < argc)
			lacctport = atoi(argv[++arg_i]);
		else if (strcmp(argv[arg_i], "-luseport") == 0 && (arg_i + 1) < argc)
			luseport = atoi(argv[++arg_i]);
		else{
			fprintf(stderr,"%s is not a valid option\n", argv[arg_i]);
			exit(EXIT_FAILURE);
		}
	}
	
	/* Check that there is either a left or right address (or both) */
	if ( no_left && no_right || argc < 2){
		printf("Piggy must have either a left or right address\n");
		exit(EXIT_FAILURE);
	}

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}


	/* Set up for left side of the connection */
	/* Acts like a server so programs can connect to piggy */
		struct sockaddr_in left_sad; /* structure to hold server's address */
		struct hostent *laddr_hostent = NULL; /* stores IP address associated with laddr if laddr is set */
		int left_sock; /* socket descriptors */
		int left_port; /* protocol port number */
	if (!no_left){
	
		memset((char *)&left_sad,0,sizeof(left_sad)); /* clear sockaddr structure */
		left_sad.sin_family = AF_INET; /* set family to Internet */
		left_sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

		/* port value given by constant PROTOPORT */
		left_port = PROTOPORT; /* use default port number */
		if (left_port > 0) /* test for illegal value */
			left_sad.sin_port = htons((u_short)left_port);
		else { /* print error message and exit */
			fprintf(stderr,"bad port number %s\n",argv[1]);
			exit(EXIT_FAILURE);
		}
		
		/* Create a socket */
		left_sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
		if (left_sock < 0) {
			fprintf(stderr, "socket creation failed\n");
			exit(EXIT_FAILURE);
		}

		/* if laddr is set to none wildcard value */
		/* Convert laddr to equivalant IP address and save to compare to the address
		   of the incoming client */
		if (laddr != NULL && strcmp(laddr,"*") != 0){
			laddr_hostent = gethostbyname(laddr);
			if ( ((char *)laddr_hostent) == NULL ) {
				fprintf(stderr,"invalid host: %s\n", laddr);
				exit(EXIT_FAILURE);
			}
		}

		/* Bind address and port in left_sad to left_sock. */
		if (bind(left_sock, (struct sockaddr *)&left_sad, sizeof(left_sad)) < 0) {
			fprintf(stderr,"bind failed\n");
			exit(EXIT_FAILURE);
		}

		/* Specify size of request queue */
		if (listen(left_sock, QLEN) < 0) {
			fprintf(stderr,"listen failed\n");
			exit(EXIT_FAILURE);
		}
	}


	/* Right side of connection */
	/* Acts like a client connection to a server */
	struct hostent *ptrh; /* pointer to a host table entry */
	struct sockaddr_in right_sad; /* structure to hold an IP address */
	int right_sock; /* socket descriptor for left side*/
	int right_port; /* protocol port number */
		
	if (!no_right && raddr != NULL){
		memset((char *)&right_sad,0,sizeof(right_sad)); /* clear sockaddr structure */
		right_sad.sin_family = AF_INET; /* set family to Internet */

		/* port value given by constant PROTOPORT */
		right_port = PROTOPORT; /* use default port number */
		if (right_port > 0) /* test for legal value */
			right_sad.sin_port = htons((u_short)right_port);
		else { /* print error message and exit */
			fprintf(stderr,"bad port number %s\n",argv[2]);
			exit(EXIT_FAILURE);
		}	

		/* Convert host name to equivalent IP address and copy to sad. */
		ptrh = gethostbyname(raddr);
		if ( ((char *)ptrh) == NULL ) {
			fprintf(stderr,"invalid host: %s\n", raddr);
			exit(EXIT_FAILURE);
		}	
		memcpy(&right_sad.sin_addr, ptrh->h_addr, ptrh->h_length);
		
		/* Create a socket. */
		right_sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
		if (right_sock < 0) {
			fprintf(stderr, "socket creation failed\n");
			exit(EXIT_FAILURE);
		}

		/* Connect the socket to the specified server. */
		if (connect(right_sock, (struct sockaddr *)&right_sad, sizeof(right_sad)) < 0) {
			fprintf(stderr,"connect failed\n");
			exit(EXIT_FAILURE);
		}

	} else if (!no_right && raddr == NULL){
		fprintf(stderr,"must specify -raddr or set -noright\n");
		exit(EXIT_FAILURE);
	}

	/* Pass data from left_sock to right_sock */	
	/* Main server loop - accept and handle requests */
	struct sockaddr_in cad; /* structure to hold client's address */
	int sd2; /* socket descriptor for accept socket */
	int alen; /* length of address */
	char buf[1000]; /* buffer for string the server sends */
	int n; /* number of characters read */
	while (1) {
		alen = sizeof(cad);
		if ( !no_left && (sd2=accept(left_sock, (struct sockaddr *)&cad, &alen)) < 0) {
			fprintf(stderr, "accept failed\n");
			exit(EXIT_FAILURE);
		} else if (no_left)
			sd2 = 0; /* set left side to stdin */
	
		/* if -laddr was set then check if connecting IP matches. If not skip request */
		if (laddr_hostent != NULL) {	
			char straddr[INET_ADDRSTRLEN];	
			struct in_addr addr, addr2;
			memcpy(&addr, laddr_hostent->h_addr_list[0], sizeof(struct in_addr));
			if (strcmp(inet_ntoa(addr), inet_ntop(AF_INET, &cad.sin_addr,straddr, sizeof(straddr))) != 0){
				closesocket(sd2);
				continue;
			}
		}

		/* Repeatedly read data from socket and write to user's screen. */
		n = read(sd2, buf, sizeof(buf));
		while (n > 0) {
			write(right_sock,buf,n);
			n = read(sd2, buf, sizeof(buf));
		}
		
		closesocket(sd2);
	}

	/* Close the sockets. */	
	closesocket(right_sock);
	closesocket(left_sock);

	/* Terminate the client program gracefully. */
	exit(0);
}
