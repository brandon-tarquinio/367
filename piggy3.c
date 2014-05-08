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
#include <locale.h>
#include <curses.h>
#define PROTOPORT 36790 /* default protocol port number */
#define QLEN 6 /* size of request queue */
extern int errno;
char localhost[] = "localhost"; /* default host name */
struct protoent *ptrp; /* pointer to a protocol table entry */

/* Data structures for ncurses. */
#define MAXWIN 6
static WINDOW *w[MAXWIN];
static ww[MAXWIN];
static wh[MAXWIN];
static wrpos[MAXWIN];
static wcpos[MAXWIN];
static chtype ls,rs,ts,bs,tl,tr,bl,br;
#define IN_L  1 // top left window
#define OUT_R 2 // top right window 
#define OUT_L 3 // bottom left window
#define IN_R  4 // bottom right window
#define IO    5 // bottom window
#define BACKSPACE 127 
#define ENTER    13

/* delete in w[i] the characters until eol. Doesn't clobber boarders*/
void wClrtoeol(int i);

/* function that creates, binds, and calls listen on a socket. */
int create_server(int port);

/* Returns a socket descriptor of a socket connected to given
*  host (IP or host name) and port */
int create_client(char *host, int port);

/* Add string to window specified by i */
void wAddstr(int i, char s[132]);

/* Print buffer from 0 to n into the window w[i]*/
void wAddnstr(int i, char s[1000],int n);

/* Check that string contains a numeric value */
int is_numeric(char *s){
	while (*s){
		if (!(*s >= '0' || *(s++) <= '9'))
			return 0;
	}
	return 1;
}

/*------------------------------------------------------------------------
* Program: piggy3
*
* Purpose: Network middleware where a client can connect to the left side
* of piggy and piggy can also connect to a server with an address specified
* by -raddr option. Alternatively if -noright option is set then piggy will
* echo what is read from the left.  
*
* Syntax: piggy -option 
*
* Options:
* laddr    - specifies what address the server can accept. 
* raddr    - specifies what address piggy should connect to.
* noleft   - flags that piggy should use stdin for it's left side.
* noright  - flags that piggy should use stdout for it's right side.
* luseport - Port to use for left side server.
* loopr    - Data from the left that would be written to the right is looped back to the left.
* loopl    - Data from the right that would be written to the left is looped back to the right.
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
	struct timeval timeout;
	timeout.tv_sec = .5;

	/* loop through arguments and set values */
	bool no_left  = false; /* holds value of command line argument -noleft */
	bool no_right = false; /* Holds value of command line argument -no_right */
	char *laddr   = NULL;  /* Holds the address of the left connect as either an IP address or DNS name*/
	char *raddr   = NULL;  /* Holds the address of the left connect as either an IP address or DNS name*/
	int lacctport = 0;     /* Holds the port number will be accepted on the left connection */
	int luseport  = 0;     /* Holds the port number that will be assigned to the left connection server */
	int rport     = 0;     /* Holds the port number used when making the connection the raddr */
	bool loopr    = false;    /* Holds value of command line argument -loopr */
	bool loopl    = false;    /* Holds value of command line argument -loopl */
	
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
		else if (strcmp(argv[arg_i], "-rport") == 0 && (arg_i + 1) < argc)
			rport = atoi(argv[++arg_i]);
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
	
	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set up ncurses
