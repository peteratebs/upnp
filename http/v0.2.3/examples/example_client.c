/*
|  example_client.c
|
|  Copyright EBS Inc. , 2009. All rights reserved.

This source file contains an example of a interactive HTTP application written using the managed HTTP library.

This example demontrates the following:

.
.
.
.
*/

/*7****************************************************************************/
/* Header files
 *****************************************************************************/

#include "rtpnet.h"
#include "rtpterm.h"
#include "rtpprint.h"
#include "rtpstr.h"
#include "httpmcli.h"
#include "rtpgui.h"
#include "rtpthrd.h"
#include "rtpfile.h"

/*****************************************************************************/
/* Macros
 *****************************************************************************/

/* Configuration parameters */


/*****************************************************************************/
/* Data
 *****************************************************************************/

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/

/* Main entry point for the web server application.
	A callable web server.
*/
static int prompt_for_command(void);
static int http_client_init(HTTPManagedClient* phttpClient);
static int http_client_get(HTTPManagedClient* phttpClient, int savetofile);
static int http_client_post(HTTPManagedClient* phttpClient);
static int http_client_put(HTTPManagedClient* phttpClient);

#define COMMAND_QUIT 		0
#define COMMAND_GET 		1
#define COMMAND_GETTO_FILE 	2
#define COMMAND_POST 		3
#define COMMAND_PUT  		4

int http_client_demo(void)
{
	int idle_count = 0,command;
	rtp_net_init();
	rtp_threads_init();

	while ((command = prompt_for_command()) != COMMAND_QUIT)
	{
	HTTPManagedClient httpClient;
		if (http_client_init(&httpClient) < 0)
			return(-1);
		if (command == COMMAND_GET)
			http_client_get(&httpClient,0);
		else if (command == COMMAND_GETTO_FILE)
			http_client_get(&httpClient,1);
		else if (command == COMMAND_POST)
			http_client_post(&httpClient);
		else if (command == COMMAND_PUT)
			http_client_put(&httpClient);
	}
	rtp_net_exit();
	return (0);

}

static int http_client_init(HTTPManagedClient* phttpClient)
{
	rtp_memset(phttpClient, 0, sizeof(*phttpClient));

	if (HTTP_ManagedClientInit (
			phttpClient,
			"EBS MANAGED CLIENT" /* User-Agent */,
			0                      /* Accept (mime types) */,
			0                      /* KeepAlive (bool) */,
			0                      /* Cookie context */,
			0                      /* Authentication context */,
			0                      /* SSL context */,
			0                      /* SSL enabled? (bool) */,
			8192                   /* write buffer size */,
			0                      /* max connections (0 == no limit) */,
			0                      /* max connections per host (0 == no limit) */
			) >= 0)
	{
			return (0);
	}
	else
	{
		rtp_printf("HTTP_ManagedClientInit Failed\n");
		return (-1);
	}
}

static int prompt_for_command(void)
{
    int command =0;
	void *pmenu;

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_MENU, "EBS Media Controller Demo", "Select Option", "Option >");
	if (!pmenu)
		return(-1);;

	rtp_gui_list_add_int_item(pmenu, "Get URL and count number of bytes", COMMAND_GET, 0, 0, 0);
	rtp_gui_list_add_int_item(pmenu, "Get URL and save to file", COMMAND_GETTO_FILE, 0, 0, 0);
	rtp_gui_list_add_int_item(pmenu, "Post data to a URL", COMMAND_POST, 0, 0, 0);
	rtp_gui_list_add_int_item(pmenu, "Put a local file to a url", COMMAND_PUT, 0, 0, 0);
	rtp_gui_list_add_int_item(pmenu, "Quit", COMMAND_QUIT, 0, 0, 0);

	command = rtp_gui_list_execute_int(pmenu);
	rtp_gui_list_destroy(pmenu);

	return (command);
}

HTTP_UINT8 read_buffer[4096];
HTTP_INT32 read_buffer_size = 4096;

