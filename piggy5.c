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
#include <fcntl.h>
#include <sys/stat.h>
// for external filters
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>


#define MAX_COMMAND_COUNT 13
#define PROTOPORT 36790 /* default protocol port number */
#define QLEN 6 /* size of request queue */
extern int errno;
char localhost[] = "localhost"; /* default host name */
struct protoent *ptrp; /* pointer to a protocol table entry */
/* For select */
int max_fd;
fd_set inputs; /* set of selector to be passed to select */
fd_set outputs; /* set of selectors to be passed to select for writing */

void sys_err(char *s){
	printf("%s\n",s);
	exit(1);
}
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

/* Add string to window specified by i */
void wAddstr(int i, char *s);

/* Print buffer from 0 to n into the window w[i]*/
void wAddnstr(int i, char *s,int n);

/* fills the buffer from curses getchar starting at spot n + 1 */
void fillbuf(char *buf,char char_in,int *n);

/* starts an external command and fills pipein and pipeout with the in and out pipe file descriptors */
/* command[0] must contain the name of command and the remaining cells contain the arguments. */
/* arg_count is number of arguments and must be atleast one less then size of array */
void pOpen(char **command, int arg_count, int *pipein, int *pipeout);
	
/* gets char from file descriptor */
char getChar(int fd);

/* strips non-pritable chars from given buf. n is updated to the new size */
void strip_np(char *buf,int *n);

/* Same as strip_np but does not remove LF and CR */
void strip_npxeol(char *buf, int *n);

/* set up log file from command prompt.*/
void logset(int *fd, char *file_name[],int arg_count);

/* Returns a str in the form peer_IP:peer_port */
char *peer_info(int sock);

/* Returns a str in the form my_IP:my_port */
char *my_info(int sock, int port);

/* Wrapper for accept that checks the accepted connection is from the valid ip and port.
*  Then adds it to the set of file descriptors and returns the socket */
int Accept(int sock_in,char* lacct_addr,int lacct_port);

/* function that creates, binds, and calls listen on a socket. */
int create_server(int port);

/* Returns a socket descriptor of a socket connected to given
*  host (IP or host name) and port */
int create_client(char *host, int port);

/* prints a info about socket in the form sourceIP:sourcePort:destinationIP:destinationPort*/ 
void pair_info(int passive_sock,int sock,int port);

/* wrapper for close the removes the socket from inputs and sets sock to -1 */
void Close(int *sock);

/* Check that string contains a numeric value */
int is_numeric(char *s){
	while (*s){
		if (!(*s >= '0' || *(s++) <= '9'))
			return 0;
	}
	return 1;
}

