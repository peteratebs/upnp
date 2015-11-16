/*
|  example_server.c
|
|  EBS -
|
|   $Author: pvanoudenaren $
|   $Date: 2009/08/22 19:14:06 $
|   $Name:  $
|   $Revision: 1.2 $
|
|  Copyright EBS Inc. , 2005
|  All rights reserved.
|  This code may not be redistributed in source or linkable object form
|  without the consent of its author.
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
#define DEMO_SERVER_NAME "EBS WEB SERVER DEMO"
#define DEMO_WWW_ROOT "\\Users\\peter\\Downloads\\website\\ebsembeddedsoftware"
static unsigned char DEFAULT_DEMO_IPADDRESS[] = {192,168,1,9};
#define DEMO_IPVERSION 4
//#define DEMO_MAX_CONNECTIONS 8
//#define DEMO_MAX_HELPERS 2
#define DEMO_MAX_CONNECTIONS 8
#define DEMO_MAX_HELPERS 1

 /*****************************************************************************/
/* Types
 *****************************************************************************/

/*****************************************************************************/
/* Function Prototypes
/*****************************************************************************/
static int http_server_create_virtual_form(HTTPServerContext *phttpServer);
static int http_server_create_virtual_index(HTTPServerContext *phttpServer);
static HTTP_INT32 http_demo_count_vcontent_bytes(char **pContent);

static char **http_demo_generate_vcontent(HTTP_INT32 virtUrlId);
static void http_demo_release_vcontent(HTTP_INT32 virtUrlId, char **pContent);

static void http_demo_write_vcontent_bytes(HTTPSession *session, char **pContent);
static void http_demo_init_vcontent(void);
static char **http_demo_retrieve_vcontent(HTTP_INT32 contentid);


/*****************************************************************************/
/* Data
 *****************************************************************************/
typedef struct s_HTTPExampleServerCtx
{
	HTTP_UINT8         rootDirectory[256];
	HTTP_UINT8         defaultFile[256];
  	HTTP_BOOL          disableVirtualIndex;
  	HTTP_BOOL          chunkedEncoding;
	HTTP_INT16         numHelperThreads;
	HTTP_INT16         numConnections;
	HTTP_UINT8         defaultIpAddr[4];
	HTTP_CHAR          defaultIpAddrstr[256];
	HTTPServerConnection* connectCtxArray;
	HTTPServerContext  httpServer;
  	HTTP_BOOL          timeToReload;
} HTTPExampleServerCtx;

HTTPExampleServerCtx ExampleServerCtx;



/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/
/*---------------------------------------------------------------------------*/


/* target independent - Main entry point for the media contoller application. */
static int http_server_demo_restart(void);


int http_server_demo(void)
{
	HTTP_INT16 ipType = 4;
	rtp_net_init();
	rtp_threads_init();


	rtp_memset(&ExampleServerCtx, 0, sizeof(ExampleServerCtx));
	rtp_strcpy(ExampleServerCtx.rootDirectory, DEMO_WWW_ROOT);
	ExampleServerCtx.chunkedEncoding 		= 0;
	ExampleServerCtx.numHelperThreads 		= DEMO_MAX_HELPERS;
	ExampleServerCtx.numConnections		 	= DEMO_MAX_CONNECTIONS;
	rtp_strcpy(ExampleServerCtx.defaultFile,"index.htm");
	ExampleServerCtx.defaultIpAddr[0] = DEFAULT_DEMO_IPADDRESS[0];
	ExampleServerCtx.defaultIpAddr[1] = DEFAULT_DEMO_IPADDRESS[1];
	ExampleServerCtx.defaultIpAddr[2] = DEFAULT_DEMO_IPADDRESS[2];
	ExampleServerCtx.defaultIpAddr[3] = DEFAULT_DEMO_IPADDRESS[3];
	rtp_sprintf(ExampleServerCtx.defaultIpAddrstr, "%d.%d.%d.%d",
	ExampleServerCtx.defaultIpAddr[0],	ExampleServerCtx.defaultIpAddr[1],
	ExampleServerCtx.defaultIpAddr[2],	ExampleServerCtx.defaultIpAddr[3]);


	/* Initialize the server */
	if (http_server_demo_restart() != 0)
		return(-1);


	for (;;)
	{
		HTTP_ServerProcessOneRequest (&ExampleServerCtx.httpServer, 1000*60);
		if (ExampleServerCtx.timeToReload)
		{
			ExampleServerCtx.timeToReload = 0;
			HTTP_ServerDestroy(&ExampleServerCtx.httpServer, &ExampleServerCtx.connectCtxArray);
			rtp_free(ExampleServerCtx.connectCtxArray);
			/* Initialize the server */
			if (http_server_demo_restart() != 0)
				return(-1);
		}
	}


	rtp_net_exit();
	return (0);

}