/************************************************************************************************************************************/

	int i,j,a,b,c,d,nch; 
  	chtype ch;
  	char response[132];
  	ch=(chtype) " ";
	ls=(chtype) 0;
	rs=(chtype) 0;
	ts=(chtype) 0;
	bs=(chtype) 0;
	tl=(chtype) 0;
	tr=(chtype) 0;
	bl=(chtype) 0;
	br=(chtype) 0;
	setlocale(LC_ALL,""); // this has to do with the character set to use
	initscr();            // must always call this (or newterm) to initialize the
			      // library before any routines that deal with windows
			      // or the screen can be called
	  // set up some foreground / background color pairs

	cbreak();          // this allows use to get characters as they are typed
			     // without the need for the userpressing the enter key 
	noecho();          // this means the characters typed are not 
			     // shown on the screen unless we put them there
	nonl();            // this means don't translate a newline character
			     // to a carraige return linefeed sequence on output
	intrflush(stdscr, FALSE);  // 
	keypad(stdscr, TRUE);       // 
	  
	if (!(LINES==43) || !(COLS==132) ){ 
	      if (resizeterm(43,132)==ERR){
			clear();
			move(0,0);
			addstr("Piggy3 requires a screen size of 132 columns and 43 rows");
			move(1,0);
			addstr("Set screen size to 132 by 43 and try again");
			move(2,0);
			addstr("Press enter to terminate program");
			refresh();
			getstr(response);            // Pause so we can see the screen 
			endwin();
			exit(EXIT_FAILURE);
		}
	}
	clear();             // make sure screen is clear before we start
	w[0]=newwin(0,0,0,0);
	touchwin(w[0]);
	wmove(w[0],0,0);
	wrefresh(w[0]);

	// Create the 5 windows
	a = 18; b=66; c=0; d=0;
	w[1]=subwin(w[0],a,b,c,d);
	w[2]=subwin(w[0],a,b,c,b);
	w[3]=subwin(w[0],a,b,a,c);
	w[4]=subwin(w[0],a,b,a,b);
	w[5]=subwin(w[0],7,132,36,c);
	for (i=1; i < MAXWIN - 1; i++){
		ww[i]= b - 1;
		wh[i]= a - 1;
	}
	ww[5]= 131;
	wh[5]= 6;
	for (i=1;i < MAXWIN; i++){
		wattron(w[i], A_STANDOUT);
		wborder(w[i],ls,rs,ts,bs,tl,tr,bl,br);
		wrefresh(w[i]);
		wattroff(w[i], A_STANDOUT);
		wrpos[i] = 1;
		wcpos[i] = 1;
	}
	wAddstr(IN_L,"Data arriving from the left:\n");
	wAddstr(OUT_R,"Data leaving from left to right:\n");
	wAddstr(OUT_L,"Data leaving from right to left:\n");
	wAddstr(IN_R,"Data arriving from the right:\n");
	wrefresh(w[0]);
	
	i = 0;
	wmove(w[IO],1,1);// start curser in IO window at top left

