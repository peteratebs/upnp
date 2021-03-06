 /*
 | RTPTERM.C - Runtime Platform Network Services
 |
 |   PORTED TO THE LINUX PLATFORM
 |
 | EBS - RT-Platform 
 |
 |  $Author: vmalaiya $
 |  $Date: 2006/07/17 15:29:01 $
 |  $Name:  $
 |  $Revision: 1.3 $
 |
 | Copyright EBS Inc. , 2006
 | All rights reserved.
 | This code may not be redistributed in source or linkable object form
 | without the consent of its author.
 |
 | Module description:
 |  [tbd]
*/

/************************************************************************
* Headers
************************************************************************/
#include "rtp.h"
#include "rtpterm.h"
#include "rtpstr.h"

#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

/************************************************************************
* Defines
************************************************************************/
#define TERMINAL_UP_ARROW       1
#define TERMINAL_DOWN_ARROW     2
#define TERMINAL_RIGHT_ARROW    3
#define TERMINAL_LEFT_ARROW     4
#define TERMINAL_ESCAPE         27

#define TERMINAL_THREAD_SLEEP   sleep

/************************************************************************
* Types
************************************************************************/

/************************************************************************
* Data
************************************************************************/

/************************************************************************
* Macros
************************************************************************/

/************************************************************************
* Function Prototypes
************************************************************************/

/************************************************************************
* Function Bodies
************************************************************************/

/*----------------------------------------------------------------------*
                            rtp_term_kbhit
 *----------------------------------------------------------------------*/
int rtp_term_kbhit (void)
{
    fd_set fds;
    struct timeval tv;

    /* Turn off Linux stream buffering */
    setbuf (stdin, NULL);

    /* set up fd */
    FD_ZERO (&fds);
    FD_SET (STDIN_FILENO, &fds);

    tv.tv_sec = tv.tv_usec = 0;

    if (select (STDIN_FILENO + 1, &fds, NULL, NULL, &tv) == 1)
        return (1);
    else
        return (0);
}


/*----------------------------------------------------------------------*
                            rtp_term_getch
 *----------------------------------------------------------------------*/
int rtp_term_getch (void)
{
int ch = 0;

    /* Turn off Linux stream buffering */
    setbuf (stdin, NULL);

    ch = getchar();
    return (ch);
}


/*----------------------------------------------------------------------*
                             rtp_term_putc
 *----------------------------------------------------------------------*/
void rtp_term_putc (char ch)
{
    /* Turn off Linux stream buffering */
    setbuf (stdout, NULL);

	putchar(ch);
}



/************************************************************************/
/*      THERE IS NO NEED TO CHANGE ANYTHING BELOW THIS COMMENT          */
/************************************************************************/




/*----------------------------------------------------------------------*
                             rtp_term_puts
 *----------------------------------------------------------------------*/
void rtp_term_puts (const char * string)
{
	rtp_term_cputs(string);
	rtp_term_putc('\n');
}


/*----------------------------------------------------------------------*
                             rtp_term_cputs
 *----------------------------------------------------------------------*/
int rtp_term_cputs (const char * string)
{
    while (*string)
	{
		rtp_term_putc(*string++);
	}
	return (0);
}


/*----------------------------------------------------------------------*
                             rtp_term_gets
 *----------------------------------------------------------------------*/
int rtp_term_gets (char * string)
{
    *string = 0;
    return (rtp_term_promptstring(string,0));
}


/*----------------------------------------------------------------------*
                          rtp_term_promptstring
 *----------------------------------------------------------------------*/