static int http_server_demo_restart(void)
{
	HTTP_INT16 ipType = 4;

	/* Initialize the server */
	ExampleServerCtx.connectCtxArray = (HTTPServerConnection*) rtp_malloc(sizeof(HTTPServerConnection) * ExampleServerCtx.numConnections);
	if (!ExampleServerCtx.connectCtxArray)
		return -1;
	if (HTTP_ServerInit (
			&ExampleServerCtx.httpServer,
			DEMO_SERVER_NAME, // name
			ExampleServerCtx.rootDirectory,    // rootDir
			ExampleServerCtx.defaultFile,      // defaultFile
			1,                // httpMajorVersion
			1,                // httpMinorVersion
			0, // ExampleServerCtx.defaultIpAddr,   // Use the default IP address
			80,               // 80  ?? Use the www default port
			DEMO_IPVERSION,   // ipversion type(4 or 6)
			0,                // allowKeepAlive
			ExampleServerCtx.connectCtxArray,  // connectCtxArray
			ExampleServerCtx.numConnections,   // maxConnections
			ExampleServerCtx.numHelperThreads  // maxHelperThreads
		) < 0)
		return -1;

	/* Demonstrate using the server API by setting up virtual pages, and assigning callback handlers */
	/* Set up tables of virtual content that we will server from source code */
	http_demo_init_vcontent();

	/* Create "demo_form.html" uses a ram based web page and demonstrates froms processing */
	if (http_server_create_virtual_form(&ExampleServerCtx.httpServer) != 0)
	{
		return(-1);
	}
	/* Create a virtual default web page that we'll use as a sign on instructional page. */
	if (http_server_create_virtual_index(&ExampleServerCtx.httpServer) != 0)
	{
		return(-1);
	}
	return (0);
}
/* ========================================================================= */
/* Virtual index page code starts here. Demonstrates how to serve on the fly
   smart pages along side disk based pages */
static void _Demo_UrlRequestDestructor (void *userData);
static int _Demo_UrlRequestHandler (void *userData, HTTPServerRequestContext *ctx,  HTTPSession *session, HTTPRequest *request, RTP_NET_ADDR *clientAddr);

#define DEMO_INDEX_ID 		0
#define DEMO_AJAX_ID  		1
#define DEMO_JQUERY_ID 		2
#define DEMO_DISMISS_ID 	3
#define DEMO_CONFIGURE_ID 	4
#define DEMO_NUM_VIRT_URLS  5

#define DEMO_INDEX_URL 		"\\index.htm"
#define DEMO_AJAX_URL  		"\\ajax.htm"
#define DEMO_JQUERY_URL 	"\\jquery.htm"
#define DEMO_DISMISS_URL 	"\\dismiss.htm"
#define DEMO_CONFIGURE_URL 	"\\configure.htm"