/*****************************************************************************************************************/
// create initial sockets
/*****************************************************************************************************************/
	/* Set up for left side of the connection */
	/* Acts like a server so programs can connect to piggy */
	int left_sock = -1; /* socket descriptors */
	struct hostent *laddr_hostent = NULL; /* stores IP address associated with laddr if laddr is set */
	if (!no_left){
		/* if laddr is set to non wildcard value */
		/* Convert laddr to equivalant IP address and save to compare to the address
	  	 of the incoming client */
		if (laddr != NULL && strcmp(laddr,"*") != 0){
			char s[INET_ADDRSTRLEN];	
			laddr_hostent = gethostbyname(laddr);
			inet_ntop(AF_INET,laddr_hostent->h_addr_list[0],s,sizeof(s));
			//wprintw(w[IO],"the value of laddr is %s and the value of laddr_hostent is %s",laddr,s);	
			if ( ((char *)laddr_hostent) == NULL ) {
				fprintf(stderr,"invalid host: %s. Defaulting to allowing any address.\n", laddr);
				laddr_hostent = NULL;
			}
		}

		/* If luseport is not set then use protoport value */
		if (luseport == 0)
			luseport = PROTOPORT; 
		if((left_sock = create_server(luseport)) != -1){	
			FD_SET(left_sock, &inputs);
			if (left_sock > max_fd)
				max_fd = left_sock;}
		else
			wAddstr(IO,"An error has occured creating the left connection. Piggy does not have a left side.\n");
		
	}

	/* Right side of connection */
	/* Acts like a client connection to a server */
	int right_sock = -1; /* socket descriptor for left side*/
	if (!no_right && raddr != NULL){
		/* If rport was not set by the command line then use default PROTOPORT */
		if (rport == 0)
			rport = PROTOPORT;
		if ((right_sock = create_client(raddr, rport)) != -1){
			wAddstr(IO,"Piggy has a valid right connection.\n");
			FD_SET(right_sock, &inputs);
			if (right_sock > max_fd)
				max_fd = right_sock;}
		else
			wAddstr(IO,"An error has occured creating the right connection. Piggy does not have a right side.\n");
	}
	else if (!no_right && raddr == NULL){
		wAddstr(IO,"Must specify -raddr or set -noright\n");
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
	char *commands[3];	
	char command1[10];
	char command2[20];
	char command3[20];
	commands[0] = command1;
	commands[1] = command2;
	commands[2] = command3;
	int command_lengths[3];
	int command_i;
	int command_count;
		
	bool outputr = true;
	bool outputl = false;
	if (no_right){
		outputr = false;
		outputl = true;
	}
	fd_set inputs_loop = inputs;
	while (1) {
		wmove(w[IO],wrpos[IO],wcpos[IO]);
		inputs_loop = inputs;
		input_ready = select(max_fd+1,&inputs_loop,NULL,NULL,&timeout);

		/* accepts incoming client from left side and assigns to sd2 */	
		if (left_sock != -1 && FD_ISSET(left_sock,&inputs_loop)){	
			alen = sizeof(cad);
			if ( !no_left && (sd2=accept(left_sock, (struct sockaddr *)&cad, &alen)) < 0) {
				wAddstr(IO,"Accept failed on left side. \n");
			} 
			
			/* if -laddr was set then check if connecting IP matches. If ot skip request */
			if (laddr_hostent != NULL) {
				char straddr[INET_ADDRSTRLEN];
				char s[INET_ADDRSTRLEN];	
				struct in_addr addr;
				memcpy(&addr, laddr_hostent->h_addr_list[0], sizeof(struct in_addr));
				inet_ntop(AF_INET,laddr_hostent->h_addr_list[0],s,sizeof(s));
				if (strcmp(inet_ntoa(addr), inet_ntop(AF_INET, &cad.sin_addr,straddr, sizeof(straddr))) != 0){
					wAddstr(IO,"Piggy rejected a left connection.\n");
					closesocket(sd2);
					sd2 = -1;	
					continue;
				}
			}
			wAddstr(IO,"Piggy established a valid left connection.\n");
				
			/* add sd2 to inputs */
			if (sd2 > 0){
				FD_SET(sd2,&inputs);
				if (sd2 > max_fd)
					max_fd = sd2;
			}
		}
		
		/* read input from stdin */
		char cur_char;
		halfdelay(20);	
		if ( (cur_char = wgetch(w[IO])) != ERR){
			wrefresh(w[5]);
			int cur_y,cur_x;
			getyx(w[IO],cur_y,cur_x);
			wrpos[IO]=cur_y;
  			wcpos[IO]=cur_x;

			/* Process input commands */
			/* Insert Mode */
			if (cur_char == 'i'){
				/* show that the user has entered insert mode */
				mvwprintw(w[IO],wh[IO] -1,1,"-- INSERT --");
				wrpos[IO]=cur_y;
				wcpos[IO]=cur_x;
				wmove(w[IO],wrpos[IO],wcpos[IO]);
				/* parse input */	
				while ((cur_char = wgetch(w[IO])) != 27){
					if (cur_char == BACKSPACE){ // A backspace
						if (stdin_n != 0){
							--stdin_n;
							mvwaddch(w[IO],wrpos[IO],wcpos[IO],' ');
							wmove(w[IO],wrpos[IO],wcpos[IO]);
							wcpos[IO]--;
						}}
					else if (cur_char == ENTER){ // An enter
						stdin_buf[stdin_n++] = '\n';
						if (++wrpos[IO] == wh[IO] - 1)
							wrpos[IO] = 1;
				
						wcpos[IO] = 1;
						wmove(w[IO],wrpos[IO],wcpos[IO]);}
					else if ((cur_char >= 32) && (cur_char != 127)){
						if (++wcpos[IO] == ww[IO]){
							wcpos[IO]=1;
							if (++wrpos[IO]==wh[IO] -1)
								wrpos[IO]=1;
						}
						stdin_buf[stdin_n++] = cur_char;
						mvwaddch(w[IO],wrpos[IO],wcpos[IO],cur_char);
					}	
					wrefresh(w[5]);
				}
				/* Clean up */
				stdin_buf[stdin_n] = 0;// make it a proper string	
				for (i = 1; i < wh[IO]; i++){
					for (j = 1; j < ww[IO]; j++)
						mvwaddch(w[IO],i,j,' ');
				}
				wrpos[IO] = wcpos[IO] = 1;
				wmove(w[IO],wrpos[IO],wcpos[IO]);		
			}	
			/* Command Mode */	
			else if (cur_char == ':'){
				mvwaddch(w[IO],wh[IO]-1, 1,':');
				nocbreak();
				echo();
				/* Put command into commands[]*/
				command_count = 0;
				command_lengths[command_count] = 0;
				while ((cur_char = wgetch(w[IO])) != EOF && cur_char != '\n'){
					if (cur_char == ' '){
						commands[command_count][command_lengths[command_count]] = 0; // make command i proper
						if (++command_count > 2){
							wAddstr(IO,"No valid commands have more than two args.");
							break;
						}
						command_lengths[command_count] = 0;
					}
					else	
						commands[command_count][command_lengths[command_count]++] = cur_char;
				}
				if (command_lengths[command_count])
					commands[command_lengths[command_count]] = 0;	
				cbreak();
				noecho();
				wrpos[IO]=wh[IO] -1;
				wcpos[IO]=1;
				wClrtoeol(IO);
				wmove(w[IO],1,1);
				/* Check if valid command and process it*/
				if (strncmp(commands[0],"q",command_lengths[0]) == 0){
					/* Close the sockets. */	
					closesocket(right_sock);
					closesocket(left_sock);
					if (sd2 != -1)
						closesocket(sd2);	
					/* Put piggy out to paster */
					endwin();
					exit(0);}
				else if (strncmp(commands[0],"dropl",command_lengths[0]) == 0){
					if (left_sock != -1){
						if (sd2 != -1){
							closesocket(sd2);
							FD_CLR(sd2,&inputs);
							sd2 = -1;
						}
						closesocket(left_sock);
						FD_CLR(left_sock,&inputs);
						left_sock = -1;
						wAddstr(IO,"Dropped the left side connection.\n");} 
					else
						wAddstr(IO,"No left side connection to drop.\n");}
				else if (strncmp(commands[0],"dropr",command_lengths[0]) == 0){
					if (right_sock != -1){
						closesocket(right_sock);
						FD_CLR(right_sock,&inputs);
						right_sock = -1;
						wAddstr(IO,"Dropped the right side connection.\n");}
					else
						wAddstr(IO,"No right side connection to drop.\n");}
				else if (strncmp(commands[0],"output",command_lengths[0]) == 0){
					if (outputr)
						wAddstr(IO,"The current output direction for insert mode is to the right.\n");
					else 
						wAddstr(IO,"The current output direction for insert mode is to the left. \n");}
				else if (strncmp(commands[0],"outputl",command_lengths[0]) == 0){
					outputr = false;
					outputl = true;}
				else if (strncmp(commands[0],"outputr",command_lengths[0]) == 0){
					outputl = false;
					outputr = true;}	
				else if (strncmp(commands[0],"loopl",command_lengths[0]) == 0){
					loopr = false;
					loopl = true;}	
				else if (strncmp(commands[0],"loopr",command_lengths[0]) == 0){
					loopl = false;
					loopr = true;}	
				else if (strncmp(commands[0],"luseport",command_lengths[0]) == 0){
					if (command_lengths[1] > 0)
						luseport = atoi(commands[1]);
					else
						wAddstr(IO,"Must specify valid port number after :luseport\n");}
				else if (strncmp(commands[0],"lacctport",command_lengths[0]) == 0){
					if (command_lengths[1] > 0)
						lacctport = atoi(commands[1]);
					else
						wAddstr(IO,"Must specify valid port number after :lacctport\n");}
				else if (strncmp(commands[0],"laccptip", command_lengths[0]) == 0){
					if (command_lengths[1] > 0)
						laddr = commands[1];
					else
						wAddstr(IO,"Must specify valid IP number after :lacctip\n");}
				
				else
					wprintw(w[IO],"Not a valid command :%s\n",commands[0]);	
			}
		}
		
		/* read from left side. */	
		if (!no_left && FD_ISSET(sd2,&inputs_loop)){
			if ((left_n = read(sd2,left_buf,sizeof(left_buf))) == 0){
				wAddstr(IO,"Lost connection to left side. \n");	
				closesocket(sd2);
				FD_CLR(sd2, &inputs);
				sd2 = -1;}
			else // Display input from left to top left corner
				wAddnstr(IN_L,left_buf,left_n);
		}
		
		/* read from right side. */
		if (!no_right && FD_ISSET(right_sock, &inputs_loop)){
			if ((right_n = read(right_sock, right_buf,sizeof(right_buf))) == 0){
				wAddstr(IO,"Lost connection to right side. \n");
				closesocket(right_sock);
				FD_CLR(right_sock, &inputs);
				right_sock = -1;}
			else // Display input from right to bottom right corner
				wAddnstr(IN_R,right_buf,right_n);	
		}
	
		/* output contents of stdin_buf */
		if (stdin_n !=0){
			if (outputr && right_sock != -1){
				write(right_sock,stdin_buf,stdin_n);
				wAddnstr(OUT_R,stdin_buf,stdin_n);}
			else if (outputl && sd2 != -1){
				write(sd2, stdin_buf,stdin_n);
				wAddnstr(OUT_L,stdin_buf,stdin_n);}
			else
				wAddstr(IO,"Unable to output string. Check your output and loop settings are correct and try again.\n");
			
			stdin_n = 0;
		}
	
		/* Output contents of left and right buffer if data is present */
		if (left_n != 0){
			if (!loopr && right_sock != -1){
				write(right_sock,left_buf,left_n);
				wAddnstr(OUT_R,left_buf,left_n);}
			else if (loopr && sd2 != -1){
				write(sd2,left_buf,left_n);
				wAddnstr(OUT_L,left_buf,left_n);}
			left_n = 0;
		}
		if (right_n != 0){
			if (!loopl && sd2 != -1){
				write(sd2,right_buf,right_n);
				wAddnstr(OUT_L,right_buf,right_n);}
			else if (loopl && right_sock != -1){
				write(right_sock,right_buf,right_n);
				wAddnstr(OUT_R,right_buf,right_n);}
			right_n = 0;
		}
	}
}

