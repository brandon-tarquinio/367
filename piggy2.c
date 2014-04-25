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
* Program: piggy2
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
		else
			fprintf(stderr,"%s is not a valid option. It will be ignored.\n", argv[arg_i]);
	}
	
	/* Check that there is either a left or right address (or both) */
	if ( no_left && no_right || argc < 2){
		printf("Piggy must have either a left or right side.\n");
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
	int left_sock = -1; /* socket descriptors */
	int left_port = PROTOPORT; /* protocol port number. Set to default*/
	struct hostent *laddr_hostent = NULL; /* stores IP address associated with laddr if laddr is set */
	if (!no_left){
		/* If luseport is set then set left_sock to given value */
		if (luseport != 0)
			left_port = luseport; 
		if((left_sock = create_server(left_port)) != -1){	
			FD_SET(left_sock, &inputs);
			if (left_sock > max_fd)
				max_fd = left_sock;}
		else
			fprintf(stderr,"An error has occured creating the left connection. Piggy does not have a left side.\n");

		/* if laddr is set to non wildcard value */
		/* Convert laddr to equivalant IP address and save to compare to the address
	  	 of the incoming client */
		if (laddr != NULL && strcmp(laddr,"*") != 0){
			laddr_hostent = gethostbyname(laddr);
			if ( ((char *)laddr_hostent) == NULL ) {
				fprintf(stderr,"invalid host: %s. Defaulting to allowing any address.\n", laddr);
				laddr_hostent = NULL;
			}
		}
	}

	/* Right side of connection */
	/* Acts like a client connection to a server */
	int right_sock = -1; /* socket descriptor for left side*/
	if (!no_right && raddr != NULL){
		if ((right_sock = create_client(raddr, PROTOPORT)) != -1){
			FD_SET(right_sock, &inputs);
			if (right_sock > max_fd)
				max_fd = right_sock;}
		else
			fprintf(stderr,"An error has occured creating the right connection. Piggy does not have a right side.\n");
	}
	else if (!no_right && raddr == NULL){
		fprintf(stderr,"must specify -raddr or set -noright\n");
		exit(EXIT_FAILURE);
	}

	/* Main server loop - accept and handle requests */
	struct sockaddr_in cad; /* structure to hold client's address */
	int sd2 = -1; /* socket descriptor for accept socket */
	int alen; /* length of address */
	char left_buf[1000]; /* buffer for string the server reads*/
	int left_n = 0; /* number of characters read from input stream */
	char right_buf[1000]; /* buffer for string the server sends */
	int right_n = 0; /* number of characters read to go to output stream*/
	char stdin_buf[1000]; /* buffer for insert mode */
	int stdin_n = 0; /* number of characters read in insert mode */
	bool outputr = false;
	bool outputl = false;	
	fd_set inputs_loop = inputs;
	while (1) {
		inputs_loop = inputs;
		input_ready = select(max_fd+1,&inputs_loop,NULL,NULL,NULL);

		/* accepts incoming client from left side and assigns to sd2 */	
		if (!no_left && FD_ISSET(left_sock,&inputs_loop)){	
			alen = sizeof(cad);
			if ( !no_left && (sd2=accept(left_sock, (struct sockaddr *)&cad, &alen)) < 0) {
				fprintf(stderr, "accept failed\n");
				exit(EXIT_FAILURE);
			} 
			/* if -laddr was set then check if connecting IP matches. If not skip request */
			if (laddr_hostent != NULL) {
				char straddr[INET_ADDRSTRLEN];	
				struct in_addr addr;
				memcpy(&addr, laddr_hostent->h_addr_list[0], sizeof(struct in_addr));
				if (strcmp(inet_ntoa(addr), inet_ntop(AF_INET, &cad.sin_addr,straddr, sizeof(straddr))) != 0){
					closesocket(sd2);
					sd2 = -1;	
					continue;
				}
				else
					printf("Piggy established a valid left connection.\n"); 
			}
			
			/* add sd2 to inputs */
			if (sd2 != -1){
				FD_SET(sd2,&inputs);
				if (sd2 > max_fd)
					max_fd = sd2;
			}
		}
		
		/* read input from stdin */	
		if (FD_ISSET(0,&inputs_loop)){
			/* process input commands */
			char cur_char;
			char command[10];
			int command_length = 0;
			int command_i;
			while ((cur_char = getchar()) != '\n' && cur_char != EOF){
				/* Insert Mode */
				if (cur_char == 'i'){
					getchar(); /* throw away the newline from entering i */
					printf("Now in Insert Mode (press Esc and hit Enter to exit):\n");	
					while ((cur_char = getchar()) != 27){
						stdin_buf[stdin_n++] = cur_char;
					}
					stdin_buf[stdin_n] = 0;// make it a proper string	
					printf("Leaving Insert Mode\n");
					break;}	
				/* Command Mode */	
				else if (cur_char == ':'){
					/* Put command into command[]*/
					for (command_i = 0;(cur_char = getchar()) != EOF && cur_char != '\n' && cur_char != ' '; command_i++){
						command[command_i] = cur_char;
						command_length = command_i;
					}
					command[++command_length]= 0;
					
					/* Check if valid command and set appropriate flag*/
					if (strncmp(command,"q",command_length) == 0){
						/* Close the sockets. */	
						closesocket(right_sock);
						closesocket(left_sock);
						if (sd2 != -1)
							closesocket(sd2);	
						/* Terminate the piggy gracefully. */
						exit(0);}
					else if (strncmp(command,"noleft",command_length) == 0){
						if (sd2 != -1){
							closesocket(sd2);
							FD_CLR(sd2,&inputs);
							sd2 = -1;
							printf("Dropped the left side connection.\n");} 
						else
							printf("No left side connection to drop.\n");}
					else if (strncmp(command,"noright",command_length) == 0){
						if (right_sock != -1){
							closesocket(right_sock);
							FD_CLR(right_sock,&inputs);
							sd2 = -1;
							printf("Dropped the right side connection.\n");}
						else
							 printf("No right side connection to drop.\n");}
					else if (strncmp(command,"outputl",command_length) == 0){
						if (outputr = true)
							outputr = false;
						outputl = true;}
					else if (strncmp(command,"outputr",command_length) == 0){
						if (outputl = true)
							outputr = false;
						outputr = true;}	
					else if (strncmp(command,"dsplr",command_length) == 0){
						if (dsprl = true)
							dsprl = false;
						dsplr = true;}	
					else if (strncmp(command,"dsprl",command_length) == 0){
						if (dsplr = true)
							dsplr = false;
						dsprl = true;}	
					else if (strncmp(command,"loopl",command_length) == 0){
						if (loopr = true)
							loopr = false;
						loopl = true;}	
					else if (strncmp(command,"loopr",command_length) == 0){
						if (loopl = true)
							loopl = false;
						loopr = true;}	
					else
						fprintf(stderr,"Not a valid command :%s\n",command);	
					break;
				}
				else
					break;
			}
			__fpurge(stdin);
		}
		
		/* read from left side. */	
		if (!no_left && FD_ISSET(sd2,&inputs_loop)){
			if ((left_n = read(sd2,left_buf,sizeof(left_buf))) == 0){
				fprintf(stderr,"Lost connection to left side.\n");
				closesocket(sd2);
				FD_CLR(sd2, &inputs);
				sd2 = -1;
			}
		}
		
		/* read from right side. */
		if (!no_right && FD_ISSET(right_sock, &inputs_loop)){
			if ((right_n = read(right_sock, right_buf,sizeof(right_buf))) == 0){
				fprintf(stderr,"Lost connection to right side.\n");
				closesocket(right_sock);
				FD_CLR(right_sock, &inputs);
				right_sock = -1;
			}
		}
	
		/* output contents of stdin_buf */
		if ((no_left || outputr || (!no_left && !no_right)) && !outputl && right_sock != -1 && stdin_n != 0) // write stdin to right_sock
			write(right_sock,stdin_buf, stdin_n);
		else if ((no_right || outputl) && sd2 != -1 && stdin_n != 0) // write stdin to left_sock
			write(sd2,stdin_buf, stdin_n);
	
		/* check if piggy is in the middle and transfer data according to options */
		if (!no_right && !no_left){  
			if (right_sock != -1 && left_n != 0 && !loopr){
				write(right_sock,left_buf,left_n);
				if (dsplr)
					printf("%.*s",left_n,left_buf);}
			else if (left_n != 0 && sd2 != -1 && loopr){
				write(sd2,left_buf,left_n);
				if (dsplr)
					printf("%.*s",left_n,left_buf);
			}
			if (sd2 != -1 && right_n != 0 && !loopl){
				write(sd2,right_buf,right_n);
				if (dsprl)
					printf("%.*s",right_n,right_buf);}
			else if (right_n != 0 && right_sock != -1 && loopl){
				write(right_sock,right_buf,right_n);
				if (dsprl)
					printf("%.*s",right_n,right_buf);
			}
		}	
		/* if piggy has noright set, then display left data to stdout */
		else if (no_right && left_n != 0)
			printf("%.*s",left_n,left_buf);
		/* if piggy has no_left set, then display right data to stdout */ 
		else if (no_left && right_n != 0)
			printf("%.*s",right_n,right_buf);

		/*clean up*/
		left_n = right_n = stdin_n = 0;
	}
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
		fprintf(stderr,"Bad port number %d.\n",port);
		return(-1);
	}
		
	/* Create a socket */
	server_sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (server_sock < 0) {
		fprintf(stderr, "Socket creation failed.\n");
		return(-1);
	}

	/* Eliminate "Address already in use" eroor message. */
	int flag = 1;
	if (setsockopt(server_sock, SOL_SOCKET,SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
		perror("Setsockopt to SO_REUSEADDR failed.");
	}

	/* Bind address and port in left_sad to left_sock. */
	if (bind(server_sock, (struct sockaddr *)&server_sad, sizeof(server_sad)) < 0) {
		fprintf(stderr,"Bind failed.\n");
		return(-1);
	}

	/* Specify size of request queue */
	if (listen(server_sock, QLEN) < 0) {
		fprintf(stderr,"Listen failed.\n");
		return(-1);
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
		fprintf(stderr,"Bad port number %d.\n", port);
		return(-1);
	}	

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ((char *)ptrh) == NULL ) {
		fprintf(stderr,"Invalid host: %s.\n", host);
		return(-1);
	}	
	memcpy(&client_sad.sin_addr, ptrh->h_addr, ptrh->h_length);
	
	/* Create a socket. */
	client_sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (client_sock < 0) {
		fprintf(stderr, "Socket creation failed.\n");
		return(-1);
	}

	/* Connect the socket to the specified server. */
	if (connect(client_sock, (struct sockaddr *)&client_sad, sizeof(client_sad)) < 0) {
		fprintf(stderr,"Connect failed.\n");
		return(-1);
	}

	/* return the created socket descriptor */
	return client_sock;
}	
