/*
|  example_server.c
|
|  Copyright EBS Inc. , 2009. All rights reserved.

This source file contains an example of a web server application written using the managed HTTP library.

This example demontrates the following:

. Configure initialize and call the web server.
. Serve locally generated and stored content when certain urls are requested.
. Serve file based content if a request is made for a url with no url specific handler.
. Control the server itself through URL specific handlers.

This example also provides a framework that other, more complex example utilize. By structuring
the demo in this way this file may be used as a template that provides all of the core functionality
needed for a basic dynamic server. The other server demos provide more elaborate demos and can be
eliminated from your project without confusion.
*/

/*****************************************************************************/
/* Header files
 *****************************************************************************/

#include "rtpthrd.h"
#include "rtpnet.h"
#include "rtpterm.h"
#include "rtpprint.h"
#include "rtpstr.h"
#include "httpsrv.h"

/*****************************************************************************/
/* Macros
 *****************************************************************************/

/* Configuration parameters */

/* These values may be changed to customize the server. */
#define DEMO_SERVER_NAME "EBS WEB SERVER" /* Server name that we use to format the Server property in a standard html reponse header */
#define DEMO_WWW_ROOT "/webcdemo"        /* Path used as the root for all content loads from disk. */
#define DEMO_WWW_FILE "index.html"        /* The default file name to use if a user accesses the server providing no file name. Index.htm is recommended. */

static unsigned char
DEFAULT_DEMO_IPADDRESS[] = {0,0,0,0};   /* IP address of the interface to serve content to. Set to {0,0,0,0} to
                                           use the default interface. If you want to serve to an alternate interface
                                           set this to the IP addres of the alternate interface (for example: {192,168,1,1}.
                                           Note: Linux and windows do not but some emebedded network stack
                                           implementations may require this field to be initialized.  */

#define DEMO_MAX_CONNECTIONS 8		   /* Configure the maximum number of simultaneously connected/queued for acceptance */

#define DEMO_IPVERSION 4               /* Do not change. The API supports V6 but the demo is wired for V4 right now */
#define DEMO_MAX_HELPERS 1             /* Do not change. Number of helper threads to spawn. Must be 1. */
#define DEMO_INDEX_ID 0				   /* Do not change. Indeces we use to Identify some URL */
#define DEMO_DISMISS_ID 1			   /* Do not change. Urls can also be identidied by string, that is done in other modules */


/*****************************************************************************/
/* Function Prototypes
/*****************************************************************************/

/* External functions that initialize advance examples and retrieve content from advanced examples */
int http_demo_init_vcontent(HTTPServerContext  *httpServer);
extern char **http_demo_retrieve_vcontent(HTTPServerContext  *httpServer,HTTP_INT32 contentid);


/* Utility functions used by this example */
static void http_demo_write_vcontent_bytes(HTTPSession *session, char **pContent);
static HTTP_INT32 http_demo_count_vcontent_bytes(char **pContent);
HTTP_CHAR *http_demo_alloc_string(HTTP_CHAR *string);
static void http_demo_release_vcontent(HTTP_INT32 virtUrlId, char **pContent);
static int http_server_demo_restart(void);


/*****************************************************************************/
/* Data
 *****************************************************************************/

 /* A context structure for keeping our application data */
typedef struct s_HTTPExampleServerCtx
{
	HTTP_UINT8         rootDirectory[256];
	HTTP_UINT8         defaultFile[256];
  	HTTP_BOOL          disableVirtualIndex;
  	HTTP_BOOL          chunkedEncoding;
	HTTP_INT16         numHelperThreads;
	HTTP_INT16         numConnections;
	HTTP_UINT8         defaultIpAddr[4];
	HTTPServerConnection* connectCtxArray;
	HTTPServerContext  httpServer;
  	HTTP_BOOL          ModuleRequestingReload;
} HTTPExampleServerCtx;

HTTPExampleServerCtx ExampleServerCtx;

/* Default content displayed when index.htm, index.html, or a null index page is selected
   Modify this page to presnt something else when the page is accessed. */