/****************************************************************************************/
// Functions for curses
/****************************************************************************************/
/* clears the current line in w[i]. Doesn't clobber boarders*/
void wClrtoeol(int i){
	int row = wrpos[i];
	int col;
	for (col = 1; col < ww[i]; col++)
		mvwaddch(w[i],row,col,' ');
}
/* Prints string to window w[i] */
void wAddstr(int i, char s[132]){
	int j,l,y,x;
	getyx(w[i],y,x);      // find out where we are in the window
  	y=y?y:!y;
  	x=x?x:!x;  
  	wrpos[i]=y;
  	wcpos[i]=x;
  	l=strlen(s);
  	for (j=0;j<l;j++){ 
		if (++wcpos[i]==ww[i] -1) {
	  		wcpos[i]=1;
	  		if (++wrpos[i]==wh[i] -1)
				wrpos[i]=1; 
		}
		if (s[j] == '\n'){
			wrpos[i]++;
			wcpos[i]= 1;}
		else	
      			mvwaddch(w[i],wrpos[i],wcpos[i],(chtype) s[j]);   
    	}
  	wrefresh(w[i]);
}

/* Prints from 0 to n of buffer to window w[i]*/ 
void wAddnstr(int i, char s[1000],int n){
	int j,l,y,x;
	getyx(w[i],y,x);      // find out where we are in the window
  	y=y?y:!y;
  	x=x?x:!x;  
  	wrpos[i]=y;
  	wcpos[i]=x;
  	for (j=0;j<n;j++){
      		if (++wcpos[i]==ww[i] -1){
	  		wcpos[i]=1;
	  		if (++wrpos[i]==wh[i] -1) 
				wrpos[i]=1;
		}
		if (s[j] == '\n'){
			wrpos[i]++;
			wcpos[i]=1;}
		else	
      			mvwaddch(w[i],wrpos[i],wcpos[i],(chtype) s[j]); 
    	}
  	wrefresh(w[i]);
}