static int http_client_get(HTTPManagedClient* phttpClient, int savetofile)
{
HTTPManagedClientSession* session = 0;
char urlpath[255];
char urlfile[255];
char localfile[255];
RTP_HANDLE  fd;

HTTP_INT32 result = -1;
HTTP_INT32 totalresult = 0;
HTTPResponseInfo info;

	rtp_gui_prompt_text(" ","IP Address (eg: 192.161.2.1) or domain name of host (eg: www.google.com)\n    :", &urlpath[0]);
	rtp_gui_prompt_text(" ","File name: (eg: /index.html or / or /mydownloads/mypdf.pdf\n    :", &urlfile[0]);

	if (savetofile)
	{
		rtp_gui_prompt_text(" ","Local file name: \n    :", &localfile[0]);
		if (rtp_file_open (&fd, (const char *) &localfile[0], (RTP_FILE_O_CREAT|RTP_FILE_O_RDWR|RTP_FILE_O_TRUNC), RTP_FILE_S_IWRITE) < 0)
		{
	 		rtp_printf("Failure opening %s\n", localfile);
	 		return(-1);
		}
	}


	/* A HTTPManagedClientSession is the abstraction for a SINGLE HTTP request/response pair.
	Thus a new session must be opened for each HTTP operation
	(although this may not cause a new connection to be established, if keep alive is used),
	and closed when the operation has completed (though, again, this may not actually close a
	physical network connection) */

	if (HTTP_ManagedClientStartTransaction (
	 phttpClient,
	 &urlpath[0],
	 80,
      4,
	 HTTP_SESSION_TYPE_TCP,
	 1, /* blocking? */ &session ) < 0)
	{
	 	rtp_printf("Failed: retrieving data from from %s/%s\n", urlpath, urlfile);
	 	return(-1);
	}


	 /* Once the session is open, one command may be issued; in this case a GET
	   (by calling HTTP_ManagedClientGet) */
	 HTTP_ManagedClientGet(session, urlfile, 0 /* if-modified-since */);

	 /* This may be called at any time after the command is issued to get information
	    about the server response; this info includes the status code given, date when
	    the request was processed, file mime type information (if a file is transferred
	    as the result of a command), authentication information, etc. */

	 HTTP_ManagedClientReadResponseInfo(session, &info);

	 do {
	 	/* Read data from the session */
	 	result = HTTP_ManagedClientRead(session, read_buffer, read_buffer_size);
		if (result > 0)
		{
			totalresult += result;
			if (savetofile)
			{
				if (rtp_file_write(fd,read_buffer, (long)read_buffer_size) < 0)
				{
					rtp_printf("Failed writing to file\n");
					return(-1);
				}
			}
		}
	 } while (result > 0);

	/* Now we are done; close the session (see note above about sessions) */
	 HTTP_ManagedClientCloseSession(session);

	 /* When all HTTP client activity is completed, the managed client context may safely be destroyed */
	 HTTP_ManagedClientDestroy(phttpClient);


	if (savetofile)
	{
		rtp_file_close(fd);
	}
	 if (result == 0)
	 {
	 	rtp_printf("Success: (%d) bytes retrieved from %s/%s\n", totalresult, urlpath, urlfile);
	 	return(0);
	 }
	 else
	 {
	 	rtp_printf("Failure: (%d) bytes retrieved from %s/%s\n", totalresult, urlpath, urlfile);
	 	return(-1);
	 }
}



char *posttoebsupnpinfo = "VTI-GROUP=0&T1=etphoninghome&T2=mycompany&C4=ON&S1=myaddress&C5=ON&C6=ON&T3=mycity&C7=ON&T4=mystate&C8=ON&T5=myzip&C9=ON&T6=myphone&T7=myfax&T8=my%40emailaddr.xxx&C2=ON&S2=mycomments&B1=Submit";
char *posttoebsupnurl = "/requestinfo.php?product=RTUPNP";

static int http_client_post(HTTPManagedClient* phttpClient)
{
HTTPManagedClientSession* session = 0;
char urlpath[255];
char urlfile[255];
char datatopost[255];
char contenttype[255];
char useebssite[32];
HTTP_INT32 result = -1;
HTTP_INT32 totalresult = 0;
HTTPResponseInfo info;
HTTP_INT32 contentLength;

	rtp_strcpy(contenttype, "application/x-www-form-urlencoded"); /* content-type */
 	rtp_printf("Type Y or y to post a fixed message to www.ebsembeddedsoftware.com\n");
 	rtp_printf("Type N to be prompted for URL form name and data \n");
	rtp_gui_prompt_text(" ","Select Y or N :", &useebssite[0]);
	if (useebssite[0] == 'Y' || useebssite[0] == 'y')
	{
		rtp_strcpy(&urlpath[0], "www.ebsembeddedsoftware.com");
		rtp_strcpy(&datatopost[0], posttoebsupnpinfo);
		rtp_strcpy(&urlfile[0], posttoebsupnurl);
	}
	else
	{
		rtp_gui_prompt_text(" ","IP Address (eg: 192.161.2.1) or domain name of host (eg: www.google.com)\n   :", &urlpath[0]);
		rtp_gui_prompt_text(" ","File (form name) name: (eg: /requestinfo.php?product=RTUPNP\n    :", &urlfile[0]);
		rtp_gui_prompt_text(" ","Data to post: (eg: company=acme&product=wigits) \n    :", &datatopost[0]);
	}

	contentLength = rtp_strlen(&datatopost[0]);

	/* A HTTPManagedClientSession is the abstraction for a SINGLE HTTP request/response pair.
	Thus a new session must be opened for each HTTP operation
	(although this may not cause a new connection to be established, if keep alive is used),
	and closed when the operation has completed (though, again, this may not actually close a
	physical network connection) */

	if (HTTP_ManagedClientStartTransaction (
	 phttpClient,
	 &urlpath[0],
	 80,
      4,
	 HTTP_SESSION_TYPE_TCP,
	 1, /* blocking? */ &session ) < 0)
	{
	 	rtp_printf("Failed: connecting to %s\n", urlpath);
	 	return(-1);
	}


	/* Once the session is open, one command may be issued; in this case a Post */
 	HTTP_ManagedClientPost ( session,
 	urlfile, /* path */
 	contenttype,
 	contentLength /* content-length */ );

 	/* write the POST data */
	HTTP_ManagedClientWrite (session, (HTTP_UINT8*) &datatopost[0], contentLength);

/* this function must be called when all data has been written */
	HTTP_ManagedClientWriteDone (session);

/* This may be called at any time after the command is issued to get information about the
   server response; this info includes the status code given, date when the request was
   processed, file mime type information (if a file is transferred as the result of a
   command), authentication information, etc. */

   HTTP_ManagedClientReadResponseInfo(session, &info);

   do { /* Read data from the session */
   		result = HTTP_ManagedClientRead(session, read_buffer, read_buffer_size);
   } while (result > 0);


   /* Now we are done; close the session (see note above about sessions) */
   HTTP_ManagedClientCloseSession(session);

   /* When all HTTP client activity is completed, the managed client context may safely be destroyed */
   HTTP_ManagedClientDestroy(phttpClient);


   if (result == 0)
   {
   	rtp_printf("Success: posting to %s%s\n", totalresult, urlpath, urlfile);
	return(0);
	}
	else
	{
		rtp_printf("Failure: posting to %s%s\n", totalresult, urlpath, urlfile);
		return(-1);
	}
}


