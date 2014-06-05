#include <stdlib.h>
#include <curses.h>
#include <locale.h>
#include <string.h>
#define maxwin 6
static  WINDOW *w[maxwin];
static ww[maxwin];
static wh[maxwin];
static wrpos[maxwin];
static wcpos[maxwin];
static chtype ls,rs,ts,bs,tl,tr,bl,br;

void wAddstr(int i, char s[132])
{
  int j,l,y,x;
  getyx(w[i],y,x);      // find out where we are in the window
  y=y?y:!y;
  x=x?x:!x;  
  wrpos[i]=y;
  wcpos[i]=x;
  l=strlen(s);
  for (j=0;j<l;j++)
    {
      if (++wcpos[i]==ww[i]) 
	{
	  wcpos[i]=1;
	  if (++wrpos[i]==wh[i]) { wrpos[i]=1; }
	  
	}	
      mvwaddch(w[i],wrpos[i],wcpos[i],(chtype) s[j]);   
    }
  wrefresh(w[i]);
}
  
int main(int argc, char *argv[])
{
 
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
  setlocale(LC_ALL,"");       // this has to do with the character set to use
  initscr();         // must always call this (or newterm) to initialize the
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
  
  if (!(LINES==43) || !(COLS==132) ) 
    { 
      if (resizeterm(43,132)==ERR)
	{
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
  
  // create the 5 windows 
  a=18; b=66; c=0; d=0;
  w[1]=subwin(w[0],a,b,c,d);
  w[2]=subwin(w[0],a,b,c,b);
  w[3]=subwin(w[0],a,b,a,c);
  w[4]=subwin(w[0],a,b,a,b);
  w[5]=subwin(w[0],7,132,36,c);
  for (i=1;i<maxwin-1;i++) 
    { 
      ww[i]=b-1;
      wh[i]=a-1;
    }
  ww[5]=131;
  wh[5]=6;
  // draw a border on each window
  for (i=1;i<maxwin;i++)
    { wattron(w[i], A_STANDOUT);
      wborder(w[i],ls,rs,ts,bs,tl,tr,bl,br);
      wrefresh(w[i]);
      wattroff(w[i], A_STANDOUT);
      wrpos[i]=1;
      wcpos[i]=1;
    }  
  wAddstr(1,"This is the window where the data flowing from left to right \
  will be displayed. Notice that you should \"wrap around\" to the next line \
  before you use the very last character in the width of a window or you will \
  clobber the right border character. You should also not use the very first \
  position in a row or you will clobber the left border character. ");
  wAddstr(2,"Data leaving right side");
  wAddstr(3,"Data leaving the left side");
  wAddstr(4,"Data arriving from the right"); 
  // Place cursor at top corner of window 5
  wmove(w[5],1,1);  
  wprintw(w[5],"Hit enter to continue");
  wgetstr(w[5],response);            // Pause so we can see the screen 
  wmove(w[5],1,1);
  wclrtoeol(w[5]);
  wrefresh(w[5]);
  // let's write some stuff to the windows
  for (i=1;i<65535;i++) 
    { 
      for (j=1;j<5;j++) 
	wAddstr(j,"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwuyz"); 
	
    }
  halfdelay(20);
  i=0;
  nch=0x20;
  while  (!(nch==113))
    {
      if (  (nch=wgetch(w[5]))==ERR )
	{
	  i++;
	  mvwaddstr(w[5],2,1,"Nothing typed in last 2 seconds times");
 	  mvwprintw(w[5],2,40,"%d",i);
	  wrefresh(w[5]);
	}
      else
	{
	  wmove(w[5],2,1);
          wclrtoeol(w[5]);
	  mvwprintw(w[5],4, 1, "Character pressed is = %3d ",nch);
          if ((nch<32) || (nch==127)) 
	    {
	      wprintw(w[5],"Non-printable character");
	    }
	  else  
	    {
	      wprintw(w[5],"it can be printed as '%c'", nch);
	    }
	  i=0;
          wrefresh(w[5]);
	}
    }
  /* End screen updating */
  endwin();
}