static char *virtual_index_content[]  = {
"<html><body><hr><center><b><big>Welcome to the EBS WEB Server Demo</big></b></center><hr><br><br>",
"<center>Login Page</center><br>",
"Select an option <br>",
"<br>Use this selection to demonstrate form processing.<br>",
"<a href=\"demo_form.htm\">Demonstrate Simple Forms Processing.</a><br>",
"<br>Use this selection to change the path where content is served from.<br>",
"<a href=\"configure.htm\">Setup And Configuration Web Server.</a><br>",
"<br>Use this selection to revert to disk based html. ",
"<br> When you refresh, index.html et al will be serverd from the disk.<br>",
"<a href=\"dismiss.htm\">Revert to disk based html index.</a><br>",
"<hr><br><center>The Ajax and jQuery examples are implemented as simple stubs. They are provide to demonstrate ",
"how to add new handlers.</center><br>",
"<a href=\"ajax.htm\">Demonstrate Using Ajax an Interactive Client Server Application.</a><br>",
"<a href=\"jquery.htm\">Demonstrate Using Jquery To Add Client Server UI.</a><br>",
"<br><hr>",
"</body></html>",
0};

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/

/* Main entry point for the web server application.
	A callable web server.
*/

int http_server_demo(void)
{
	HTTP_INT16 ipType = DEMO_IPVERSION;
	int idle_count = 0;
	rtp_net_init();
	rtp_threads_init();

	/* Set initial default values */
	rtp_memset(&ExampleServerCtx, 0, sizeof(ExampleServerCtx));
	rtp_strcpy(ExampleServerCtx.rootDirectory, DEMO_WWW_ROOT);
	rtp_strcpy(ExampleServerCtx.defaultFile,DEMO_WWW_FILE);
	ExampleServerCtx.chunkedEncoding 		= 0;
	ExampleServerCtx.numHelperThreads 		= DEMO_MAX_HELPERS;
	ExampleServerCtx.numConnections		 	= DEMO_MAX_CONNECTIONS;
	/* If these are {0,0,0,0} use the default interface otherwise we use the configured address */
	ExampleServerCtx.defaultIpAddr[0] = DEFAULT_DEMO_IPADDRESS[0];
	ExampleServerCtx.defaultIpAddr[1] = DEFAULT_DEMO_IPADDRESS[1];
	ExampleServerCtx.defaultIpAddr[2] = DEFAULT_DEMO_IPADDRESS[2];
	ExampleServerCtx.defaultIpAddr[3] = DEFAULT_DEMO_IPADDRESS[3];

	rtp_printf("Using IP address %d.%d.%d.%d (all zeroes means use default interface) \n",
		ExampleServerCtx.defaultIpAddr[0],	ExampleServerCtx.defaultIpAddr[1],
		ExampleServerCtx.defaultIpAddr[2],	ExampleServerCtx.defaultIpAddr[3]);

	/* Initialize the server */
	if (http_server_demo_restart() != 0)
		return(-1);

	/* Now loop continuously process one request per loop. */
	for (;;)
	{
		if (HTTP_ServerProcessOneRequest (&ExampleServerCtx.httpServer, 1000*60) < 0)
		{
			/* Print an idle counter every minute the server is not accessed */
			idle_count += 1;
			if (idle_count == 1)
				rtp_printf("\n Idle %d minutes      ", idle_count);
			else
				rtp_printf("                                     \r Idle %d minutes", idle_count);
		}
		else
			idle_count = 0;

		if (ExampleServerCtx.ModuleRequestingReload)
		{
			ExampleServerCtx.ModuleRequestingReload = 0;
			HTTP_ServerDestroy(&ExampleServerCtx.httpServer, &ExampleServerCtx.connectCtxArray);
			rtp_free(ExampleServerCtx.connectCtxArray);
			ExampleServerCtx.connectCtxArray = 0;
			/* Initialize the server */
			if (http_server_demo_restart() != 0)
			{
				rtp_net_exit();
				return(-1);
			}
		}
	}

	rtp_net_exit();
	return (0);

}


int _Demo_UrlRequestHandler (void *userData,HTTPServerRequestContext *ctx,
                                     HTTPSession *session,
                                     HTTPRequest *request,RTP_NET_ADDR *clientAddr);
void _Demo_UrlRequestDestructor (void *userData);
static int _Demo_IndexFileProcessHeader(void *userData,	HTTPServerRequestContext *server, HTTPSession *session,HTTPHeaderType type, const HTTP_CHAR *name,	const HTTP_CHAR *value);

/* Start or restart the server. Called when the application is first run and when callback handlers
   request a restart of the server because parametrs have been changed */
