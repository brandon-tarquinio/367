/* client.c - code for example client program that uses TCP */
#define closesocket close
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define PROTOPORT 36790 /* default protocol port number */
#define QLEN 6 /* size of request queue */
extern int errno;
typedef enum { false, true } bool;
char localhost[] = "localhost"; /* default host name */
struct protoent *ptrp; /* pointer to a protocol table entry */

/* function that creates, binds, and calls listen on a socket. */
int create_server(int port);

/* Returns a socket descriptor of a socket connected to given
*  host (IP or host name) and port */
int create_client(char *host, int port);

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
	/* setup for select */
	int input_ready, max_fd;
	fd_set inputs; /* set of selector to be passed to select */
	FD_ZERO(&inputs);
	FD_SET(0,&inputs); /* add stdin to input fd_set */
	max_fd = 0;

	/* loop through arguments and set values */
	bool no_left = false;  /* holds value of command line argument -noleft */
	bool no_right = false; /* Holds value of command line argument -no_right */
	char *laddr = NULL;    /* Holds the address of the left connect as either an IP address or DNS name*/
	char *raddr = NULL;    /* Holds the address of the left connect as either an IP address or DNS name*/
	int lacctport = 0;     /* Holds the port number will be accepted on the left connection */
	int luseport = 0;      /* Holds the port number that will be assigned to the left connection server */
	bool dsplr = false;    /* Holds value of command line argument -dsplr */
	bool dsprl = false;    /* Holds value of command line argument -dsprl */
	bool loopr = false;    /* Holds value of command line argument -loopr */
	bool loopl = false;    /* Holds value of command line argument -loopl */
	
	int arg_i;
	for (arg_i = 1; arg_i < argc; arg_i++){
		if (strcmp(argv[arg_i],"-noleft") == 0)
			no_left = true;
		else if (strcmp(argv[arg_i], "-noright") == 0)
			no_right = true;
		else if (strcmp(argv[arg_i], "-laddr") == 0 && (arg_i + 1) < argc)
			laddr = argv[++arg_i]; /* Note that this will also move arg_i past val */
		else if (strcmp(argv[arg_i], "-raddr") == 0 && (arg_i + 1) < argc)
			raddr = argv[++arg_i]; /* Note that this will also move arg_i past val */
		else if (strcmp(argv[arg_i], "-lacctport") == 0 && (arg_i + 1) < argc)
			lacctport = atoi(argv[++arg_i]);
		else if (strcmp(argv[arg_i], "-luseport") == 0 && (arg_i + 1) < argc)
			luseport = atoi(argv[++arg_i]);
		else if (strcmp(argv[arg_i], "-dsplr") == 0)
			dsplr = true;
		else if (strcmp(argv[arg_i], "-dsprl") == 0)
			dsprl = true;
		else if (strcmp(argv[arg_i], "-loopr") == 0)
			loopr = true;
		else if (strcmp(argv[arg_i], "-loopl") == 0)
			loopl = true;
		else{
			fprintf(stderr,"%s is not a valid option.\n", argv[arg_i]);
			exit(EXIT_FAILURE);
		}
	}
	
	/* Check that there is either a left or right address (or both) */
	if ( no_left && no_right || argc < 2){
		printf("Piggy must have either a left or right address.\n");
		exit(EXIT_FAILURE);
	}
	
	/* Check that display was not set to left and right */
	if (dsplr == true && dsprl == true){
		printf("dsplr and dsprl should not both be set. Defaulting to dsplr.\n");
		dsprl = false;
	}

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}


	/* Set up for left side of the connection */
	/* Acts like a server so programs can connect to piggy */
	int left_sock; /* socket descriptors */
	int left_port = PROTOPORT; /* protocol port number. Set to default*/
	if (!no_left){
		/* If luseport is set then set left_sock to given value */
		if (luseport != 0)
			left_port = luseport; 
		left_sock = create_server(left_port);	
		FD_SET(left_sock, &inputs);
		if (left_sock > max_fd)
			max_fd = left_sock;
	} 
	
	/* if laddr is set to none wildcard value */
	/* Convert laddr to equivalant IP address and save to compare to the address
	   of the incoming client */
	struct hostent *laddr_hostent = NULL; /* stores IP address associated with laddr if laddr is set */
	if (laddr != NULL && strcmp(laddr,"*") != 0){
		laddr_hostent = gethostbyname(laddr);
		if ( ((char *)laddr_hostent) == NULL ) {
			fprintf(stderr,"invalid host: %s\n", laddr);
			exit(EXIT_FAILURE);
		}
	}

	/* Right side of connection */
	/* Acts like a client connection to a server */
	int right_sock; /* socket descriptor for left side*/
	if (no_right) 
		right_sock = 1; /* set to stdout */
	else if (!no_right && raddr != NULL){
		right_sock = create_client(raddr, PROTOPORT);
		FD_SET(right_sock, &inputs);
		if (right_sock > max_fd)
			max_fd = right_sock;
	}
	else if (!no_right && raddr == NULL){
		fprintf(stderr,"must specify -raddr or set -noright\n");
		exit(EXIT_FAILURE);
	}

	/* Pass data from left_sock to right_sock */	
	/* Main server loop - accept and handle requests */
	struct sockaddr_in cad; /* structure to hold client's address */
	int sd2; /* socket descriptor for accept socket */
	int alen; /* length of address */
	char inbuf[1000]; /* buffer for string the server reads*/
	int n_in = 0; /* number of characters read from input stream */
	char outbuf[1000]; /* buffer for string the server sends */
	int n_out = 0; /* number of characters read to go to output stream*/
	fd_set inputs_loop = inputs;
	while (1) {
		inputs_loop = inputs;
		input_ready = select(max_fd+1,&inputs_loop,NULL,NULL,NULL);
		fprintf(stderr,"hello\n");

		/* accepts incoming client from left side and assigns to sd2 */	
		if (!no_left && FD_ISSET(left_sock,&inputs_loop)){	
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
			
			/* add sd2 to inputs */
			FD_SET(sd2,&inputs);
			if (sd2 > max_fd)
				max_fd = sd2;
		}
		
		/* read input from stdio */	
		if (FD_ISSET(0,&inputs_loop)){
			n_out = read(0,outbuf,sizeof(outbuf));
		}
		
		/* read from left side. */	
		if (!no_left && FD_ISSET(sd2,&inputs_loop)){
			if ((n_in = read(sd2,inbuf,sizeof(inbuf))) == 0){
				closesocket(sd2);
				FD_CLR(sd2, &inputs);
			} 
		}
		
		/* read from right side. */
		if (!no_right && FD_ISSET(right_sock, &inputs_loop)){
			n_in = read(right_sock, inbuf,sizeof(inbuf));
		}

		/* output contents of buffer */
		if (no_left && right_sock != 0 && n_out != 0){ // write stdin to right_sock
			write(right_sock,outbuf,n_out);
			n_out = 0;
		} else if (no_right && sd2 != 0 && n_out != 0){ // write stdin to left_sock
			write(sd2,outbuf,n_out);
			n_out = 0;
		}		
		
		if (n_in != 0){	
			write(1,inbuf,n_in);	
			n_in = 0;
		}
	}

	/* Close the sockets. */	
	closesocket(right_sock);
	closesocket(left_sock);

	/* Terminate the client program gracefully. */
	exit(0);
}


