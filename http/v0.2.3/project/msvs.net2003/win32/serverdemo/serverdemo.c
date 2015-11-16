//
// SERVERDEMO.C - Web Server Demo
//
// EBS -
//
// Copyright EBS Inc. , 2009
// All rights reserved
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//
/*---------------------------------------------------------------------------*/
//#include "rtpnet.h"
//#include "rtpthrd.h"
//#include "rtpmemdb.h"
//#include "rtpterm.h"
#include "rtpprint.h"
#include "rtpgui.h"

int http_server_demo(void);
int http_client_demo(void);

/*---------------------------------------------------------------------------*/
#define CLIENT 1
#define SERVER 2

/*---------------------------------------------------------------------------*/
int main (void)
{
	int choice = 0;
	void *pmenu;

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_MENU, "EBS HTTP Demo", "Select Option", "Option >");
	if (!pmenu)
		return(-1);;

	rtp_gui_list_add_int_item(pmenu, "CLIENT DEMO", CLIENT, 0, 0, 0);
	rtp_gui_list_add_int_item(pmenu, "SERVER DEMO", SERVER, 0, 0, 0);
	
	choice = rtp_gui_list_execute_int(pmenu);
	rtp_gui_list_destroy(pmenu);

	switch(choice){
		case 1:
			{
                rtp_printf("Calling client demo\n");
                http_client_demo();
                rtp_printf("Returned from client demo\n");
			}
		case 2:
			{
				rtp_printf("Calling server demo\n");
                http_server_demo();
                rtp_printf("Returned from server demo\n");
			}
	}
	return(0);
}