static int http_server_create_virtual_index(HTTPServerContext *phttpServer)
{
	/* Make the server call _Demo_UrlRequestHandler when index.html, index.htm and \\ are accessed. Pass it DEMO_INDEX_ID*/
	if (HTTP_ServerAddPath(phttpServer, "\\index.html", 1,(HTTPRequestHandler)_Demo_UrlRequestHandler,(HTTPServerPathDestructor)_Demo_UrlRequestDestructor, DEMO_INDEX_ID) < 0)
		return (-1);
	/* Add "\\" */
	if (HTTP_ServerAddPath(phttpServer, "\\", 1, (HTTPRequestHandler)_Demo_UrlRequestHandler,
	                        (HTTPServerPathDestructor)_Demo_UrlRequestDestructor,   (void *) DEMO_INDEX_ID) < 0)
		return (-1);
	/* Add index.htm url */
	if (HTTP_ServerAddPath(phttpServer, DEMO_INDEX_URL, 1, (HTTPRequestHandler)_Demo_UrlRequestHandler,
	                        (HTTPServerPathDestructor)_Demo_UrlRequestDestructor,   (void *) DEMO_INDEX_ID) < 0)
		return (-1);
	/* Make the server call _Demo_UrlRequestHandler when ajax.html is accessed. Pass it DEMO_AJAX_ID*/
	if (HTTP_ServerAddPath(phttpServer, DEMO_AJAX_URL, 1, (HTTPRequestHandler)_Demo_UrlRequestHandler,
	                        (HTTPServerPathDestructor)_Demo_UrlRequestDestructor,   (void *)DEMO_AJAX_ID) < 0)
		return (-1);
	/* Make the server call _Demo_UrlRequestHandler when jquery.htm url */
	if (HTTP_ServerAddPath(phttpServer, DEMO_JQUERY_URL, 1, (HTTPRequestHandler)_Demo_UrlRequestHandler,
	                        (HTTPServerPathDestructor)_Demo_UrlRequestDestructor,   (void *)DEMO_JQUERY_ID) < 0)
		return (-1);
	/* Make the server call _Demo_UrlRequestHandler when dismiss.htm url */
	if (HTTP_ServerAddPath(phttpServer, DEMO_DISMISS_URL, 1, (HTTPRequestHandler)_Demo_UrlRequestHandler,
	                        (HTTPServerPathDestructor)_Demo_UrlRequestDestructor,   (void *)DEMO_DISMISS_ID) < 0)
		return (-1);
	/* Make the server call _Demo_UrlRequestHandler when configure.htm url */
	if (HTTP_ServerAddPath(phttpServer, DEMO_CONFIGURE_URL, 1, (HTTPRequestHandler)_Demo_UrlRequestHandler,
	                        (HTTPServerPathDestructor)_Demo_UrlRequestDestructor,   (void *)DEMO_CONFIGURE_ID) < 0)
		return (-1);
	return 0;

}

/*---------------------------------------------------------------------------*/
static void _Demo_UrlRequestDestructor (void *userData)
{
}

static char *virtual_index_content[]  = {
"<html><body><hr><center><b><big>Welcome to the EBS WEB Server Demo</big></b></center><hr><br><br>",
"<center>Login Page</center><br>",
"Select an option <br>",
"<a href=\"demo_form.htm\">Demonstrate Simple Forms Processing.</a><br>",
"<a href=\"configure.htm\">Setup And Configuration Web Server.</a><br>",
"<a href=\"ajax.htm\">Demonstrate Using Ajax an Interactive Client Server Application.</a><br>",
"<a href=\"jquery.htm\">Demonstrate Using Jquery To Add Client Server UI.</a><br>",
"<a href=\"dismiss.htm\">Revert to disk based html index.</a><br>",
"<br><hr>",
"</body></html>",
0};
static char *virtual_dismiss_content[]  = {
"<html><body><hr><center><b><big>The disk based default index is visible now.</big></b></center><hr><br><br>",
"<hr>",
"<a href=\"index.htm\">Click hear to go to index.htm in the file system.</a><br>",
"<a href=\"index.html\">Click hear to go to index.html in the file system.</a><br>",
"<br><hr>",
"</body></html>",
0};

