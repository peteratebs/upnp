/*---------------------------------------------------------------------------*/
#include "httpp.h"
#include "winsock.h"
#include "stdio.h"
#include "httpsrv.h"
#include "conio.h"
#include "upnp.h"

typedef struct 
{
	SOCKET sd;
}
WinsockStruct;

#define TEST_BUFFER_SIZE     8192

int getHostAddress (struct sockaddr_in *sin, const char *hostName, short port);

void clientTest (void);
void simpleServerTest (void);
void serverTest (void);
void secureServerTest (void);

/*---------------------------------------------------------------------------*/
HTTP_INT32 testHttpReadFn  (void *streamPtr, HTTP_UINT8 *buffer, HTTP_INT32 min, HTTP_INT32 max);
HTTP_INT32 testHttpWriteFn (void *streamPtr, const HTTP_UINT8 *buffer, HTTP_INT32 size);

int testProcessHeader(
		void *userData, 
		HTTPSession *session, 
		HTTPHeaderType type,
		const HTTP_CHAR *name, 
		const HTTP_CHAR *value);
		
struct WSAData wsa_data;

HTTP_UINT8 testBuffer[TEST_BUFFER_SIZE];
HTTP_INT32 testBufferSize = TEST_BUFFER_SIZE;

/*---------------------------------------------------------------------------*/
int main (void)
{
	/* first initialize winsock API */
	if (WSAStartup(0x0101, &wsa_data))
	{
		return (-1);
	}

	//clientTest();
	//simpleServerTest();
	serverTest();
	//secureServerTest();

	return (0);
}

/*---------------------------------------------------------------------------*/
void clientTest (void)
{
int result;
HTTPSession session;
WinsockStruct s;
/*unsigned long ioctl_arg;*/

	/* allocate a socket */
	s.sd = socket(AF_INET, SOCK_STREAM, 0);
	if (s.sd != INVALID_SOCKET)
	{
		struct sockaddr_in sin;
		char serverName[] = "www.google.com";
		char path[] = "/";
		char method[] = "GET";

		/* try looking up the name via DNS */
		if (getHostAddress(&sin, serverName, 80) >= 0)
		{
			/* connect to the server */
			
			if (connect(s.sd, (PSOCKADDR) &sin, sizeof(sin)) >= 0)
			{
				HTTPRequest request;
				HTTPResponse response;
				HTTP_INT32 len;
				
				/* set the socket to non-blocking mode */
				/*
				ioctl_arg = 1;
				if (ioctlsocket(s.sd, FIONBIO, &ioctl_arg) < 0)
				{
					return (-1);
				}
				*/
				
				/* create an HTTP session */
				result = HTTP_InitSession (&session, 
			                               testHttpReadFn,	
			                               testHttpWriteFn,
			                               &s);
				
				result = HTTP_InitRequest(&session, &request, method, path, 1, 0);
				
				result = HTTP_SetRequestHeaderStr(&request, "Host", serverName);
				result = HTTP_SetRequestHeaderStr(&request, "User-Agent", "HTTP test client");
				result = HTTP_SetRequestHeaderStr(&request, "Accept", "text/*");
				result = HTTP_SetRequestHeaderStr(&request, "Connection", "close");				
				
				result = HTTP_WriteRequest(&session, &request);
				
				HTTP_FreeRequest(&request);

				len = HTTP_ReadResponse(&session, &response, testBuffer, testBufferSize);

				HTTP_ReadHeaders(&session, testProcessHeader, 0, testBuffer + len, testBufferSize - len);

				HTTP_FreeSession(&session);		
			}
		}
		
		/* free the socket */
		closesocket(s.sd);
	}	
}

HTTPServerContext gServer;
	
#include "gena.h"
void _GENA_GenerateSubscriptionId (GENA_CHAR *sid, 
                                   const GENA_CHAR *callbackURL, 
                                   const GENA_CHAR *eventURL);

#include "soap.h"



int _test_SOAPServerCallback (
		SOAPServerContext *ctx, 
		SOAPRequest *request, 
		void *cookie)
{
	request->out.body = ixmlParseBuffer (		
			"<u:SetTargetResponse xmlns:u=\"urn:schemas-upnp-org:service:SwitchPower:1\" />");
	
	return (0);
}		

/*---------------------------------------------------------------------------*/
void secureServerTest (void)
{
RTP_SSL_CONTEXT sslContext;

	if (rtp_ssl_init_context (
			&sslContext, 
			RTP_SSL_MODE_SERVER|RTP_SSL_MODE_SSL2|RTP_SSL_MODE_SSL3|RTP_SSL_MODE_TLS1,
			RTP_SSL_VERIFY_NONE
		) >= 0)
	{
		if (HTTPS_InitServer(&gServer, 0, 443) >= 0)
		{
			/* set some options */
			HTTP_ServerSetRootDir(&gServer, "C:\\www-root\\");

			HTTPS_ServerStartDaemon(sslContext, &gServer, 5, 0);
			
			getch();
			HTTPS_ServerStopDaemon(&gServer, 5);
			HTTPS_FreeServer(&gServer);
		}
		rtp_ssl_close_context(sslContext);
	}
}