/* function that creates, binds, and calls listen on a socket. */
int create_server(int port){
	struct sockaddr_in server_sad; /* structure to hold server's address */
	int server_sock; /* socket descriptors */
		
	memset((char *)&server_sad,0,sizeof(server_sad)); /* clear sockaddr structure */
	server_sad.sin_family = AF_INET; /* set family to Internet */
	server_sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

	/* port value given by constant PROTOPORT */
	if (port > 0) /* test for illegal value */
		server_sad.sin_port = htons((u_short)port);
	else { /* print error message and exit */
		fprintf(stderr,"bad port number %d\n",port);
		exit(EXIT_FAILURE);
	}
		
	/* Create a socket */
	server_sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (server_sock < 0) {
		fprintf(stderr, "socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Eliminate "Address already in use" eroor message. */
	int flag = 1;
	if (setsockopt(server_sock, SOL_SOCKET,SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	/* Bind address and port in left_sad to left_sock. */
	if (bind(server_sock, (struct sockaddr *)&server_sad, sizeof(server_sad)) < 0) {
		fprintf(stderr,"bind failed\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(server_sock, QLEN) < 0) {
		fprintf(stderr,"listen failed\n");
		exit(EXIT_FAILURE);
	}

	/* return socket descriptor */
	return server_sock;	
}

/* Returns a socket descriptor of a socket connected to given
*  host (IP or host name) and port */
int create_client(char *host, int port){
	struct hostent *ptrh; /* pointer to a host table entry */
	struct sockaddr_in client_sad; /* structure to hold an IP address */
	int client_sock; /* socket descriptor for left side*/

	memset((char *)&client_sad,0,sizeof(client_sad)); /* clear sockaddr structure */
	client_sad.sin_family = AF_INET; /* set family to Internet */

	/* check that port given is valid */
	if (port > 0) /* test for legal value */
		client_sad.sin_port = htons((u_short)port);
	else { /* print error message and exit */
		fprintf(stderr,"bad port number %d\n", port);
		exit(EXIT_FAILURE);
	}	

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ((char *)ptrh) == NULL ) {
		fprintf(stderr,"invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}	
	memcpy(&client_sad.sin_addr, ptrh->h_addr, ptrh->h_length);
	
	/* Create a socket. */
	client_sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (client_sock < 0) {
		fprintf(stderr, "socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Connect the socket to the specified server. */
	if (connect(client_sock, (struct sockaddr *)&client_sad, sizeof(client_sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}

	/* return the created socket descriptor */
	return client_sock;
}	
