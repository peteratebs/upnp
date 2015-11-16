/*
|  example_server_advanced.c
|
|  EBS -
|
|   $Author:  $
|   $Date: 2009/08/22 19:14:06 $
|   $Name:  $
|   $Revision: 1.2 $
|
|  Copyright EBS Inc. , 2009
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


 /* A context structure for keeping our application data
    This is a duplicate of the typedef in example_server.c
    this is bad form but not requiring an additional header file
    implifies the examples for documentation purposes. */
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

extern HTTPExampleServerCtx ExampleServerCtx;

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

static char **http_demo_generate_vcontent(HTTPServerContext  *httpServer,HTTP_INT32 virtUrlId);
static void http_demo_release_vcontent(HTTP_INT32 virtUrlId, char **pContent);

/* Exported */
int http_demo_init_vcontent(HTTPServerContext  *phttpServer);
char **http_demo_retrieve_vcontent(HTTPServerContext  *httpServer,HTTP_INT32 contentid);


/* Imported */
void _Demo_UrlRequestDestructor (void *userData);
int _Demo_UrlRequestHandler (void *userData, HTTPServerRequestContext *ctx,  HTTPSession *session, HTTPRequest *request, RTP_NET_ADDR *clientAddr);
HTTP_CHAR *http_demo_alloc_string(HTTP_CHAR *string);

/*****************************************************************************/
/* Data
 *****************************************************************************/

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/
/*---------------------------------------------------------------------------*/

/* ========================================================================= */
/* Virtual index page code starts here. Demonstrates how to serve on the fly
   smart pages along side disk based pages */

#define DEMO_INDEX_ID 		0
#define DEMO_DISMISS_ID 	1
#define DEMO_AJAX_ID  		2
#define DEMO_JQUERY_ID 		3
#define DEMO_CONFIGURE_ID 	4
#define DEMO_NUM_VIRT_URLS  5

#define DEMO_AJAX_URL  		"\\ajax.htm"
#define DEMO_JQUERY_URL 	"\\jquery.htm"
#define DEMO_CONFIGURE_URL 	"\\configure.htm"


static int http_server_create_virtual_index(HTTPServerContext *phttpServer)
{
	/* Make the server call _Demo_UrlRequestHandler when ajax.html is accessed. Pass it DEMO_AJAX_ID*/
	if (HTTP_ServerAddPath(phttpServer, DEMO_AJAX_URL, 1, (HTTPRequestHandler)_Demo_UrlRequestHandler,
	                        (HTTPServerPathDestructor)_Demo_UrlRequestDestructor,   (void *)DEMO_AJAX_ID) < 0)
		return (-1);
	/* Make the server call _Demo_UrlRequestHandler when jquery.htm url */
	if (HTTP_ServerAddPath(phttpServer, DEMO_JQUERY_URL, 1, (HTTPRequestHandler)_Demo_UrlRequestHandler,
	                        (HTTPServerPathDestructor)_Demo_UrlRequestDestructor,   (void *)DEMO_JQUERY_ID) < 0)
		return (-1);
	/* Make the server call _Demo_UrlRequestHandler when configure.htm url */
	if (HTTP_ServerAddPath(phttpServer, DEMO_CONFIGURE_URL, 1, (HTTPRequestHandler)_Demo_UrlRequestHandler,
	                        (HTTPServerPathDestructor)_Demo_UrlRequestDestructor,   (void *)DEMO_CONFIGURE_ID) < 0)
		return (-1);
	return 0;
}