static int http_client_put(HTTPManagedClient* phttpClient)
{
HTTPManagedClientSession* session = 0;
char urlpath[255];
char urlfile[255];
char localfile[255];
char contenttype[255];
HTTP_INT32 result = -1;
HTTP_INT32 totalresult = 0;
HTTPResponseInfo info;
HTTP_INT32 contentLength;
RTP_HANDLE  fd;
HTTP_INT32 nread;

	rtp_strcpy(contenttype, "application/x-www-form-urlencoded"); /* content-type */

	rtp_gui_prompt_text(" ","IP Address (eg: 192.161.2.1) or domain name of host (eg: www.google.com)\n    :", &urlpath[0]);
	rtp_gui_prompt_text(" ","File name on server\n    :", &urlfile[0]);
	rtp_gui_prompt_text(" ","Local file name to put\n    :", &localfile[0]);
	rtp_gui_prompt_text(" ","Content type eg: text/html\n    :", &contenttype[0]);

	contentLength = 0;			/* Set content length to zero so we use chunked encoding */

	/* A HTTPManagedClientSession is the abstraction for a SINGLE HTTP request/response pair.
	Thus a new session must be opened for each HTTP operation
	(although this may not cause a new connection to be established, if keep alive is used),
	and closed when the operation has completed (though, again, this may not actually close a
	physical network connection) */

	if (HTTP_ManagedClientStartTransaction (
	 phttpClient,
	 &urlpath[0],
	 80,
      4,
	 HTTP_SESSION_TYPE_TCP,
	 1, /* blocking? */ &session ) < 0)
	{
	 	rtp_printf("Failed: connecting to %s\n", urlpath);
	 	return(-1);
	}


	/* Once the session is open, one command may be issued; in this case a Post */
 	HTTP_ManagedClientPut ( session,
 	urlfile, /* path */
 	contenttype,
 	contentLength /* content-length */ );

	if (rtp_file_open (&fd, (const char *) &localfile[0], RTP_FILE_O_RDONLY, 0) < 0)
	{
 		rtp_printf("Failure opening %s\n", localfile);
 		return(-1);
	}

 	/* write the POST data */
	do
	{

 		nread = rtp_file_read(fd,read_buffer, (long)read_buffer_size);
 		if (nread > 0)
			HTTP_ManagedClientWrite (session, (HTTP_UINT8*) &read_buffer[0], nread);
	} while (nread >= 0);

/* this function must be called when all data has been written */
	HTTP_ManagedClientWriteDone (session);

/* This may be called at any time after the command is issued to get information about the
   server response; this info includes the status code given, date when the request was
   processed, file mime type information (if a file is transferred as the result of a
   command), authentication information, etc. */

   HTTP_ManagedClientReadResponseInfo(session, &info);

   do { /* Read data from the session */
   		result = HTTP_ManagedClientRead(session, read_buffer, read_buffer_size);
   } while (result > 0);


   /* Now we are done; close the session (see note above about sessions) */
   HTTP_ManagedClientCloseSession(session);

   /* When all HTTP client activity is completed, the managed client context may safely be destroyed */
   HTTP_ManagedClientDestroy(phttpClient);

   rtp_file_close(fd);

   if (result == 0)
   {
   		rtp_printf("Success: putting file: %s to %s%s\n", localfile, urlpath, urlfile);
   		return(0);
	}
	else
	{
   		rtp_printf("Failed: putting file: %s to %s%s\n", localfile, urlpath, urlfile);
		return(-1);
	}
}