int rtp_term_promptstring (char * string, unsigned int handle_arrows)
{
/* ----------------------------------- */
/*  Endptr always points to            */
/*  null-terminator.                   */
/* ----------------------------------- */
char * endptr = &string[rtp_strlen(string)];
int ch;
char clbuff[80];

	rtp_memset((unsigned char *)clbuff, ' ', 79);
	clbuff[0] = '\r';
	clbuff[78] = '\r';
	clbuff[79] = 0;

#define CLEAR_LINE() rtp_term_cputs(clbuff)

    /* ----------------------------------- */
	/*  Print out the default answer.      */
	/* ----------------------------------- */
	rtp_term_cputs(string);

    while (!rtp_term_kbhit( ))
    {
        /* ----------------------------------- */
        /*  Free resources to time critical    */
        /*  events.                            */
        /* ----------------------------------- */
        TERMINAL_THREAD_SLEEP(2);
    }
    
    ch = rtp_term_getch( );
	while (ch != -1)
	{
		switch(ch)
		{
		    /* ----------------------------------- */
			/*  Return.                            */
			/* ----------------------------------- */
		    case '\n':
		    case '\r':
			    rtp_term_putc('\n');
			    return (0);

            /* ----------------------------------- */
			/*  Backspace.                         */
			/* ----------------------------------- */
		    case '\b':
			    if(endptr > string)
			    {
				    rtp_term_cputs("\b \b");
				    *(--endptr) = 0;
			    }               /* ----------------------------------- */
			    goto getnext;   /*  Get next character.                */
			                    /* ----------------------------------- */
			                
		    case TERMINAL_UP_ARROW:
			    if(handle_arrows)
			    {
			        /* ----------------------------------- */
				    /*  Erase the current line.            */
				    /* ----------------------------------- */
				    CLEAR_LINE();                       /* ----------------------------------- */
				    return (rtp_term_up_arrow ( ));     /*  TERMINAL_UP_ARROW                  */
			    }                                       /* ----------------------------------- */
			    break;

		    case TERMINAL_DOWN_ARROW:
			    if(handle_arrows)
			    {
			        /* ----------------------------------- */
				    /*  Erase the current line.            */
				    /* ----------------------------------- */
				    CLEAR_LINE();                       /* ----------------------------------- */
				    return (rtp_term_down_arrow ( ));   /*  TERMINAL_DOWN_ARROW                */
			    }                                       /* ----------------------------------- */
			    break;

		    case TERMINAL_ESCAPE:
			    if(handle_arrows)
			    {
				    /* ----------------------------------- */
				    /*  Erase the current line.            */
				    /* ----------------------------------- */
				    CLEAR_LINE();                       /* ----------------------------------- */
				    return (rtp_term_escape_key ( ));   /*  TERMINAL_ESCAPE                    */
			    }                                       /* ----------------------------------- */
			    break;
		}

        /* ----------------------------------- */
		/*  Display the editing.               */
		/* ----------------------------------- */
		rtp_term_putc((char)ch);
		*endptr++ = (char)ch;
		*endptr = 0;

getnext:
        while (!rtp_term_kbhit( ))
        {
            /* ----------------------------------- */
            /*  Free resources to time critical    */
            /*  events.                            */
            /* ----------------------------------- */
            TERMINAL_THREAD_SLEEP(2);
        }
        
        ch = rtp_term_getch( );
	}
	return (-1);
}


/*----------------------------------------------------------------------*
                           rtp_term_up_arrow
 *----------------------------------------------------------------------*/
int rtp_term_up_arrow (void)
{
    return ((int) TERMINAL_UP_ARROW);
}


/*----------------------------------------------------------------------*
                          rtp_term_down_arrow
 *----------------------------------------------------------------------*/
int rtp_term_down_arrow (void)
{
    return ((int) TERMINAL_DOWN_ARROW);
}


/*----------------------------------------------------------------------*
                          rtp_term_left_arrow
 *----------------------------------------------------------------------*/
int rtp_term_left_arrow (void)
{
    return ((int) TERMINAL_LEFT_ARROW);
}


/*----------------------------------------------------------------------*
                          rtp_term_right_arrow
 *----------------------------------------------------------------------*/
int rtp_term_right_arrow (void)
{
    return ((int) TERMINAL_RIGHT_ARROW);
}


/*----------------------------------------------------------------------*
                          rtp_term_escape_key
 *----------------------------------------------------------------------*/
int rtp_term_escape_key (void)
{
    return ((int) TERMINAL_ESCAPE);
}



/* ----------------------------------- */
/*             END OF FILE             */
/* ----------------------------------- */