static int _Demo_IndexFileProcessHeader(void *userData,	HTTPServerRequestContext *server, HTTPSession *session,HTTPHeaderType type, const HTTP_CHAR *name,	const HTTP_CHAR *value);



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
int processing_configure = 0;

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

			if (rtp_strcmp("demo_configure_submit", (char *) userData) == 0)
			{
				processing_configure = 1;
			}

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

						if (processing_configure)
						{
							if (rtp_strcmp(name, "root") == 0)
							{ /* Un-escape %2F and %5C to / and \ respectively. Also strip trailing / or \ */
								int i; char *pv,c;
								pv = value;
								for ( i = 0; i < sizeof(ExampleServerCtx.rootDirectory); i++)
								{
									if (*pv == '%' && *(pv+1) == '2' && (*(pv+2) == 'F' || *(pv+2) == 'f'))
										{c = '/'; pv+= 3; if (*pv == 0) c = 0;}
									else if (*pv == '%' && *(pv+1) == '5' && (*(pv+2) == 'C' || *(pv+2) == 'c'))
										{c = '\\'; pv+= 3; if (*pv == 0) c = 0;}
									else
										{c = *pv; pv+=1;}
									ExampleServerCtx.rootDirectory[i] = c;
									if (c == 0)
										break;
								}
								ExampleServerCtx.ModuleRequestingReload = 1;
							}
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

			if (processing_configure)
				rtp_strcat(respBuffer, "<hr>The configruation was changed. To test your results select the option to revert to disk based html from the main page.<br><br>");
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

static char *ajax_content[]  = {
"<html><body><hr><center><b><big>Not done yet</big></b></center><hr><br><br>",
"<br><a href=\"index.htm\">Click here to return to main index.</a><br>",
"</body></html>",
0};

/*Initialize an array of pages we will serve from ourt application */
char **pVirtualContent[DEMO_NUM_VIRT_URLS];
int http_demo_init_vcontent(HTTPServerContext  *phttpServer)
{
	pVirtualContent[0] 		= 0;
	pVirtualContent[1] 		= 0;
	pVirtualContent[DEMO_AJAX_ID] 		= ajax_content;
	pVirtualContent[DEMO_JQUERY_ID] 	= ajax_content;
	pVirtualContent[DEMO_CONFIGURE_ID] 	= ajax_content;


	/* Create "demo_form.html" uses a ram based web page and demonstrates forms processing */
	if (http_server_create_virtual_form(phttpServer) != 0)
	{
		return(-1);
	}
	/* Create a virtual default web page that we'll use as a sign on instructional page. */
	if (http_server_create_virtual_index(phttpServer) != 0)
	{
		return(-1);
	}
	return(0);

}
/*Look up a page we will serve when the url is accessed */
char **http_demo_retrieve_vcontent(HTTPServerContext  *httpServer,HTTP_INT32 contentid)
{
	if (contentid == DEMO_CONFIGURE_ID)
		return(http_demo_generate_vcontent(httpServer,contentid));

	if (contentid >= DEMO_NUM_VIRT_URLS)
		contentid = 0;
	return(pVirtualContent[contentid]);
}

static int http_demo_generate_configform(HTTPServerContext  *httpServer,char **pContent)
{
	int line = 0;
	char temp_buff[512];
	pContent[line++] =	http_demo_alloc_string("<html><body><hr><center><b><big>Server Configuration form.</big></b></center><hr><br><br>");
	pContent[line++] =	http_demo_alloc_string("<form name=\"configure\" action=\"demo_configure_submit\" method=\"get\">");

	pContent[line++] =	http_demo_alloc_string("<br>Enter a new subdirectory to serve content from. And submit the form. <br>");
	pContent[line++] =	http_demo_alloc_string("The PostHandler function will change the configuration parameter <br>and arrange for the top level of the ");
	pContent[line++] =	http_demo_alloc_string(" loop to reconfigure the server. <br>");
	pContent[line++] =	http_demo_alloc_string(" <i>Changes will not be noticed until you Revert to disk based html index and refresh.</i> <br>");

	rtp_sprintf(temp_buff,"Content Directory :<input type=\"text\" name=\"root\" value=%s/>", ExampleServerCtx.rootDirectory);
	pContent[line++] =	http_demo_alloc_string(temp_buff);
	pContent[line++] =	http_demo_alloc_string("<input type=\"submit\" value=\"Submit\" /><br>");

	/* Create a form initialized from our current configuration */
	pContent[line++] =	http_demo_alloc_string("</form>"	);
	pContent[line++] =	http_demo_alloc_string("</body></html>"	);
	pContent[line++] =	0;

	if (HTTP_ServerAddPostHandler(httpServer, "\\demo_configure_submit", demo_form_cb, (void *)"demo_configure_submit") < 0)
		return -1;

	return(0);
}
static char **http_demo_generate_vcontent(HTTPServerContext  *httpServer, HTTP_INT32 virtUrlId)
{
	char **pContent	= rtp_malloc(sizeof(char *) * 1024);
	switch (virtUrlId)
	{
		case DEMO_CONFIGURE_ID:
			http_demo_generate_configform(httpServer,pContent);
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