static int _Demo_IndexFileProcessHeader(void *userData,	HTTPServerRequestContext *server, HTTPSession *session,HTTPHeaderType type, const HTTP_CHAR *name,	const HTTP_CHAR *value);

static int _Demo_UrlRequestHandler (void *userData,
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
    if (virtUrlId == DEMO_INDEX_ID && ExampleServerCtx.disableVirtualIndex)
		return(HTTP_REQUEST_STATUS_CONTINUE);
	/* If the dismiss link was clicked respond with a message but disable the virtual index feature on the next pass */
    if (virtUrlId == DEMO_DISMISS_ID)
		ExampleServerCtx.disableVirtualIndex = 1;

	switch (request->methodType)
	{
		case HTTP_METHOD_GET:
		{
			HTTPResponse response;
			HTTP_UINT8 tempBuffer[256];
			HTTP_INT32 tempBufferSize = 256;
			int  use_static_content;
			HTTP_INT32 vfilesize;
			char **virtual_content;

			switch (virtUrlId)
			{
				default:
				case DEMO_CONFIGURE_ID:
					use_static_content = 0;
					break;
				case DEMO_INDEX_ID:
				case DEMO_AJAX_ID:
				case DEMO_JQUERY_ID:
				case DEMO_DISMISS_ID:
					use_static_content = 1;
					break;
			}

			HTTP_ServerReadHeaders(	ctx,session, _Demo_IndexFileProcessHeader, 0, tempBuffer, tempBufferSize);

			HTTP_ServerInitResponse(ctx, session, &response, 200, "OK");
			HTTP_ServerSetDefaultHeaders(ctx, &response);
			HTTP_SetResponseHeaderStr(&response, "Content-Type", "text/html");

			if (use_static_content)
				virtual_content = http_demo_retrieve_vcontent(virtUrlId);
			else
				virtual_content = http_demo_generate_vcontent(virtUrlId);

			vfilesize = http_demo_count_vcontent_bytes(virtual_content);
				HTTP_SetResponseHeaderInt(&response, "Content-Length", vfilesize);
//			HTTP_SetResponseHeaderTime(&response, "Last-Modified", &vfile->creationTime);

			HTTP_WriteResponse(session, &response);
			http_demo_write_vcontent_bytes(session, virtual_content);

			HTTP_WriteFlush(session);

			if (!use_static_content)
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


/* ========================================================================= */
/* Virtual form code starts here. Demonstrates how to serve on the fly
   forms and how to process form data */

static char *demo_form_content = "\
<form name=\"input\" action=\"demo_form_submit\" method=\"get\">\
I have a bike: <input type=\"checkbox\" name=\"vehicle\" value=\"Bike\" /> <br />\
I have a car: <input type=\"checkbox\" name=\"vehicle\" value=\"Car\" /> <br />\
I have an airplane: <input type=\"checkbox\" name=\"vehicle\" value=\"Airplane\" />\
Username:<input type=\"text\" name=\"user\" />\
<input type=\"submit\" value=\"Submit\" /><br>\
</form> ";

static int demo_form_cb (void *userData, HTTPServerRequestContext *ctx, HTTPSession *session, HTTPRequest *request, RTP_NET_ADDR *clientAddr);

static int http_server_create_virtual_form(HTTPServerContext *phttpServer)
{
	if (HTTP_ServerAddVirtualFile(phttpServer, "\\demo_form.htm",  demo_form_content, rtp_strlen(demo_form_content), "text/html") < 0)
		return -1;

	if (HTTP_ServerAddPostHandler(phttpServer, "\\demo_form_submit", demo_form_cb, (void *)"demo_form_submit") < 0)
		return -1;

	rtp_printf("A virtual page named demo_form.html has been created \n");
	return 0;
}

/*---------------------------------------------------------------------------*/
static int _demo_form_ProcessHeader(void *userData,	HTTPServerRequestContext *server, HTTPSession *session, HTTPHeaderType type, const HTTP_CHAR *name, const HTTP_CHAR *value);
static int demo_form_cb (void *userData, HTTPServerRequestContext *ctx, HTTPSession *session, HTTPRequest *request, RTP_NET_ADDR *clientAddr)
{
	HTTPServerContext *server = ctx->server;
	switch (request->methodType)
	{
		case HTTP_METHOD_GET:
		{
			HTTPResponse response;
			HTTP_UINT8 tempBuffer[256];
			HTTP_INT32 tempBufferSize = 256;
			HTTP_UINT8 *respBuffer;
			HTTP_INT32 respBufferSize = 4096;
			HTTP_CHAR *p;
			int pairs = 0;


			/* Allocate a buffer for sending response. */
			respBuffer = (HTTP_UINT8 *) rtp_malloc(respBufferSize);
			*respBuffer = 0;

			/* Parse the submittd values from the command line and send them back to the user in a document */
			p = rtp_strstr(request->target, "?");
			if (p)
			{
			HTTP_UINT8 *name;
			HTTP_UINT8 *value;
				rtp_strncpy(tempBuffer, p, tempBufferSize);
				p++; /* Past '?' */
				rtp_strncpy(tempBuffer, p, tempBufferSize);
				tempBuffer[tempBufferSize-1] = 0;
				p = tempBuffer;
				rtp_strcat(respBuffer, "<hr>You submitted these values: <hr>");
				while (p)
				{
					name = p;
					p = rtp_strstr(p, "=");
					if (p)
					{
					HTTP_UINT8 *savep0,*savep1;
						savep0 = savep1 = 0;
						savep0 = p;
						*p++ = 0;
						value = p;
						p = rtp_strstr(p, "&");
						if (p)
						{
							savep1 = p;
							*p++ = 0;
						}
						rtp_strcat(respBuffer, name);
						rtp_strcat(respBuffer, " = ");
						rtp_strcat(respBuffer, value);
						rtp_strcat(respBuffer, "<br>");
						if (savep0)
							*savep0 = (HTTP_UINT8) '=';
						if (savep1)
							*savep1 = (HTTP_UINT8) '&';
					}
				}
			}
			/* Read the HTTP headers and send them back to the user in the document
			   respBuffer is passed to _demo_form_ProcessHeader once for each NV pair found in the header section
			   tempBuffer is used internally to recv the headers */
			rtp_strcat(respBuffer, "<hr>Your browser passed these HTTP header values: <hr>");
			HTTP_ServerReadHeaders(	ctx, session, _demo_form_ProcessHeader, respBuffer, tempBuffer, tempBufferSize);

			rtp_strcat(respBuffer, "<hr>Thank you ... and have a nice day<hr>");
			rtp_strcat(respBuffer, "<br><a href=\"index.htm\">Click here to return to main index.</a><br>");

			HTTP_ServerInitResponse(ctx, session, &response, 200, "OK");
			HTTP_ServerSetDefaultHeaders(ctx, &response);
			HTTP_SetResponseHeaderStr(&response, "Content-Type", "text/html");

			HTTP_SetResponseHeaderInt(&response, "Content-Length", rtp_strlen(respBuffer));

			HTTP_WriteResponse(session, &response);
			HTTP_Write(session, respBuffer, rtp_strlen(respBuffer));
			HTTP_WriteFlush(session);

			rtp_free(respBuffer);
			HTTP_FreeResponse(&response);

			return (HTTP_REQUEST_STATUS_DONE);
		}

		case HTTP_METHOD_HEAD: /* tbd - implement HEAD method */
		default:
			return (HTTP_REQUEST_STATUS_CONTINUE);
	}
}


static int _demo_form_ProcessHeader(
		void *userData,
		HTTPServerRequestContext *server,
		HTTPSession *session,
		HTTPHeaderType type,
		const HTTP_CHAR *name,
		const HTTP_CHAR *value)
{
HTTP_CHAR *respBuffer;
	respBuffer = (HTTP_CHAR *)userData;
	if (!name)
		name = "No Name";
	if (!value)
		value = "No Value";
	rtp_strcat(respBuffer, name);
	rtp_strcat(respBuffer, " = ");
	rtp_strcat(respBuffer, value);
	rtp_strcat(respBuffer, "<br>");
	return (0);
}


/*Initialize an array of pages we will serve from ourt application */
char **pVirtualContent[DEMO_NUM_VIRT_URLS];
static void http_demo_init_vcontent(void)
{
	pVirtualContent[DEMO_INDEX_ID] 		= virtual_index_content;
	pVirtualContent[DEMO_AJAX_ID] 		= virtual_index_content;
	pVirtualContent[DEMO_JQUERY_ID] 	= virtual_index_content;
	pVirtualContent[DEMO_DISMISS_ID]	= virtual_dismiss_content;
	pVirtualContent[DEMO_CONFIGURE_ID] 	= virtual_index_content;
}
/*Look up a page we will serve when the url is accessed */
static char **http_demo_retrieve_vcontent(HTTP_INT32 contentid)
{
	if (contentid >= DEMO_NUM_VIRT_URLS)
		contentid = 0;
	return(pVirtualContent[contentid]);
}
/* Get the total length of an array of character strings */
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
	while (*_pContent)
	{
		rtp_free(*_pContent);
		_pContent++;
	}
	rtp_free(pContent);
}

static HTTP_CHAR *http_demo_alloc_string(HTTP_CHAR *string)
{
HTTP_CHAR *r;
	r = rtp_malloc(rtp_strlen(string)+1);
	if (r)
		rtp_strcpy(r, string);
	return(r);
}



static int http_demo_generate_configform(char **pContent)
{
	int line = 0;
	char temp_buff[512];
	pContent[line++] =	http_demo_alloc_string("<html><body><hr><center><b><big>Server Configuration form.</big></b></center><hr><br><br>");
	pContent[line++] =	http_demo_alloc_string("<form name=\"configure\" action=\"demo_configure\" method=\"get\">");

	rtp_sprintf(temp_buff,"Content Directory :<input type=\"text\" name=\"root\" value=%s/>", ExampleServerCtx.rootDirectory);
	pContent[line++] =	http_demo_alloc_string(temp_buff);
//	HTTP_UINT8         defaultFile[256];
//  	HTTP_BOOL          disableVirtualIndex;
//  	HTTP_BOOL          chunkedEncoding;
//	HTTP_INT16         numHelperThreads;
//	HTTP_INT16         numConnections;
//	HTTP_UINT8         defaultIpAddr[4];
//	HTTP_CHAR          defaultIpAddrstr[256];
//	HTTPServerConnection* connectCtxArray;
//	HTTPServerContext  httpServer;
//  	HTTP_BOOL          timeToReload;

	/* Create a form initialized from our current configuration */
	pContent[line++] =	http_demo_alloc_string("<form>"	);
	pContent[line++] =	http_demo_alloc_string("</form>"	);
	pContent[line++] =	http_demo_alloc_string("</body></html>"	);
	pContent[line++] =	0;

//  HEREHERE
//
//	if (HTTP_ServerAddPostHandler(phttpServer, "\\demo_form_submit", demo_form_cb, (void *)"demo_form_submit") < 0)
//		return -1;

	return(0);
}
static char **http_demo_generate_vcontent(HTTP_INT32 virtUrlId)
{
	char **pContent	= rtp_malloc(sizeof(char *) * 1024);
	switch (virtUrlId)
	{
		case DEMO_CONFIGURE_ID:
			http_demo_generate_configform(pContent);
		break;
		case DEMO_INDEX_ID:
		case DEMO_AJAX_ID:
		case DEMO_JQUERY_ID:
		case DEMO_DISMISS_ID:
		default:
			pContent[0] =	http_demo_alloc_string(
				"<html><body><hr><center><b><big>No virtual content found for that link.</big></b></center><hr><br><br></body></html>"
				);
			pContent[1] =	0;
		break;
	}
	return(pContent);
}