/******************************************************************************************/
// Functions for networking.
/******************************************************************************************/

/* function that creates, binds, and calls listen on a socket. */
/* Returns socket_id of created socket or -1 if a failure occured */
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
		wAddstr(IO,"Socket creation failed.\n");
		return(-1);
	}

	/* Eliminate "Address already in use" eroor message. */
	int flag = 1;
	if (setsockopt(server_sock, SOL_SOCKET,SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
		wAddstr(IO,"Setsockopt to SO_REUSEADDR failed.\n");
	}

	/* Bind address and port in left_sad to left_sock. */
	if (bind(server_sock, (struct sockaddr *)&server_sad, sizeof(server_sad)) < 0) {
		wAddstr(IO,"Bind failed.\n");
		return(-1);
	}

	/* Specify size of request queue */
	if (listen(server_sock, QLEN) < 0) {
		wAddstr(IO,"Listen failed.\n");
		return(-1);
	}

	/* return socket descriptor */
	return server_sock;	
}

/* Returns a socket descriptor of a socket connected to given
*  host (IP or host name) and port */
/* Returns socket_id of created socket or -1 if a failer occured */
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
		wAddstr(IO,"Socket creation failed.\n");
		return(-1);
	}

	/* Connect the socket to the specified server. */
	if (connect(client_sock, (struct sockaddr *)&client_sad, sizeof(client_sad)) < 0) {
		wAddstr(IO,"Connect failed.\n");
		return(-1);
	}

	/* return the created socket descriptor */
	return client_sock;
}	