/*------------------------------------------------------------------------
* Program: piggy5
*
* Purpose: Network middleware where a client can connect to the left side
* of piggy and piggy can also connect to a server with an address specified
* by -raddr option. Alternatively if -noright option is set then piggy will
* echo what is read from the left.  
*
* Syntax: piggy -option 
*
* Options:
* lacct_addr    - specifies what address the server can accept. 
* raddr    - specifies what address piggy should connect to.
* noleft   - flags that piggy should use stdin for it's left side.
* noright  - flags that piggy should use stdout for it's right side.
* luseport - Port to use for left side server.
* loopr    - Data from the left that would be written to the right is looped back to the left.
* loopl    - Data from the right that would be written to the left is looped back to the right.
*
* The address for lacct_addr and raddr can be a dotted IP address or
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
	int input_ready;
	struct timeval timeout;
	timeout.tv_sec = .5;

	/* loop through arguments and set values */
	bool no_left  = false; 	/* holds value of command line argument -noleft */
	bool no_right	= false;/* Holds value of command line argument -no_right */
	char *lacct_addr = NULL;/* Holds the address of the left connect as either an IP address or DNS name*/
	char *raddr   = NULL; 	/* Holds the address of the left connect as either an IP address or DNS name*/
	int lacctport = -1;     	/* Holds the port number will be accepted on the left connection */
	int luseport  = -1;     	/* Holds the port number that will be assigned to the left connection server */
	int rport     = -1;     	/* Holds the port number used when making the connection the raddr */
	bool loopr    = false;  /* Holds value of command line argument -loopr */
	bool loopl    = false;  /* Holds value of command line argument -loopl */
	int script_fd = -1;	/* Holds file descriptor of script file */
	
	int arg_i;
	for (arg_i = 1; arg_i < argc; arg_i++){
		if (strcmp(argv[arg_i],"-noleft") == 0)
			no_left = true;
		else if (strcmp(argv[arg_i], "-noright") == 0)
			no_right = true;
		else if (strcmp(argv[arg_i], "-laddr") == 0 && (arg_i + 1) < argc)
			lacct_addr = argv[++arg_i]; /* Note that this will also move arg_i past val */
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
		else if (strcmp(argv[arg_i], "-s") == 0){
			if (arg_i < (argc - 1) && argv[++arg_i][0] != '-')
				script_fd = open(argv[arg_i],O_RDONLY);
			else{
				script_fd = open("scriptin",O_RDONLY);
				if (arg_i > 2)
					arg_i--;
			}}
		else
			fprintf(stderr,"%s is not a valid option. It will be ignored.\n", argv[arg_i]);
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
	wmove(w[IO],1,1);// start curser in IO window at top left
	wrefresh(w[0]);

/*****************************************************************************************************************/
// create initial sockets
/*****************************************************************************************************************/
	/* Set up for left side of the connection */
	/* Acts like a server so programs can connect to piggy */
	int left_passive_sock = -1; /* socket descriptors */
	/* If luseport is not set then use protoport value */
	if (luseport == -1)
		luseport = PROTOPORT; 
	if (!no_left){
		if((left_passive_sock = create_server(luseport)) != -1){	
			FD_SET(left_passive_sock, &inputs);
			if (left_passive_sock > max_fd)
				max_fd = left_passive_sock;}
		else
			wAddstr(IO,"An error has occured creating the left connection. Piggy does not have a left side.\n");
		
	}

	/* Right side of connection */
	/* Acts like a client connection to a server */
	int right_sock = -1; /* socket descriptor for left side*/
	if (!no_right && raddr != NULL){
		/* If rport was not set by the command line then use default PROTOPORT */
		if (rport == -1)
			rport = PROTOPORT;
		if ((right_sock = create_client(raddr, rport)) != -1){
			wAddstr(IO,"Piggy has a valid right connection.\n");
			FD_SET(right_sock, &inputs);
			if (right_sock > max_fd)
				max_fd = right_sock;}
		else
			wAddstr(IO,"An error has occured creating the right connection. Piggy does not have a right side.\n");
	}
	
	/* Main server loop - accept and handle requests */
	/* For left side */	
	int left_sock = -1; 			/* socket descriptor for accept socket */
	char *lconnect_addr = NULL;
	char left_buf[1000]; 			/* buffer for string the server reads*/
	int left_n = 0; 			/* number of characters read from input stream */
	/* For right side */
	int right_passive_sock = -1; 		/* socket descriptor for right side server */ 	
	int ruseport  = PROTOPORT;
	int racctport = -1;
	char *racct_addr = NULL;	
	char right_buf[1000]; 			/* buffer for string the server sends */
	int right_n = 0; 			/* number of characters read to go to output stream*/
	/* For keyboard input */
	const int stdin_buf_size = 1000;
	char stdin_buf[stdin_buf_size]; 	/* buffer for insert mode */
	int stdin_n = 0; 			/* number of characters read in insert mode */
	const int command_buf_size = 100;	/* constant for size of command buf */
	char command_buf[command_buf_size];	/* buffer the hold command mode input */	
	char *commands[MAX_COMMAND_COUNT];	/* holds the parsed commands from command_buf */
	int command_n = 0;			/* count of chars in command_buf */
	int command_count;
	bool command_ready;
	/* set output defaults */		
	bool outputr = true;
	bool outputl = false;
	if (no_right){
		outputr = false;
		outputl = true;
	}
	/* modes */
	bool insert = false;			/* Set to true when in insert mode. Set back to false when esc is hit in insert mode */
	bool command = false;			/* Set to true when in command mode. Set to false once enter or esc is hit in command mode */
	bool output_stdin = false;
	// loglr
	int loglrpre_fd = -1;
	int loglrpost_fd = -1;
	// logrl
	int logrlpre_fd = -1;
	int logrlpost_fd = -1;
	/* Strips */
	bool stlrnp_bool = false;
	bool stlrnpx_bool = false;
	bool strlnp_bool = false;
	bool strlnpx_bool = false;
	/* external filters */
	bool exfilter_lr_bool = false;		/* Boolean for whether an external filter has been set from left to right */
	int write_pipe_lr;			/* FD for pipe that connects to the read in of the external filter from left to right */
	int read_pipe_lr;			/* FD for pipe from read in of external filter left to right */
	bool exfilter_rl_bool = false;
	int write_pipe_rl;
	int read_pipe_rl;	
	int left_n_out = 0;
	/* script */
	char cur_char_s; // the current char when processing scripts
	fd_set inputs_loop = inputs;

	FD_ZERO(&outputs);

	//fd_set outputs_loop = outputs;
	while (1) {
		inputs_loop = inputs;
	//	outputs_loop = outputs;
		input_ready = select(max_fd+1,&inputs_loop,NULL,NULL,&timeout);
		/* accepts incoming client from left side and assigns to left_sock */	
		if (left_passive_sock != -1 && FD_ISSET(left_passive_sock,&inputs_loop))	
			if ((left_sock = Accept(left_passive_sock,lacct_addr,lacctport)) != -1)
				wAddstr(IO,"Piggy established a valid left connection.\n");
		
		if (right_passive_sock != -1 && FD_ISSET(right_passive_sock,&inputs_loop))
			if ((right_sock = Accept(right_passive_sock,racct_addr,racctport)) != -1)
				wAddstr(IO,"Piggy established a valid right connection.\n");	
		
		/* read input from curses input */
		char cur_char;
		halfdelay(10);	
		if ( (cur_char = wgetch(w[IO])) != ERR){
			if (!insert && !command){
				if ( cur_char == 'i'){
					insert = true; 
					/* show that the user has entered insert mode */
					mvwprintw(w[IO],wh[IO] -1,1,"-- INSERT --");
					wmove(w[IO],wrpos[IO],wcpos[IO]);
				}
				else if ( cur_char == ':'){
					command = true;
					wrpos[IO] = wh[IO]-1;
					wcpos[IO] = 1;
					mvwaddch(w[IO],wrpos[IO],wcpos[IO],':');
				}}
			else if (insert){
				if ( cur_char == 27){
					insert = false;
					stdin_buf[stdin_n] = 0;
					output_stdin = true;
					/*clean up */
					for (i = 1; i < wh[IO]; i++){
						for (j = 1; j < ww[IO]; j++)
							mvwaddch(w[IO],i,j,' ');
					}
					wrpos[IO] = wcpos[IO] = 1;
					wmove(w[IO],wrpos[IO],wcpos[IO]);}
				else {
					fillbuf(stdin_buf,cur_char,&stdin_n);
					if (stdin_buf[stdin_n - 1] == '\n' || stdin_n == stdin_buf_size){
						stdin_buf[stdin_n] = 0;
						output_stdin = true;
					}
				}
				

				wmove(w[IO],wrpos[IO],wcpos[IO] + 1);
				wrefresh(w[5]);
			}
			else if (command){
				if ( cur_char == 27){
					command = false;
					
					/*clean up */
					for (i = 1; i < wh[IO]; i++){
						for (j = 1; j < ww[IO]; j++)
							mvwaddch(w[IO],i,j,' ');
					}
					wrpos[IO] = wcpos[IO] = 1;
					wmove(w[IO],wrpos[IO],wcpos[IO]);}
				else {
					fillbuf(command_buf,cur_char,&command_n);
					/* Put command into commands[]*/
					if (command_buf[command_n - 1] == '\n' || command_n == command_buf_size){
						command = false;	
						command_ready = true;	
						/* clear command and reset curser to top of IO */	
						wrpos[IO] = wh[IO] -1;
						wcpos[IO] = 1;
						wClrtoeol(IO);
						wrpos[IO] = 1;
						wcpos[IO] =1;
						wmove(w[IO],wrpos[IO],wcpos[IO]);	
						wrefresh(w[IO]);
					}
				}
			}
		}
		/* parse script */
		if (script_fd != -1){
			while( (cur_char_s != EOF) && (cur_char_s != ':'))
				cur_char_s = getChar(script_fd);	
			if(cur_char_s == ':'){  
				/* check if bad input */
				if ((cur_char_s = getChar(script_fd)) == EOF){
					close(script_fd);
					script_fd = -1;
					wAddstr(IO,"End of script. \n");}
				/* check if insert mode */
				else if (cur_char_s == 'i'){
					stdin_n = 0;
					while((cur_char_s = getChar(script_fd)) != EOF && (cur_char_s != ':') && stdin_n < (stdin_buf_size - 1))
						stdin_buf[stdin_n++] = cur_char_s;  
					stdin_buf[stdin_n] = 0;
					output_stdin = true;}
				/* check if command */	
				else {
					command_n = 0;
					while((cur_char_s != EOF) && cur_char_s != '\n' && cur_char_s != ':'){
						command_buf[command_n++] = cur_char_s;
						cur_char_s = getChar(script_fd);
					}
					command_buf[command_n++] = 0;
					command_ready = true;
				}
			} else {
				close(script_fd);
				script_fd = -1;
				wAddstr(IO,"End of script.\n");
			}
		}
		
		/* if a null terminated string is in stdbuf then output it */
		if (stdin_n != 0 && output_stdin){
			if (outputr && right_sock != -1){
				/* log curses input into prelog */
				if (loglrpre_fd != -1)
					write(loglrpre_fd, stdin_buf, stdin_n);
				write(right_sock,stdin_buf,stdin_n);
				wAddnstr(OUT_R,stdin_buf,stdin_n);}
			else if (outputl && left_sock != -1){
				if (logrlpre_fd != -1)
					write(logrlpre_fd, stdin_buf, stdin_n);
				write(left_sock, stdin_buf,stdin_n);
				wAddnstr(OUT_L,stdin_buf,stdin_n);}
			else
				wAddstr(IO,"Unable to output string. Check your output and loop settings are correct and try again.\n");
			output_stdin = false;	
			stdin_n = 0;
		}
	
		/* process command if ready */
		if (command_ready){
			command_count = 0;
			for (i = 0; i < 3; i++)
				commands[i] = NULL;

			/*parse commands from command_buf and place them in commands[] */
			commands[0] = &command_buf[0];	
			for(i = 0; i < command_n - 1; i++){
				if (command_buf[i] == ' '){
					command_buf[i] = '\0'; // make command i proper
					if (++command_count > MAX_COMMAND_COUNT - 2){ // -1 for off-by-one and -1 for space to place null
						wAddstr(IO,"No valid commands have more than ten args.");
						commands[0] = ""; // so no commands are activated
						break;
					}
					/* trim extra spaces */
					while (command_buf[++i] == ' ' && i < command_n)
						;
					if (i < command_n)
						commands[command_count] = &command_buf[i];
					else 
						command_count--;
				}
			}
			command_buf[i] = 0;

			/* Check if valid command and process it*/
			if (strcmp(commands[0],"q") == 0){
				/* Close the sockets. */	
				closesocket(left_sock);
				closesocket(right_sock);
				closesocket(left_passive_sock);
				closesocket(right_passive_sock);
				/* Close the files */
				close(loglrpre_fd);
				close(loglrpost_fd);
				close(logrlpre_fd);
				close(logrlpost_fd);
				close(script_fd);
				/* Close the pipes */
				close(write_pipe_lr);
				close(read_pipe_lr);
				close(write_pipe_rl);
				close(read_pipe_rl);
				/* Put piggy out to paster */
				endwin();
				exit(0);}
			else if (strcmp(commands[0],"dropl") == 0){
				if (left_passive_sock != -1 || left_sock != -1){
					Close(&left_sock);
					Close(&left_passive_sock);
					wAddstr(IO,"Dropped the left side connection.\n");} 
				else
					wAddstr(IO,"No left side connection to drop.\n");}
			else if (strcmp(commands[0],"dropr") == 0){
				if (right_passive_sock != -1 || right_sock != -1){
					Close(&right_passive_sock);
					Close(&right_sock);
					wAddstr(IO,"Dropped the right side connection.\n");}
				else
					wAddstr(IO,"No right side connection to drop.\n");}
			else if (strcmp(commands[0],"output") == 0){
				if (outputr)
					wAddstr(IO,"The current output direction for insert mode is to the right.\n");
				else 
					wAddstr(IO,"The current output direction for insert mode is to the left. \n");}
			else if (strcmp(commands[0],"outputl") == 0){
				outputr = false;
				outputl = true;}
			else if (strcmp(commands[0],"outputr") == 0){
				outputl = false;
				outputr = true;}	
			else if (strcmp(commands[0], "lpair") == 0)
					pair_info(left_passive_sock, left_sock, luseport);
			else if (strcmp(commands[0], "rpair") == 0)
					pair_info(right_passive_sock, right_sock, ruseport);
			else if (strcmp(commands[0],"loopl") == 0){
				loopr = false;
				loopl = true;}	
			else if (strcmp(commands[0],"loopr") == 0){
				loopl = false;
				loopr = true;}	
			else if (strcmp(commands[0],"luseport") == 0){
				if (command_count == 1)
					luseport = atoi(commands[1]);
				else
					wAddstr(IO,"Must specify valid port number after :luseport\n");}
			else if (strcmp(commands[0],"ruseport") == 0){
				if (command_count = 1)
					ruseport = atoi(commands[1]);
				else
					wAddstr(IO,"Must specify valid port number after :ruseport\n");}
			else if (strcmp(commands[0],"lacctport") == 0){
				if (command_count = 1)
					lacctport = atoi(commands[1]);
				else
					wAddstr(IO,"Must specify valid port number after :lacctport\n");}
			else if (strcmp(commands[0],"racctport") == 0){
				if (command_count = 1)
					racctport = atoi(commands[1]);
				else
					wAddstr(IO,"Must specify valid port number after :racctport.\n");}
			else if (strcmp(commands[0],"laccptip") == 0){
				if (command_count = 1)
					lacct_addr = commands[1];
				else
					wAddstr(IO,"Must specify valid IP number after :lacctip\n");}
			else if (strcmp(commands[0],"racctip") == 0){
				if (command_count = 1)
					racct_addr = commands[1];
				else
					wAddstr(IO,"Must specify valid IP number after :racctip\n");}
			else if (strcmp(commands[0],"listenl") == 0){
				if (left_passive_sock != -1 || left_sock != -1)
					wAddstr(IO,"Already a left side. Use dropl and try agian\n");
				else { 
					/* If luseport is not set then use protoport value */
					if (command_count = 2)
						luseport = atoi(commands[1]);
					if((left_passive_sock = create_server(luseport)) != -1){	
						FD_SET(left_passive_sock, &inputs);
						if (left_passive_sock > max_fd)
							max_fd = left_passive_sock;}
					else
						wAddstr(IO,"An error has occured creating the left connection. Piggy does not have a left side.\n");
				}}
			else if (strcmp(commands[0],"listenr") == 0){
				if (right_passive_sock != -1 || right_sock != -1)
					wAddstr(IO,"Already a right side. Use dropr and try agian.\n");
				else { 
					/* If port specified use it. else use protoport value */
					if (command_count = 2)
						ruseport = atoi(commands[1]);
					if((right_passive_sock = create_server(ruseport)) != -1){	
						FD_SET(right_passive_sock, &inputs);
						if (right_passive_sock > max_fd)
							max_fd = right_passive_sock;}
					else
						wAddstr(IO,"An error has occured creating the right connection. Piggy does not have a right side.\n");
				}}
			else if (strcmp(commands[0],"connectl") == 0){
				if (left_passive_sock != -1 || left_sock != -1)
					wAddstr(IO,"Already a left side. Use dropl and try again.\n");
				else if (command_count = 2) {
					/* Get IP address */
					lconnect_addr = commands[1];
					/* Get port number */	
					luseport = atoi(commands[2]);
					/* Create socket */	
					if ((left_sock = create_client(lconnect_addr, luseport)) != -1){
						wAddstr(IO,"Piggy has a valid left connection.\n");
						FD_SET(right_sock, &inputs);
					if (left_sock > max_fd)
						max_fd = left_sock;}
					else
						wAddstr(IO,"An error has occured creating the left connection. Piggy does not have a left side.\n");}
				else
					wAddstr(IO,"connect not made. Please enter in form :connectl addr port#");}
			else if (strcmp(commands[0],"connectr") == 0){
				if (right_passive_sock != -1 || right_sock != -1)
					wAddstr(IO,"Already a right side. Use dropr and try again.\n");
				else if (command_count = 2){
					/* Get IP address */
					raddr = commands[1];
					/* Get port number */	
					rport = atoi(commands[2]);
					/* Create socket */	
					if ((right_sock = create_client(raddr, rport)) != -1){
						wAddstr(IO,"Piggy has a valid right connection.\n");
						FD_SET(right_sock, &inputs);
						if (right_sock > max_fd)
							max_fd = right_sock;}
					else
						wAddstr(IO,"An error has occured creating the right connection. Piggy does not have a right side.\n");}
				 else 
					wAddstr(IO,"Connect not made.Please enter in form :connectr addr port.\n");
				}
			else if (strcmp(commands[0],"read") == 0){
				if (command_count = 1){
					int read_fd = open(commands[1],O_RDONLY);
					while ((stdin_n = read(read_fd,stdin_buf,1000)) > 0){
						if (outputr && right_sock != -1){
							write(right_sock,stdin_buf,stdin_n);
							}
							//wAddnstr(OUT_R,stdin_buf,stdin_n);
						else if (outputl && left_sock != -1){
							write(left_sock, stdin_buf,stdin_n);}
						//	wAddnstr(OUT_L,stdin_buf,stdin_n);
					}
					
					if (stdin_n < 0)
						wAddstr(IO,"Error reading file.\n");	
					if (stdin_n = 0)
						wAddstr(IO,"Seccessfully read whole file.\n");
				
					/*clean up */
					close(read_fd);
					stdin_n = 0;}
				else
					wAddstr(IO,"Must specify name of file to read from.\n");}
			else if (strcmp(commands[0],"loglrpre") == 0)
				logset(&loglrpre_fd, commands, command_count);
			else if (strcmp(commands[0],"loglrpost") == 0)
				logset(&loglrpost_fd, commands, command_count);
			else if (strcmp(commands[0],"logrlpre") == 0)
				logset(&loglrpre_fd, commands, command_count);
			else if (strcmp(commands[0],"logrlpost") == 0)
				logset(&logrlpost_fd, commands, command_count);
			else if (strcmp(commands[0],"stlrnp") == 0){
				if (stlrnp_bool)
					wAddstr(IO, "stlrnp already set\n");
				else if (stlrnpx_bool || exfilter_lr_bool)
					wAddstr(IO, "A filter from left to right already exists. Only one filter in each direction is permitted");
				else {
					stlrnp_bool = true;
					wAddstr(IO, "stlrnp is now set\n");}}
			else if (strcmp(commands[0],"stlrnpxeol") == 0){
				if (stlrnpx_bool)
					wAddstr(IO, "stlrnpxeol is already set\n");
				else if (stlrnp_bool || exfilter_lr_bool)
					wAddstr(IO, "A filter from left to right already exists. Only one filter in each direction is permitted");
				else {
					stlrnpx_bool = true;
					wAddstr(IO, "stlrnpxeol is now set\n");}}
			else if (strcmp(commands[0],"strlnp") == 0){
				if (strlnp_bool)
					wAddstr(IO,"strlnp is already set\n");
				else if (strlnpx_bool || exfilter_rl_bool)
					wAddstr(IO, "A filter from right to left already exists. Only one filter in each direction is permitted");
				else {
					strlnp_bool = true;
					wAddstr(IO,"strlnp is now set\n");}}
			else if (strcmp(commands[0],"strlnpxeol") == 0){
				if (strlnpx_bool)
					wAddstr(IO,"strlnpxeol is already set\n");
				else if (strlnp_bool || exfilter_rl_bool)
					wAddstr(IO, "A filter from right to left already exists. Only one filter in each direction is permitted");
				else {
					strlnpx_bool = true;
					wAddstr(IO,"strlnpxeol is now set\n");}}
			else if (strcmp(commands[0],"source") == 0){
				if (command_count < 1){
					if ((script_fd = open("scriptin",O_RDONLY)) > 0)
						wAddstr(IO, "running script.\n");
					else
						wAddstr(IO, "error opening file");}
				else if (command_count = 1){
					if ((script_fd = open(commands[1],O_RDONLY)) > 0)
						wAddstr(IO, "running script.\n");
					else
						wAddstr(IO, "error opening file");}
				else
					wAddstr(IO,"Not valid input. :source [scriptin]\n");}
			else if (strcmp(commands[0], "externallr") == 0){
				if (stlrnpx_bool || stlrnp_bool || exfilter_lr_bool)
					wAddstr(IO, "A filter from left to right already exists. Only one filter in each direction is permitted");
				else if (command_count){
					/* Start the external command and set the read and write in of pipe */
					pOpen(&commands[1],command_count -1, &write_pipe_lr, &read_pipe_lr);	
					wAddstr(IO,"External filter from left to right has been set");
					exfilter_lr_bool = true;
					FD_SET(read_pipe_lr, &inputs);
						if (read_pipe_lr > max_fd)
							max_fd = read_pipe_lr;
					} 
				else
					wAddstr(IO, "Must specify a command to set as filter");}
			//else if (strcmp(commands[0], "externalrl"){
				
			//}
			else
				wAddstr(IO,"Not a valid command.\n");
			/* clean up */	
			wrefresh(w[IO]);
			command_n = 0;
			command_ready = false;
		}	




		/* read from left side. */	
		if (FD_ISSET(left_sock,&inputs_loop)){
			if ((left_n = read(left_sock,left_buf,sizeof(left_buf))) == 0){
				wAddstr(IO,"Lost connection to left side. \n");	
				Close(&left_sock);}
			else { // Display input from left to top left corner
				if (loglrpre_fd != -1)
					write(loglrpre_fd, left_buf, left_n);		
				wAddnstr(IN_L,left_buf,left_n);
			}
		}
		
		/* read from right side. */
		if (FD_ISSET(right_sock, &inputs_loop)){
			if ((right_n = read(right_sock, right_buf,sizeof(right_buf))) == 0){
				wAddstr(IO,"Lost connection to right side. \n");
				Close(&right_sock);}
			else{ // Display input from right to bottom right corner
				if (logrlpre_fd != -1)
					write(logrlpre_fd, right_buf, right_n);		
				wAddnstr(IN_R,right_buf,right_n);
			}	
		}
		/* read from pipe */
		if (exfilter_lr_bool && FD_ISSET(read_pipe_lr,&inputs_loop)){
			if ( (left_n_out = read(read_pipe_lr,left_buf,1000)) <= 0){
				close(read_pipe_lr);
				close(write_pipe_lr);
				exfilter_lr_bool = false;
				if (left_n_out == 0)
					wAddstr(IO, "External command has finished");
				else 
					wAddstr(IO, "An error has occured. Closing filter");
			}
			wAddstr(IO,"I am here");
		}
		/* process filters, set left_n = 0 and left_n_out = left_n */
		if (left_n != 0){
			/* if externallr was set then pass through external filter */
			if (exfilter_lr_bool){
				write(write_pipe_lr,left_buf,left_n);
			} else{
				/* process strip and post log commands then output to OUT_R */	
				if (stlrnp_bool)
					strip_np(left_buf,&left_n);
				if (stlrnpx_bool)
					strip_npxeol(left_buf, &left_n);
				left_n_out = left_n;	
			}
		
			left_n = 0;
				
		}	
		/* Output contents of left and right buffer if data is present */
 		if (left_n_out != 0){
			if (loglrpost_fd != -1)
				write(loglrpost_fd, left_buf, left_n_out);
			
			wAddnstr(OUT_R,left_buf,left_n_out);
			if (!loopr && right_sock != -1)
				write(right_sock,left_buf,left_n_out);
			else if (loopr && left_sock != -1){
				wAddnstr(IN_R,left_buf,left_n_out);
				if (logrlpre_fd != -1)
					write(logrlpre_fd, left_buf, left_n_out);
				if (strlnp_bool)
					strip_np(left_buf, &left_n);
				
				if (strlnpx_bool)
					strip_npxeol(left_buf, &left_n);
				if (logrlpost_fd != -1)
					write(logrlpost_fd, left_buf, left_n);
				write(left_sock,left_buf,left_n);
				wAddnstr(OUT_L,left_buf,left_n);
			}
			left_n_out = 0;
		}
		if (right_n != 0){
			/* process strip and post log commands then output to OUT_L */
			if (strlnp_bool)
				strip_np(right_buf, &right_n);
			if (strlnpx_bool)
				strip_npxeol(right_buf, &right_n);
			if (logrlpost_fd != -1)
				write(logrlpost_fd, right_buf, right_n);
			wAddnstr(OUT_L,right_buf,right_n);

			if (!loopl && left_sock != -1)
				write(left_sock, right_buf, right_n);
			else if (loopl && right_sock != -1){
				if (loglrpre_fd != -1)
					write(logrlpre_fd, right_buf, right_n);
				if (stlrnp_bool)
					strip_np(right_buf, &right_n);
				if (stlrnpx_bool)
					strip_npxeol(right_buf, &right_n);
				if (loglrpost_fd != -1)
					write(logrlpost_fd, right_buf, right_n);
				write(right_sock,right_buf,right_n);
				wAddnstr(OUT_R,right_buf,right_n);
			}
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
	for (col = wcpos[i]; col < ww[i]; col++)
		mvwaddch(w[i],row,col,' ');
	/* move cursor back to where it started */
	wmove(w[i],wrpos[i],wcpos[i]);
}

/* Prints string to window w[i] */
void wAddstr(int i, char *s){
	wAddnstr(i, s, strlen(s));
}

/* Prints from 0 to n of buffer to window w[i]*/ 
void wAddnstr(int i, char *s,int n){
	int j,y,x;
	getyx(w[i],y,x);      // find out where we are in the window
  	y=y?y:!y;
  	x=x?x:!x;  
  	wrpos[i]=y;
  	wcpos[i]=x;
	wClrtoeol(i);
  	for (j=0;j<n;j++){
		/* check if in writable aread */
      		if (++wcpos[i]==ww[i] -1 || s[j] == '\n') {
	  		wcpos[i]=1;
	  		if (++wrpos[i]==wh[i] -1){
				wrpos[i]=1;
				wClrtoeol(i);
			}
		}
		/* if not a special character then write to screen */
      		if (s[j] != '\n'){
			if (s[j] >= 32 && s[j] <= 126)
				mvwaddch(w[i],wrpos[i],wcpos[i],(chtype) s[j]);
			else {
				char hex_buf[5];
				sprintf(hex_buf,"0x%x",s[j]);
				wAddnstr(i,hex_buf,5);
			}
    		}
	}
  	wrefresh(w[i]);
}


/* fills the buffer from curses getchar starting at spot n + 1 */
void fillbuf(char *buf, char char_in,int *n){
	if ( char_in == BACKSPACE){
		if (*n > 0){
			--(*n);
			mvwaddch(w[IO],wrpos[IO],wcpos[IO],' ');
			wmove(w[IO],wrpos[IO],wcpos[IO]);
			wcpos[IO]--;
		}}
	else if (char_in == ENTER){ // An enter
		buf[(*n)++] = '\n';
		if (++wrpos[IO] == wh[IO] - 1)
			wrpos[IO] = 1;

		wcpos[IO] = 1;
		wmove(w[IO],wrpos[IO],wcpos[IO]);}
	else if ((char_in >= 32) && (char_in != 127)){
		if (++wcpos[IO] == ww[IO]){
			wcpos[IO]=1;
			if (++wrpos[IO]==wh[IO] -1)
				wrpos[IO]=1;
		}
		buf[(*n)++] = char_in;
		mvwaddch(w[IO],wrpos[IO],wcpos[IO],char_in);
	}
}

/******************************************************************************************/
/* Functions for commands */
/******************************************************************************************/
/* starts an external command and fills pipein and pipeout with the in and out pipe file descriptors */
/* command[0] must contain the name of command and the remaining cells contain the arguments. */
/* arg_count is number of arguments and must be atleast one less then size of array */
void pOpen(char **command,int arg_count, int *pipein, int *pipeout){
	int fds1[2];
	int fds2[2];
	pid_t pid;
	wAddstr(IO, command[0]);
	char buf[15];
	sprintf(buf, "arg_count is %d\n", arg_count);
	wAddnstr(IO, buf,14);
	/* Create two pipes.
	File descriptors for the two ends of the pipe are placed in fds.
	The read end of the pipe is at [0] the write end of the pipe is at [1]
	*/
	if (pipe(fds1)<0) sys_err("pipe");
	if (pipe(fds2)<0) sys_err("pipe");
	/* Fork a child process.
	The child process inherits all the open file handles
	*/
	if ((pid=fork()) < 0 ) sys_err("fork");
	if (pid == (pid_t) 0) {
		/* This is the child process.
		Close child's copy of the write end of the file descriptor on the
		first pipe
		*/
		close (fds1[1]);
		/* Connect the read end of the first pipe to child's standard input. */
		dup2 (fds1[0], STDIN_FILENO);
		/* close child's copy of the read end of the second pipe */
		close (fds2[0]);
		/* connect the write end of the second pipe to child's standard out */
		dup2 (fds2[1], STDOUT_FILENO);
		/* Replace the child process with the the program we want to run */
		command[arg_count + 1] = (char *) NULL;
		execvp (command[0], command);
	} else {
		close (fds1[0]);
		close (fds2[1]);
		/* make aliases */
		*pipein = fds1[1];
		*pipeout = fds2[0];
	}
}
/* gets char from file descriptor */
char getChar(int fd){
	char tempbuf[1];
	if (read(fd,tempbuf,1) == 0)
		return EOF;
	else{
		//wAddnstr(IO,tempbuf,1);
		return tempbuf[0];
	}
}

/* strips non-pritable chars left to right */
void strip_np(char *buf,int *n){
	int j = 0;	
	int i;
	for (i = 0; i < *n; i++){
		if (buf[i] >= 32 && buf[i] < 127)
			buf[j++] = buf[i];
	}
	*n = j;
}


/* strips non_pritable chars left to right except LF and CR */
void strip_npxeol(char *buf, int *n){
	int j = 0;
	int i;
	for (i = 0; i < *n; i++){
		if (buf[i] == 10 || buf[i] ==13 ||(buf[i] >= 32 && buf[i] < 127))
			buf[j++] = buf[i];
	}
	*n = j;
}

/* set up log file from command prompt.*/
void logset(int *fd, char *file_name[],int arg_count){
	if (arg_count == 1){ 
		if (*fd = open(file_name[arg_count],O_WRONLY))
			wAddstr(IO,"File opened successfully.\n");		
		else
			wAddstr(IO,"Error opening file\n");}
	else
		wAddstr(IO,"Must specify file name to log to.\n");
}
/******************************************************************************************/
// Functions for networking.
/******************************************************************************************/

/* wrapper for close the removes the socket from inputs and sets sock to -1 */
void Close(int *sock){
	closesocket(*sock);
	FD_CLR(*sock,&inputs);
	*sock = -1;
}

/* prints a info about socket in the form sourceIP:sourcePort:destinationIP:destinationPort*/ 
void pair_info(passive_sock, sock, port){
	char return_str[46];
	memset(return_str,0,sizeof(return_str));
	/* Side is currently passive */
	if (passive_sock != -1){ 
		/*if side has accepted a connection */ 
		if (sock != -1)
			strcat(return_str,peer_info(sock));
		else
			strcat(return_str,"-:-");	
		strcat(return_str,":");
		strcat(return_str,my_info(passive_sock,port));
		strcat(return_str, "\n");}
	/*Side is active connection */
	else if (sock != -1){
		strcat(return_str,my_info(sock,port));
		strcat(return_str,":");
		strcat(return_str,peer_info(sock));
		strcat(return_str,"\n");}
	else
		strcat(return_str,"-:-:-:-\n");
	wAddstr(IO,return_str);
}

/* Returns a str in the form peer_IP:peer_port */
char *peer_info(int sock){
	struct sockaddr_in peeraddr;
	socklen_t peeraddr_len = sizeof(peeraddr);
	char straddr[INET_ADDRSTRLEN];
	char peerport_str[6];
	static char return_str[23];
	memset(return_str,0,sizeof(return_str));	
	if (sock != -1){
		getpeername(sock,(struct sockaddr*)&peeraddr,&peeraddr_len);
		inet_ntop(AF_INET, &peeraddr.sin_addr,straddr, sizeof(straddr));	
		sprintf(peerport_str,"%d",ntohs(peeraddr.sin_port));
		strcat(return_str,straddr);
		strcat(return_str,":");
		strcat(return_str,peerport_str);
		return (char*)&return_str;}
	else 
		return "-:-";
}		

/* Returns a str in the form my_IP:my_port */
char *my_info(int sock,int port){
	struct sockaddr_in myaddr;
	socklen_t my_addr_len = sizeof(myaddr);
	char straddr[INET_ADDRSTRLEN];
	char myport_str[6];
	static char return_str[23];
	memset(return_str,0,sizeof(return_str));
	char localhost[32];
	if (sock != -1){
		// get local address	
		gethostname(localhost,sizeof(localhost));
		struct hostent *h_hostent = gethostbyname(localhost);
		inet_ntop(AF_INET,h_hostent->h_addr_list[0],straddr,sizeof(straddr));
		strcat(return_str,straddr);
		// get local port used
		if (port > 0)	
			sprintf(myport_str,"%d",port);
               	else
			sprintf(myport_str,"-"); 
                strcat(return_str,":");
                strcat(return_str,myport_str);
		return (char*)&return_str;}
	else
		return "-:-";
}

/* Wrapper for accept that checks the accepted connection is from the valid ip and port.
*  Then adds it to the set of file descriptors and returns the socket */
int Accept(int sock_in,char* acct_addr,int acct_port){
	struct hostent *addr_hostent = NULL; 	/* stores IP address associated with lacct_addr if laddr is set */
	struct sockaddr_in cad; 		/* structure to hold client's address */
	int alen; 				/* length of address */
	int return_sock; 			/* socket descripter to be returned */
		
	alen = sizeof(cad);
	if ((return_sock = accept(sock_in, (struct sockaddr *)&cad, &alen)) < 0) {
		wAddstr(IO,"Accept failed on side. \n");
	} 

	/* if lacct_addr is set to non wildcard value */
	/* Convert lacct_addr to equivalant IP address and save to compare to the address
	   of the incoming client */
	char s[INET_ADDRSTRLEN];	
	if (acct_addr != NULL && strcmp(acct_addr,"*") != 0){
		addr_hostent = gethostbyname(acct_addr);
		inet_ntop(AF_INET,addr_hostent->h_addr_list[0],s,sizeof(s));
		if ( ((char *)addr_hostent) == NULL )
			wprintw(w[IO],"invalid host: %s. Defaulting to allowing any address.\n", acct_addr);
	}
	
	/* if -lacct_addr was set then check if connecting IP matches. If not skip request */
	if (addr_hostent != NULL) {
		char straddr[INET_ADDRSTRLEN];
		if (strcmp(s, inet_ntop(AF_INET, &cad.sin_addr,straddr, sizeof(straddr))) != 0){
			wAddstr(IO,"Piggy rejected a connection.\n");
			closesocket(return_sock);
			return -1;	
		}
	}
	/* if acct_port was set then check accepted socket is from that port */
	if (acct_port != -1){
		if (acct_port != ntohs(cad.sin_port)){
			wAddstr(IO,"Piggy rejected a connection.\n");
			closesocket(return_sock);
			return -1;	
		}	
	}
	
	/* add left_sock to inputs and return*/
	if (return_sock > 0){
		FD_SET(return_sock,&inputs);
		if (return_sock > max_fd)
			max_fd = return_sock;
		return return_sock;}
	else
		return -1;
}

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

	/* Bind address and port in left_sad to left_passive_sock. */
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