static int http_server_demo_restart(void)
{
	HTTP_INT16 ipType = DEMO_IPVERSION;	 /* The API supports V6 but the demo is wired for V4 right now */
	HTTP_UINT8     *pDefaultIpAddr;

	/* The demo uses 0,0,0,0 to signify use default but the API uses a null pointer so
	   map the demo methodology to the API metodology */
	if (ExampleServerCtx.defaultIpAddr[0] == 0 &&
		ExampleServerCtx.defaultIpAddr[1] == 0 &&
		ExampleServerCtx.defaultIpAddr[2] == 0 &&
		ExampleServerCtx.defaultIpAddr[3] == 0)
		pDefaultIpAddr = 0;
	else
		pDefaultIpAddr = &ExampleServerCtx.defaultIpAddr[0];

	rtp_printf("Starting or restarting server \n");

	/* Allocate and clear one server context per possible simultaneous connection. see (DEMO_MAX_CONNECTIONS) */
	if (!ExampleServerCtx.connectCtxArray)
	{
		ExampleServerCtx.connectCtxArray = (HTTPServerConnection*) rtp_malloc(sizeof(HTTPServerConnection) * ExampleServerCtx.numConnections);
		if (!ExampleServerCtx.connectCtxArray)
			return -1;
	}
	rtp_memset(ExampleServerCtx.connectCtxArray,0, sizeof(HTTPServerConnection) * ExampleServerCtx.numConnections);

	/* Initialize the server */
	if (HTTP_ServerInit (
			&ExampleServerCtx.httpServer,
			DEMO_SERVER_NAME, // name
			ExampleServerCtx.rootDirectory,    // rootDir
			ExampleServerCtx.defaultFile,      // defaultFile
			1,                // httpMajorVersion
			1,                // httpMinorVersion
			pDefaultIpAddr,
			80,               // 80  ?? Use the www default port
			DEMO_IPVERSION,   // ipversion type(4 or 6)
			0,                // allowKeepAlive
			ExampleServerCtx.connectCtxArray,  // connectCtxArray
			ExampleServerCtx.numConnections,   // maxConnections
			ExampleServerCtx.numHelperThreads  // maxHelperThreads
		) < 0)
		return -1;

	/* Set up request handlers for requests for typical index urls request index.html, index.htm and none */
	/* This api assigns _Demo_UrlRequestHandler() as a callback handler ti url "\\index.html" */
	if (HTTP_ServerAddPath(&ExampleServerCtx.httpServer,
					"\\index.html", 1,(HTTPRequestHandler)_Demo_UrlRequestHandler,
					(HTTPServerPathDestructor)_Demo_UrlRequestDestructor, (void *) 0) < 0)
		return (-1);
	/* Assign_Demo_UrlRequestHandler() as a callback handler ti url "\\index.htm" */
	if (HTTP_ServerAddPath(&ExampleServerCtx.httpServer,
					"\\index.htm", 1,(HTTPRequestHandler)_Demo_UrlRequestHandler,
					(HTTPServerPathDestructor)_Demo_UrlRequestDestructor, (void *) 0) < 0)
		return (-1);
	/* Add "\\" */
	if (HTTP_ServerAddPath(&ExampleServerCtx.httpServer,
					"\\", 1,(HTTPRequestHandler)_Demo_UrlRequestHandler,
					(HTTPServerPathDestructor)_Demo_UrlRequestDestructor, (void *) 0) < 0)
		return (-1);
	/* Add a callback for the dismiss url. This is a one liner so piggyback it on our default handler */
	if (HTTP_ServerAddPath(&ExampleServerCtx.httpServer,
					"\\dismiss.htm", 1,(HTTPRequestHandler)_Demo_UrlRequestHandler,
					(HTTPServerPathDestructor)_Demo_UrlRequestDestructor, (void *) DEMO_DISMISS_ID) < 0)
		return (-1);

	/* Call external function to initialize advanced examples */
	if (http_demo_init_vcontent(&ExampleServerCtx.httpServer) != 0)
		return(-1);
	return (0);
}



/*---------------------------------------------------------------------------*/
void _Demo_UrlRequestDestructor (void *userData)
{
}

/* Compiled in web pages

  virtual_index_content[] - Contains html content that is displayed when the browser requests an index file using the
							file names index.htm, index.html or a null page.

							If any of these files is accessed the call callback routine _Demo_UrlRequestHandler()
							(see below. Is called).

							Demo_UrlRequestHandler() is also installed as a handler for "dismiss.htm" a special url we
							use for disabling the virtual index.

  							Processing for the other urls in the virtual_index_content array are installed by other
  							http_demo_init_vcontent(). Povided in a sperate module in this directory.

							The additional examples are segregated in addiditional fils to uncouple the example
							content from the structure as muich as possible.
  */