/*---------------------------------------------------------------------------*/
void serverTest (void)
{
/*
GENAContext ctx;
GENANotifier notifier;
GENA_CHAR test1[64];
GENA_CHAR test2[64];
GENA_CHAR test3[64];
*/
SOAPServerContext soapContext;

	if (HTTP_InitServer(&gServer, 0, 80) < 0)
	{
		return;
	}
	
	/* set some options */
	HTTP_ServerSetRootDir(&gServer, "C:\\www-root\\");
	
	SOAP_ServerInit(&soapContext, _test_SOAPServerCallback, 0);

	HTTP_ServerAddPath(&gServer, "/soaptest/service.blah", 1, SOAP_ServerProcessRequest, &soapContext);

	HTTP_ServerStartDaemon(&gServer, 5, 0);
/*
	_GENA_GenerateSubscriptionId(test1, "www.amazon.com", "www.google.com");
	_GENA_GenerateSubscriptionId(test2, "www.cnn.com", "www.google.com");
	_GENA_GenerateSubscriptionId(test3, "www.amazon.com", "www.cnn.com");

	GENA_InitNotifier(&ctx, &notifier);
	//GENA_AddSubscriber(&ctx, &notifier, "<http://66.189.87.81/directory/post.dll>", test1, 100);
	//GENA_AddSubscriber(&ctx, &notifier, "<http://66.189.87.81:123/directory/post.dll>", test2, 100);
	GENA_AddSubscriber(&ctx, &notifier, "http://66.189.87.81/bugzilla/index.html", test1, GENA_TIMEOUT_INFINITE);
	GENA_AddSubscriber(&ctx, &notifier, "http://66.189.87.81:80/directory/post.dll", test2, GENA_TIMEOUT_INFINITE);
	GENA_AddSubscriber(&ctx, &notifier, "<http://66.189.87.81/directory/post/index.dll>", test3, GENA_TIMEOUT_INFINITE);

	GENA_SendNotify(&ctx, &notifier, "upnp:event", "upnp:propchange", "<xml>blah.</xml>", 16);
*/	
	getch();
	HTTP_ServerStopDaemon(&gServer, 5);

	/*
	while (1)
	{
		HTTP_ServerProcessOneRequest(&server, 0xfffffff);
	}
	*/
}

/*---------------------------------------------------------------------------*/
void simpleServerTest (void)
{
int result;
HTTPSession session;
SOCKET serverSocket;
WinsockStruct s;
/*unsigned long ioctl_arg;*/

	/* allocate a socket */
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket != INVALID_SOCKET)
	{
		struct sockaddr_in sin;
		char serverName[] = "192.168.1.110";

		/* try looking up the name via DNS */
		if (getHostAddress(&sin, serverName, 1881) >= 0)
		{
			if (bind(serverSocket, (PSOCKADDR) &sin, sizeof(sin)) >= 0)
			{
				if (listen(serverSocket, 5) >= 0)
				{
					while (1)
					{
						s.sd = accept(serverSocket, 0, 0);
						if (s.sd != INVALID_SOCKET)
						{
							HTTPRequest request;
							HTTPResponse response;
							HTTP_INT32 len;
							RTP_TIMESTAMP currentTime;
							char htmlText[1024];

							/* get the current system time/date */
							rtp_date_get_timestamp(&currentTime);

							/* create an HTML document */
							strcpy(htmlText, "<html><body><i>The current time is:</i> ");
							rtp_date_print_time(htmlText + strlen(htmlText), &currentTime, 0);
							strcat(htmlText, "</body></html>");
							
							/* create an HTTP session */
							result = HTTP_InitSession (&session, 
													   testHttpReadFn,	
													   testHttpWriteFn,
													   &s);
							
							/* Read the request */
							len = HTTP_ReadRequest(&session,
												   &request,
												   testBuffer,
												   testBufferSize);

							HTTP_ReadHeaders(&session, testProcessHeader, 0, testBuffer + len, testBufferSize - len);
						 
					 		/* Create a response message */
					 		HTTP_InitResponse(&session,
					 						  &response,
					 						  1, 0,
					 						  200, 
					 						  "OK");
					 						  
							HTTP_SetResponseHeaderStr(&response, "Server", "HTTP Test Server");
							HTTP_SetResponseHeaderStr(&response, "Content-Type", "text/html");
							HTTP_SetResponseHeaderInt(&response, "Content-Length", strlen(htmlText));
							HTTP_SetResponseHeaderStr(&response, "Pragma", "no-cache");
							HTTP_SetResponseHeaderStr(&response, "Cache-Control", "no-cache, must revalidate");
							HTTP_SetResponseHeaderTime(&response, "Date", &currentTime);
							HTTP_SetResponseHeaderTime(&response, "Expires", &currentTime);
							HTTP_SetResponseHeaderTime(&response, "Last-Modified", &currentTime);
							
							/* buffer up to an ethernet MTU */
							HTTP_SetWriteBufferSize(&session, 1452);
							
							/* write the response message, the data, and flush the write buffer */						
							HTTP_WriteResponse(&session, &response);												
							HTTP_Write(&session, htmlText, strlen(htmlText));						
							HTTP_WriteFlush(&session);

							/* free the response and the session */
							HTTP_FreeResponse(&response);						
							HTTP_FreeSession(&session);
							
							/* close the connection */
							closesocket(s.sd);
						}
					}
				}
			}
		}

		/* free the socket */
		closesocket(serverSocket);
	}
}