static char *virtual_dismiss_content[]  = {
"<html><body><hr><center><b><big>The virtual index feature is now disabled.</big></b></center><hr><br><br>",
"<center> Future request for index.html, index.htm or a null page will retrieve content from the file system.</center>",
"</body></html>",
0};
int _Demo_UrlRequestHandler (void *userData,
                                     HTTPServerRequestContext *ctx,
                                     HTTPSession *session,
                                     HTTPRequest *request,
                                     RTP_NET_ADDR *clientAddr)
{
	HTTPServerContext *server = ctx->server;
	HTTP_INT32 virtUrlId;

	if (ExampleServerCtx.disableVirtualIndex)
		return(HTTP_REQUEST_STATUS_CONTINUE);
	/* We passed an integer constant to identify the URL, use that to retrieve the html source and to perform any special processing */
	virtUrlId = (HTTP_INT32) userData;

	/* If the virtual index is disabled tell the server to get index.html index.htm aind \\ fro the file store */
    if (virtUrlId == 0 && ExampleServerCtx.disableVirtualIndex)
		return(HTTP_REQUEST_STATUS_CONTINUE);
	/* If the dismiss link was clicked respond with a message but disable the virtual index feature on the next pass */
    if (virtUrlId == DEMO_DISMISS_ID)
	{
		rtp_printf("\n Disabling virtual index feature and reverting to disk index\n");
		ExampleServerCtx.disableVirtualIndex = 1;
	}

	switch (request->methodType)
	{
		case HTTP_METHOD_GET:
		{
			HTTPResponse response;
			HTTP_UINT8 tempBuffer[256];
			HTTP_INT32 tempBufferSize = 256;
			HTTP_INT32 vfilesize;
			char **virtual_content;

			if (request->target)
				rtp_printf("Ip [%d.%d.%d.%d] Requesting URL:%s \n",
					clientAddr->ipAddr[0],	clientAddr->ipAddr[1],
					clientAddr->ipAddr[2], clientAddr->ipAddr[3], request->target);

			/* Handle 0 is the default index page */
			if (virtUrlId == 0)
				virtual_content = &virtual_index_content[0];
			else if (virtUrlId == DEMO_DISMISS_ID)
				virtual_content = &virtual_dismiss_content[0];
			else /* Must be a plug in */
				virtual_content = http_demo_retrieve_vcontent(&ExampleServerCtx.httpServer,virtUrlId);

			HTTP_ServerReadHeaders(	ctx,session, _Demo_IndexFileProcessHeader, 0, tempBuffer, tempBufferSize);

			HTTP_ServerInitResponse(ctx, session, &response, 200, "OK");
			HTTP_ServerSetDefaultHeaders(ctx, &response);
			HTTP_SetResponseHeaderStr(&response, "Content-Type", "text/html");


			vfilesize = http_demo_count_vcontent_bytes(virtual_content);
			HTTP_SetResponseHeaderInt(&response, "Content-Length", vfilesize);

			HTTP_WriteResponse(session, &response);
			http_demo_write_vcontent_bytes(session, virtual_content);

			HTTP_WriteFlush(session);
			if (virtUrlId != 0)
				http_demo_release_vcontent(virtUrlId,virtual_content);

			HTTP_FreeResponse(&response);

			return (HTTP_REQUEST_STATUS_DONE);
		}

		case HTTP_METHOD_HEAD: /* tbd - implement HEAD method */
		default:
			return (HTTP_REQUEST_STATUS_CONTINUE);
	}
}

/*---------------------------------------------------------------------------*/
static int _Demo_IndexFileProcessHeader(void *userData,	HTTPServerRequestContext *server, HTTPSession *session,
		HTTPHeaderType type, const HTTP_CHAR *name,	const HTTP_CHAR *value)
{
	switch (type)
	{
		case HTTP_HEADER_IF_MODIFIED_SINCE:
			/* tbd */
			break;
	}

	return (0);
}



static HTTP_INT32 http_demo_count_vcontent_bytes(char **pContent)
{
	HTTP_INT32 length = 0;
	while (*pContent)
	{
		length += rtp_strlen(*pContent);
		pContent++;
	}
	return(length);
}

static void http_demo_write_vcontent_bytes(HTTPSession *session, char **pContent)
{
	HTTP_INT32 length = 0;
	while (*pContent)
	{
		length = rtp_strlen(*pContent);
		HTTP_Write(session, *pContent, length);
		pContent++;
	}
}

static void http_demo_release_vcontent(HTTP_INT32 virtUrlId, char **pContent)
{
	HTTP_INT32 length = 0;
	char **_pContent = pContent;
return;
	while (*_pContent)
	{
		rtp_free(*_pContent);
		_pContent++;
	}
	rtp_free(pContent);
}

HTTP_CHAR *http_demo_alloc_string(HTTP_CHAR *string)
{
HTTP_CHAR *r;
	r = rtp_malloc(rtp_strlen(string)+1);
	if (r)
		rtp_strcpy(r, string);
	return(r);
}