/*---------------------------------------------------------------------------*/
HTTP_INT32 testHttpReadFn  (void *streamPtr, HTTP_UINT8 *buffer, HTTP_INT32 min, HTTP_INT32 max)
{
	WinsockStruct *s = (WinsockStruct *) streamPtr;
	return (recv(s->sd, buffer, max, 0));
}

/*---------------------------------------------------------------------------*/
HTTP_INT32 testHttpWriteFn (void *streamPtr, const HTTP_UINT8 *buffer, HTTP_INT32 size)
{
	WinsockStruct *s = (WinsockStruct *) streamPtr;
	return (send(s->sd, buffer, size, 0));
}



/*************************************************************************
 webc_ip_to_str() - Convert 4 byte ip address to dotted string

 ipstr - 13 char array : the buffer to fill with dotted string
 ipaddr - 4 byte array : the ip address to convert
 
 Returns: ipstr
*************************************************************************/

char * webc_ip_to_str(char * ipstr, unsigned char * ipaddr)
{
int n;

	ipstr[0] = '\0';

	for (n=0; n<4; n++)	
	{
		tc_itoa(ipaddr[n], &(ipstr[tc_strlen(ipstr)]), 10);
		if (n<3)
		{
			tc_strcat(ipstr, ".");
		}
	}
	
	return (ipstr);
}


/*************************************************************************
 webc_str_to_ip() - Convert dotted string to 4 byte ip address

 ipaddr -  a 4-byte buffer to fill with the ip address
 ipstr -   the dotted string to convert
 
 Notes:
 	"111.111.111.111" converts to {111,111,111,111}
 	"111.111.111"     converts to {111,111,111,0}
 	"111.111"         converts to {111,111,0,0}
 	"111"             converts to {111,0,0,0}
 
 Returns: ipaddr
*************************************************************************/

unsigned char * webc_str_to_ip(unsigned char * ipaddr, const char * ipstr)
{
int n, i;

	tc_memset(ipaddr, 0, 4);

	for (n=0, i=0; ipstr[n] && i<4; n++)
	{
		if (IS_DIGIT(ipstr[n]))
		{
			ipaddr[i] = (ipaddr[i] * 10) + (ipstr[n] - '0');
		}
		else if (ipstr[n] == '.')
		{
			i++;
		}
		else
		{
			break;
		}
	}
	
	return (ipaddr);
}

/*---------------------------------------------------------------------------*/
int getHostAddress (struct sockaddr_in *sin, const char *hostName, short port)
{
	struct hostent * hostent;

	sin->sin_family = AF_INET;
	sin->sin_port   = htons(port);
	
	// first try the name as a numbered IP address 
	if (IS_DIGIT(hostName[0]))
	{
		char testStr[16];

		webc_str_to_ip((unsigned char *) &sin->sin_addr, hostName);
		
		// do a check to make sure we translated an IP address
		webc_ip_to_str(testStr, (unsigned char *) &sin->sin_addr);
		if (!strcmp(hostName, testStr))
		{
			// if we go from string to ip addr and back, without
			//  changing the host name, this is success.
			return(0);
		}
	}
	
	// try looking up the name via DNS
	if ((hostent = gethostbyname(hostName)))
	{
		memcpy((unsigned char *) &sin->sin_addr, (unsigned char *) hostent->h_addr, 4);
		return(0);
	}
	
	return (-1);
}

/*---------------------------------------------------------------------------*/
int testProcessHeader(
		void *userData, 
		HTTPSession *session, 
		HTTPHeaderType type,
		const HTTP_CHAR *name, 
		const HTTP_CHAR *value)
{
	printf("%s: %s\n", name, value);
	return (0);
}		
