/*
|  MAIN.C -
|
|  EBS -
|
|   $Author: vmalaiya $
|   $Date: 2006/11/29 19:14:06 $
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

#include "rtpnet.h"
#include "rtpscnv.h"
#include "ssdpcli.h"
//#include "stdio.h"
#include "upnpcli.h"
#include "rtptime.h"
#include "rtpthrd.h"
//#include "rtpterm.h"
#include "upnpdom.h"
//#include <conio.h>
#include "rtpgui.h"
#include "rtpprint.h"
/*****************************************************************************/
/* Macros
 *****************************************************************************/
#define MEDIA_RENDERER_UDN          "019297f7-9a9e-4c3a-b419-7e9cb6c124ba"
#define MEDIA_SERVER_UDN            "8d3ccfa4-7349-11db-830a-00107500015c"
#define SEARCH_DURATION             2000
#define BROWSE_ELEMENT_COUNT        "10"
/* Not to be changed */
#define MEDIA_SERVER_DEVTYPE        "MediaServer:1"
#define MEDIA_RENDERER_DEVTYPE      "MediaRenderer:1"

#define CPDEMO_PLAY 0
#define CPDEMO_STOP 1
#define CPDEMO_QUIT 2




/*****************************************************************************/
/* Types
 *****************************************************************************/
struct actionUserData
{
    int   done;
    int   numOutElements;
	int   errorCode;
	char* errorDescription;
	char*  outVariableName[10];
	char*  outVariableValue[10];
};

/*****************************************************************************/
/* Function Prototypes
/*****************************************************************************/

int controlPointCallback (
		UPnPControlPoint* cp,
		UPnPControlEvent* event,
		void *perControl,
		void *perRequest
	);

static int parseDirectoryResults(RTPXML_Document *xmlResult);
static int parseFileResults(RTPXML_Document *xmlResult);

static int _cpdemo_ask_render_command(void);

void openMediaDevice (UPnPRuntime *rt, UPnPControlPoint *cp, char *deviceUrl, char* deviceType);
void discoverMediaServer (UPnPRuntime *rt, UPnPControlPoint *cp);
void browseMediaServer (UPnPRuntime *rt, UPnPControlPoint *cp, char* id, int is_container);
void searchFileOnServer (UPnPRuntime *rt, UPnPControlPoint *cp);
void renderMediaContent(UPnPRuntime *rt, UPnPControlPoint *cp, char*url);
void playMediaFile(UPnPRuntime *rt, UPnPControlPoint *cp, char*url);
void searchCapabilitiesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp);
void sortCapabilitiesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp);
void sendMediaContent (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectURL);
void rendererPlayContent (UPnPRuntime *rt, UPnPControlPoint *cp);
void rendererStopContent (UPnPRuntime *rt, UPnPControlPoint *cp);

static int selectDeviceOfType(UPnPControlPoint* cp, UPNP_CHAR* deviceType, UPNP_INT32 timeoutMsec, void* userData,UPNP_BOOL waitForCompletion);

/*****************************************************************************/
/* Data
 *****************************************************************************/
char global_dirName[256];
char global_dirID[256];
char global_fileURI[256];
char global_ServerURI[256];
char global_RendererURI[256];
char global_fileName[128];
char global_fileID[256];



void *global_list_box;
static char *getSelectedServer(void);
static char *getSelectedRenderer(void);

static void dmc_demo_set_defaults(void);
static void dmc_demo_check_online(void);

#define DMC_DEMO_SELECT_SERVER		0
#define DMC_DEMO_SELECT_RENDERER    1
#define DMC_DEMO_BROWSE_CONTAINER   2
#define DMC_DEMO_BROWSE_ITEM        3
#define DMC_DEMO_SELECT_FILE		4
#define DMC_DEMO_PLAY_FILE			5
#define DMC_DEMO_SHOW_SEARCH_CAPS	6
#define DMC_DEMO_SHOW_SORT_CAPS		7
#define DMC_DEMO_SEND_MEDIA		    8
#define DMC_DEMO_PLAY               9
#define DMC_DEMO_STOP			   10
#define DMC_DEMO_QUIT			   11 /* Must be last */
#define DMC_DEMO_NCOMMANDS          DMC_DEMO_QUIT+1




static char *_cpdemo_ui_list_or_prompt(char *title, char *prompt, char *choices[], char *inbuffer);
static void _cpdemo_ui_report_error(char *alert, struct actionUserData *puserData);
static void _cpdemo_ui_report_deviceinfo(char *alert, UPnPControlDeviceInfo *pinfo);
static void _cpdemo_ui_help(void);
static int _cpdemo_ui_main_menu(void);
static char *_cpdemo_safe_rtp_strdup(char *v)
{
	if (v)
		return(rtp_strdup(v));
	return(rtp_strdup("_cpdemo_safe_rtp_strdup passed null"));
}

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/
/*---------------------------------------------------------------------------*/
int main (int* argc, char* agrv[])
{

}
int media_controller_demo(void)
{
	int result;
	int done = 0;
	int command;
	UPnPRuntime rt;
	UPnPControlPoint cp;

    rtp_net_init();
	rtp_threads_init();



	result = UPnP_RuntimeInit (
			&rt,
			0,                // serverAddr: IP_ANY_ADDR
			0,                // serverPort: any port
            RTP_NET_TYPE_IPV4,// ip version: ipv4
            0,                // web server root dir
			10,               // maxConnections
			5                 // maxHelperThreads
		);


	if (result >= 0)
	{
		_cpdemo_ui_help();

		/* set default values for media type, directory etc */
		dmc_demo_set_defaults();
		/* Initialize the control point */
        if (UPnP_ControlPointInit(&cp, &rt, controlPointCallback, 0) >= 0)
		{
           	while(!done)
			{
            	dmc_demo_check_online();
				command = _cpdemo_ui_main_menu();
				if (command < 0)
					return(-1);
                switch (command)
                {
                    case DMC_DEMO_SELECT_SERVER:
                        openMediaDevice (&rt, &cp, global_ServerURI, MEDIA_SERVER_DEVTYPE);
                        break;
                    case DMC_DEMO_BROWSE_CONTAINER: //Browse Mode
                        browseMediaServer (&rt, &cp, global_dirID, 1);
                        break;
                    case DMC_DEMO_BROWSE_ITEM: //Browse Mode
                        browseMediaServer (&rt, &cp, global_fileID, 0);
                        break;
                    case DMC_DEMO_PLAY_FILE: //Browse Mode
						if (global_fileName[0])
						{
                        	playMediaFile(&rt, &cp, global_fileURI);
						}
                        break;
                    case DMC_DEMO_SHOW_SEARCH_CAPS: //Browse Mode
                    	searchCapabilitiesOnServer (&rt, &cp);
                        break;
                    case DMC_DEMO_SHOW_SORT_CAPS: //Browse Mode
                    	sortCapabilitiesOnServer (&rt, &cp);
                        break;
                    case DMC_DEMO_SELECT_FILE:
                        searchFileOnServer (&rt, &cp);
                        break;
                    case DMC_DEMO_SELECT_RENDERER:
                        openMediaDevice (&rt, &cp, global_RendererURI, MEDIA_RENDERER_DEVTYPE);
                        break;
                    case DMC_DEMO_SEND_MEDIA:
                        sendMediaContent (&rt, &cp, global_fileURI);
                        break;
                    case DMC_DEMO_PLAY:
                        rendererPlayContent (&rt, &cp);
                        break;
                    case DMC_DEMO_STOP:
                        rendererStopContent (&rt, &cp);
						break;
                    case DMC_DEMO_QUIT:
                        done = 1;
                        break;
                    default :
                        break;
                }
           }
 		   UPnP_ControlPointDestroy (&cp, 0);
		}
		UPnP_RuntimeDestroy(&rt);
	}
    rtp_threads_shutdown();
	rtp_net_exit();
	return(0);
}

static void dmc_demo_set_defaults(void)
{
    rtp_strcpy(global_dirID, "0");
    rtp_strcpy(global_dirName, "root");
    rtp_strcpy(global_fileURI, "0");
    rtp_strcpy(global_fileID, "0");
    global_ServerURI[0] = 0;
    global_RendererURI[0] = 0;
    global_fileName[0] = 0;
}

static void dmc_demo_check_online(void)
{
    if (!global_ServerURI[0])
    	rtp_gui_conout("No Media Server Selected");
	else
    	rtp_gui_conout_two("Server:",global_ServerURI);
    if (!global_RendererURI[0])
    	rtp_gui_conout("No Media Renderer Selected");
	else
    	rtp_gui_conout_two("Renderer:",global_RendererURI);
   	if (!global_fileName[0])
    	rtp_gui_conout("No Media File Selected");
	else
	{
    	rtp_gui_conout_two("Media File:",global_fileName);
    	rtp_gui_conout_two("      URI :",global_fileURI);
	}
}


/*---------------------------------------------------------------------------*/
void rendererStopContent (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));


    /* Search for a Media Renderer by its type */
	deviceUrl = getSelectedRenderer();
	if (!deviceUrl)
		return;

    rtp_gui_conout_two("Connecting to Renderer", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		rtp_gui_conout("Device Open Successful. Searching for AVTransport Service");
		/* find the AVTransport:1 service and invoke an action on it */
		if (UPnP_ControlGetServicesByType(dh, &i, "AVTransport:1") >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			if (serviceId)
			{
                RTPXML_Document* action;
                /* Invoking 'Stop' action. This action contains 1 'in'
                   arguments. */
                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"Stop");
                if (action)
				{
					/* In Arguments */
					UPnP_SetActionArg (action, "InstanceID", "0");

					/* NO Out Arguments */
                    userData.numOutElements = 0;

                    if (UPnP_ControlInvokeAction (dh, serviceId, "Stop", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	_cpdemo_ui_report_error("Error occured in SetAVTransportURI request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            rtp_gui_conout("..Stopping...");
                        }
					}
				}
				else
				{
					rtp_gui_conout(" could not create action!");
				}
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
void rendererPlayContent (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Renderer by its Device type */
	deviceUrl = getSelectedRenderer();
	if (!deviceUrl)
		return;

    rtp_gui_conout_two("Connecting to Renderer", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		rtp_gui_conout("device open.\n\nLooking for AVTransport:1 service...");

		/* find the AVTransport:1 service and invoke an action on it */
		if (UPnP_ControlGetServicesByType(dh, &i, "AVTransport:1") >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			if (serviceId)
			{
                RTPXML_Document* action;
                /* Invoking 'play' action. This action contains 2 'in'
                   arguments. */
                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"Play");
                if (action)
				{
					/* In Arguments */
					UPnP_SetActionArg (action, "InstanceID", "0");
					UPnP_SetActionArg (action, "Speed", "1");

					/* NO Out Arguments */
                    userData.numOutElements = 0;

                    if (UPnP_ControlInvokeAction (dh, serviceId, "Play", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	_cpdemo_ui_report_error("Error occured in Play request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            rtp_gui_conout("..Playing...");
                        }
					}
				}
				else
				{
					rtp_gui_conout(" could not create action!");
				}
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
void sendMediaContent (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectURL)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Renderer by its type */
	deviceUrl = getSelectedRenderer();
	if (!deviceUrl)
		return;

    rtp_gui_conout_two("Connecting to Renderer", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		rtp_gui_conout("device open.\n\nLooking for AVTransport:1 service...");

		/* find the AVTransport:1 service and invoke an action on it */
		if (UPnP_ControlGetServicesByType(dh, &i, "AVTransport:1") >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			if (serviceId)
			{
                RTPXML_Document* action;
                /* Invoking 'SetAVTransportURI' action. This action contains 3 'in'
                   arguments. */
                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"SetAVTransportURI");
                if (action)
				{
					/* In Arguments */
					UPnP_SetActionArg (action, "InstanceID", "0");
					UPnP_SetActionArg (action, "CurrentURI", objectURL);
					UPnP_SetActionArg (action, "CurrentURIMetaData", "");

					/* NO Out Arguments */
                    userData.numOutElements = 0;

                    if (UPnP_ControlInvokeAction (dh, serviceId, "SetAVTransportURI", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	_cpdemo_ui_report_error("Error occured in SetAVTransportURI request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
					}
				}
				else
				{
					rtp_gui_conout(" could not create action!");
				}
            }
        }
    }
}


/*---------------------------------------------------------------------------*/
void playMediaContent (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectURL)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Renderer by its type */
	deviceUrl = getSelectedRenderer();
	if (!deviceUrl)
		return;

    rtp_gui_conout_two("Connecting to Renderer", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		rtp_gui_conout("device open.\n\nLooking for AVTransport:1 service...");

		/* find the AVTransport:1 service and invoke an action on it */
		if (UPnP_ControlGetServicesByType(dh, &i, "AVTransport:1") >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			if (serviceId)
			{
                RTPXML_Document* action;
                /* Invoking 'SetAVTransportURI' action. This action contains 3 'in'
                   arguments. */
                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"SetAVTransportURI");
                if (action)
				{
					/* In Arguments */
					UPnP_SetActionArg (action, "InstanceID", "0");
					UPnP_SetActionArg (action, "CurrentURI", objectURL);
					UPnP_SetActionArg (action, "CurrentURIMetaData", "");

					/* NO Out Arguments */
                    userData.numOutElements = 0;

                    if (UPnP_ControlInvokeAction (dh, serviceId, "SetAVTransportURI", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	_cpdemo_ui_report_error("Error occured in SetAVTransportURI request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            int done = 0;
                            while(!done)
                            {
                            	int command;

								command = _cpdemo_ask_render_command();
                                if (command == CPDEMO_PLAY)
                                {
                                    rendererPlayContent(rt, cp);
                                }
                                else if (command == CPDEMO_STOP)
                                {
                                    rendererStopContent(rt, cp);
                                }
                                else if (command == CPDEMO_QUIT)
                                {
                                    done = 1;
                                }
                            }
                        }
					}
				}
				else
				{
					rtp_gui_conout(" could not create action!");
				}
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
void renderMediaContent (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectURL)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Renderer by its type */
	deviceUrl = getSelectedRenderer();
	if (!deviceUrl)
		return;

    rtp_gui_conout_two("Connecting to Renderer", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		rtp_gui_conout("device open.\n\nLooking for AVTransport:1 service...");

		/* find the AVTransport:1 service and invoke an action on it */
		if (UPnP_ControlGetServicesByType(dh, &i, "AVTransport:1") >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			if (serviceId)
			{
                RTPXML_Document* action;
                /* Invoking 'SetAVTransportURI' action. This action contains 3 'in'
                   arguments. */
                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"SetAVTransportURI");
                if (action)
				{
					/* In Arguments */
					UPnP_SetActionArg (action, "InstanceID", "0");
					UPnP_SetActionArg (action, "CurrentURI", objectURL);
					UPnP_SetActionArg (action, "CurrentURIMetaData", "");

					/* NO Out Arguments */
                    userData.numOutElements = 0;

                    if (UPnP_ControlInvokeAction (dh, serviceId, "SetAVTransportURI", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	_cpdemo_ui_report_error("Error occured in SetAVTransportURI request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            int done = 0;
                            while(!done)
                            {
                            	int command;
								command = _cpdemo_ask_render_command();
                                if (command == CPDEMO_PLAY)
                                {
                                    rendererPlayContent(rt, cp);
                                }
                                else if (command == CPDEMO_STOP)
                                {
                                    rendererStopContent(rt, cp);
                                }
                                else if (command == CPDEMO_QUIT)
                                {
                                    done = 1;
                                }
                            }
                        }
					}
				}
				else
				{
					rtp_gui_conout(" could not create action!");
				}
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
void openMediaDevice (UPnPRuntime *rt, UPnPControlPoint *cp, char *deviceUrl, char* deviceType)
{
	UPnPControlDeviceHandle dh;
    rtp_gui_conout("Searching Media Devices...");


    /* Search for a UPnP Device by its type */
    if (!selectDeviceOfType(cp, deviceType, SEARCH_DURATION, deviceUrl, 1))
		return;

    /* open the media server description document */
    rtp_gui_conout_two("Opening device at : ", deviceUrl);
    dh = UPnP_ControlOpenDevice (cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlDeviceInfo info;
		UPnPControlServiceIterator i;

		UPnP_ControlGetDeviceInfo(dh, &info);

		_cpdemo_ui_report_deviceinfo("  success", &info);

		if (UPnP_ControlGetServices(dh, &i) >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			while (serviceId)
			{
				rtp_gui_conout_two ("Found Service ID:", serviceId);
				rtp_gui_conout_two ("            type:", UPnP_ControlGetServiceType(dh, serviceId));
				serviceId = UPnP_ControlNextService(&i);
			}

			UPnP_ControlServiceIteratorDone(&i);
		}

		UPnP_ControlCloseDevice(dh);
	}
	else
	{
		rtp_gui_conout(" open failed.");
	}
}


/*---------------------------------------------------------------------------*/
void browseMediaServer (
        UPnPRuntime *rt,
        UPnPControlPoint *cp,
        char *objectID,
        int is_container
    )
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;
    int  index;
	char *psearchflag;

	if (is_container)
		psearchflag = "BrowseDirectChildren";
	else
		psearchflag = "BrowseMetadata";

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Server by its type */
	deviceUrl = getSelectedServer();
	if (!deviceUrl)
		return;

    rtp_gui_conout_two("\nOpening device at :", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		rtp_gui_conout("device open.\n\nLooking for ContentDirectory:1 service...");

		/* find the ContentDirectory:1 service and invoke an action on it */
		if (UPnP_ControlGetServicesByType(dh, &i, "ContentDirectory:1") >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);
			if (serviceId)
			{
                RTPXML_Document* action;
                /* Invoking 'Browse' action. This action contains 6 'in'
                   arguments. */
                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"Browse");
                if (action)
				{
					/* In Arguments */
					UPnP_SetActionArg (action, "ObjectID", objectID);
					UPnP_SetActionArg (action, "BrowseFlag", psearchflag);
					UPnP_SetActionArg (action, "Filter", "*");
					UPnP_SetActionArg (action, "StartingIndex", "0");
					UPnP_SetActionArg (action, "RequestedCount", BROWSE_ELEMENT_COUNT);
					UPnP_SetActionArg (action, "SortCriteria", "");


					/* Out Arguments */
                    userData.numOutElements = 4;
                    userData.outVariableName[0] = _cpdemo_safe_rtp_strdup("Result");
                    userData.outVariableName[1] = _cpdemo_safe_rtp_strdup("NumberReturned");
                    userData.outVariableName[2] = _cpdemo_safe_rtp_strdup("TotalMatches");
                    userData.outVariableName[3] = _cpdemo_safe_rtp_strdup("UpdateID");

                    if (UPnP_ControlInvokeAction (dh, serviceId, "Browse", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	_cpdemo_ui_report_error("Error occured in Browse request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                        RTPXML_Document *xmlResult;
                            rtp_gui_conout("--------------------------------------");
                            rtp_gui_conout(" Browse Results ");
                            rtp_gui_conout_two("Total Matches: ", userData.outVariableValue[2]);
                            rtp_gui_conout("--------------------------------------");

                            /* load the buffer containing xml document into a dom document */
                            if(rtpxmlParseBufferEx(userData.outVariableValue[0], &xmlResult) == RTPXML_SUCCESS)
                            {
								if (is_container)
                           			parseDirectoryResults(xmlResult);
								else
                            		parseFileResults(xmlResult);
                            }
                        }
					}
                    /* free the xml document */
                    rtpxmlDocument_free (action);
                    for (index = 0; index < userData.numOutElements; index++)
                    {
                        rtp_free(userData.outVariableName[index]);
                        if(userData.outVariableValue[index])
                        {
                            rtp_free(userData.outVariableValue[index]);
                        }
                    }
				}
				else
				{
					rtp_gui_conout (" could not create action!");
				}
            }
        }
    }
}




/*---------------------------------------------------------------------------*/
void playMediaFile(UPnPRuntime *rt, UPnPControlPoint *cp, char*url)
{
    int command =0;
	void *pmenu;

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_MENU, "EBS Media Controller Demo", "Select Option", "Option >");
	if (!pmenu)
		return;

	rtp_gui_list_add_item(pmenu, "Send selected file to renderer", 0, 0, 0);
	rtp_gui_list_add_item(pmenu, "Continue without sending", 1, 0, 0);

	command = rtp_gui_list_execute(pmenu);
	rtp_gui_list_destroy(pmenu);

	if (command == 0)
	{
       rtp_gui_conout("Sending file to renderer....");
       renderMediaContent(rt, cp, url);
	}
}

/*---------------------------------------------------------------------------*/
static int parseDirectoryResults(RTPXML_Document *xmlResult)
{
    RTPXML_NodeList* elementList;
    RTPXML_Element *element;
    int numberOfObjects;
    /* get a list of all nodes containing tag - container */
    elementList = rtpxmlDocument_getElementsByTagName (xmlResult, "container");
    if(elementList)
    {
		void *plist;
		int index;
        /* get the number of containers */
        numberOfObjects = (int) rtpxmlNodeList_length (elementList);
		plist = rtp_gui_list_init(0, RTP_LIST_STYLE_LISTBOX, "EBS Media Controller Demo", "Select Default Directory", "Select >:");

        /* enumerate each object */
        for(index = 0; index < (int)numberOfObjects; index++)
        {
            char *temp;
            /* extract elements from the list */
    	    element = (RTPXML_Element *) rtpxmlNodeList_item(elementList, index);
            /* get the container name */
			if (element)
			{
            	temp = UPnP_DOMGetElemTextInElem (element, "dc:title", 0);
				if (temp)
					rtp_gui_list_add_item(plist, temp, index, 0, 0);
			}
        }
		rtp_gui_list_add_item(plist, "root", numberOfObjects, 0, 0);
		/* Ask the user to select one */
		index = rtp_gui_list_execute(plist);
//		rtp_gui_list_destroy(plist);

		if (index ==  numberOfObjects)
		{
            rtp_strcpy(global_dirID, "0");
		}
		else
		{
            char *temp = 0;
            /* extract elements from the list */
    	    element = (RTPXML_Element *) rtpxmlNodeList_item(elementList, index);

			if (element)
            	temp = rtpxmlElement_getAttribute (element, "id");

			if (temp)
            	rtp_strcpy(global_dirID, temp);
			else
			{
				index =  numberOfObjects;  /* Set to root  */
            	rtp_strcpy(global_dirID, "0");
			}
		}
        rtpxmlNodeList_free(elementList);
		/* Copy the name from the list box and then release it */
      	rtp_strcpy(global_dirName,rtp_gui_list_item_value(plist, index, 0));
		rtp_gui_list_destroy(plist);
      	return(1);

    }

	rtp_gui_conout("No directories found, reverting to root");
   	rtp_strcpy(global_dirID, "0");
  	rtp_strcpy(global_dirName, "root");
    return (0);
}

static int parseFileResults(RTPXML_Document *xmlResult)
{
    RTPXML_NodeList* elementList;
    RTPXML_Element *element;
    int matched = 0;
    {
    int numberOfObjects;
        elementList = rtpxmlDocument_getElementsByTagName (xmlResult, "item");
        if(elementList)
        {
			int index;
			void *plist;

            plist = rtp_gui_list_init(0, RTP_LIST_STYLE_LISTBOX, "EBS Media Controller Demo", "Select A File", "Select >:");
            /* get the number of containers */
            numberOfObjects = (int) rtpxmlNodeList_length (elementList);

            /* enumerate each object */
            for(index = 0; index < (int)numberOfObjects; index++)
            {
                char *temp = 0;

                /* extract elements from the list */
        	    element = (RTPXML_Element *) rtpxmlNodeList_item(elementList, index);
				if (element)
                /* get the container name */
                	temp = UPnP_DOMGetElemTextInElem (element, "dc:title", 0);
				if (temp)
					rtp_gui_list_add_item(plist, temp, index, 0, 0);
            }
            if (numberOfObjects)
			{
				char *temp;
				index = rtp_gui_list_execute(plist);

                rtp_strcpy(global_fileName, rtp_gui_list_item_value(plist, index, 0));
                rtp_gui_list_destroy(plist);

        	    element = (RTPXML_Element *) rtpxmlNodeList_item(elementList, index);
                temp = UPnP_DOMGetElemTextInElem (element, "res", 0);
                rtp_strcpy(global_fileURI, temp);
				temp = rtpxmlElement_getAttribute (element, "id");
				if (temp)
                	rtp_strcpy(global_fileID, temp);
			}
            rtpxmlNodeList_free(elementList);
        }
    }
    return (0);
}


/*---------------------------------------------------------------------------*/
char *parseSearchResult(char *result)
{
    RTPXML_Document *xmlResult;

    /* load the buffer containing xml document into a dom document */
    if(rtpxmlParseBufferEx(result, &xmlResult) != RTPXML_SUCCESS)
	{
	    return(0);
    }
    parseFileResults(xmlResult);
    return(0);
}

static char *global_SearchOptions[] = {
"upnp:class = object.item.audioItem.music",
"upnp:class = object.item.imageItem.photo",
"upnp:class = object.item.videoItem.movie",
"upnp:class derivedfrom object.item.audioItem",
"upnp:class derivedfrom object.item.audioItem",
"upnp:class derivedfrom object.item.imageItem",
"upnp:class derivedfrom object.item.videoItem",
"*",
"^\"\" (empty string)",
"# I want to type in my own search option",
0
};
static char *global_SortOptions[] = {
"+dc:title",
"+dc:artist",
"+dc:date",
"-dc:date",
"^\"\" (empty string)",
"# I want to type in my own sort option",
0
};

/*---------------------------------------------------------------------------*/
void searchFileOnServer (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;
    int index;
    int numReturned;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Server by its type */
	deviceUrl = getSelectedServer();
	if (!deviceUrl)
		return;

    rtp_gui_conout_two("Opening device at :", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		rtp_gui_conout("device open.\n\nLooking for ContentDirectory:1 service...");

		/* find the ContentDirectory:1 service and invoke an action on it */
		if (UPnP_ControlGetServicesByType(dh, &i, "ContentDirectory:1") >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			if (serviceId)
			{
                RTPXML_Document* action;

                /* Out Arguments */
                userData.numOutElements = 4;
                userData.outVariableName[0] = _cpdemo_safe_rtp_strdup("Result");
                userData.outVariableName[1] = _cpdemo_safe_rtp_strdup("NumberReturned");
                userData.outVariableName[2] = _cpdemo_safe_rtp_strdup("TotalMatches");
                userData.outVariableName[3] = _cpdemo_safe_rtp_strdup("UpdateID");

                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"Search");
                if (action)
				{
				char *psearch;
				char *psort;
				char search_buffer[128];
				char sort_buffer[128];

					psearch = _cpdemo_ui_list_or_prompt("Select a search or type in a value", "Select : >", global_SearchOptions, search_buffer);
					psort = _cpdemo_ui_list_or_prompt("Select a sort or type in a value", "Select : >", global_SortOptions, sort_buffer);


					/* In Arguments */
					UPnP_SetActionArg (action, "ContainerID", "0");
					UPnP_SetActionArg (action, "SearchCriteria", psearch); /*   "upnp:class = object.item.audioItem.music" */
					UPnP_SetActionArg (action, "Filter", "*");
					UPnP_SetActionArg (action, "StartingIndex", "0");
					UPnP_SetActionArg (action, "RequestedCount", "8");
					UPnP_SetActionArg (action, "SortCriteria", psort);

                    if (UPnP_ControlInvokeAction (dh, serviceId, "Search", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	_cpdemo_ui_report_error("Error occured in Search request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            numReturned = atoi(userData.outVariableValue[1]);
                            if(numReturned > 0)
                            {
                                char *objectURL;
                                char ch = '0';
                                int done = 0;
                                objectURL = parseSearchResult(userData.outVariableValue[0]);
                                if (objectURL)
                                {
                                    /* file found */
                                    playMediaFile(rt, cp, objectURL);
                                }
                            }
                        }
					}
                    /* free the xml document */
                    rtpxmlDocument_free (action);
                    /* free up userdata struct */
                    for (index = 0; index < userData.numOutElements; index++)
                    {
                        rtp_free(userData.outVariableName[index]);
                        if(userData.outVariableValue[index])
                        {
                            rtp_free(userData.outVariableValue[index]);
                        }
                    }
                }
            }
        }
    }
}


/*---------------------------------------------------------------------------*/
void sortCapabilitiesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;
    int index;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Server by its type */
	deviceUrl = getSelectedServer();
	if (!deviceUrl)
		return;

    rtp_gui_conout_two("Opening device at :", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		rtp_gui_conout("device open.\n\nLooking for ContentDirectory:1 service...");

		/* find the ContentDirectory:1 service and invoke an action on it */
		if (UPnP_ControlGetServicesByType(dh, &i, "ContentDirectory:1") >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			if (serviceId)
			{
                RTPXML_Document* action;

                /* Out Arguments */
                userData.numOutElements = 1;
                userData.outVariableName[0] = _cpdemo_safe_rtp_strdup("SortCaps");

                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"GetSortCapabilities");
                if (action)
				{
					/* In Arguments */

                    if (UPnP_ControlInvokeAction (dh, serviceId, "GetSortCapabilities", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	_cpdemo_ui_report_error("Error occured in Sort request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                        char *mycsv;

                            mycsv = userData.outVariableValue[0];
							if (mycsv)
							{
                            	rtp_gui_conout("--------------------------------------");
                            	rtp_gui_conout(mycsv);
							}
                        }
					}
                    /* free the xml document */
                    rtpxmlDocument_free (action);
                    /* free up userdata struct */
                    for (index = 0; index < userData.numOutElements; index++)
                    {
                        rtp_free(userData.outVariableName[index]);
                        if(userData.outVariableValue[index])
                        {
                            rtp_free(userData.outVariableValue[index]);
                        }
                    }
                }
            }
        }
    }
}
/*---------------------------------------------------------------------------*/
void searchCapabilitiesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;
    int index;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Server by its type */
	deviceUrl = getSelectedServer();
	if (!deviceUrl)
		return;

    rtp_gui_conout_two("Opening device at :", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		rtp_gui_conout("device open.\n\nLooking for ContentDirectory:1 service...");

		/* find the ContentDirectory:1 service and invoke an action on it */
		if (UPnP_ControlGetServicesByType(dh, &i, "ContentDirectory:1") >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			if (serviceId)
			{
                RTPXML_Document* action;

                /* Out Arguments */
                userData.numOutElements = 1;
                userData.outVariableName[0] = _cpdemo_safe_rtp_strdup("SearchCaps");

                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"GetSearchCapabilities");
                if (action)
				{
					/* In Arguments */

                    if (UPnP_ControlInvokeAction (dh, serviceId, "GetSearchCapabilities", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	_cpdemo_ui_report_error("Error occured in Search request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                        char *mycsv;

                            mycsv = userData.outVariableValue[0];
							if (mycsv)
							{
                            	rtp_gui_conout("--------------------------------------");
                            	rtp_gui_conout(mycsv);
							}
                        }
					}
                    /* free the xml document */
                    rtpxmlDocument_free (action);
                    /* free up userdata struct */
                    for (index = 0; index < userData.numOutElements; index++)
                    {
                        rtp_free(userData.outVariableName[index]);
                        if(userData.outVariableValue[index])
                        {
                            rtp_free(userData.outVariableValue[index]);
                        }
                    }
                }
            }
        }
    }
}

static char *getSelectedServer(void)
{
	if (global_ServerURI[0])
		return(&global_ServerURI[0]);
	rtp_gui_conout("No Media server is selected");
	return(0);
}

static char *getSelectedRenderer(void)
{
	if (global_RendererURI[0])
		return(&global_RendererURI[0]);
	rtp_gui_conout("No Media Renderer is selected");
	return(0);
}


static int selectDeviceOfType(UPnPControlPoint* cp, UPNP_CHAR* deviceType, UPNP_INT32 timeoutMsec, void* userData,UPNP_BOOL waitForCompletion)
{
	char *dev_url;

    global_list_box = rtp_gui_list_init(0, RTP_LIST_STYLE_LISTBOX, "EBS Media Controller Demo", "Select A Device", "Select >:");

	dev_url = (char *) userData;
	UPnP_ControlFindDevicesByType (cp, deviceType, timeoutMsec, userData, waitForCompletion);
	if (!rtp_gui_list_item_value(global_list_box, 0,0))
	{
        rtp_gui_list_destroy(global_list_box);
		global_list_box = 0;
		rtp_gui_conout_two("No devices found of type :", deviceType);
		*dev_url = 0;
		return(0);
	}
	else
	{
	int index;
        index = rtp_gui_list_execute(global_list_box);
		rtp_strcpy(dev_url, rtp_gui_list_item_value(global_list_box, index,0));
        rtp_gui_list_destroy(global_list_box);
		global_list_box = 0;
		return(1);
	}
}
/*---------------------------------------------------------------------------*/
int controlPointCallback (
		UPnPControlPoint* cp,
		UPnPControlEvent* event,
		void *perControl,
		void *perRequest
	)
{
	int result = 0;
	switch (event->type)
	{
		case UPNP_CONTROL_EVENT_DEVICE_FOUND:
		{
            char* str = (char*) perRequest;
            rtp_gui_conout_two(" Device found at: ", (char *)event->data.discover.url);
            rtp_gui_conout_two("         Service: ", (char *)event->data.discover.type);
            rtp_gui_conout("--------------------------------------------------");

            rtp_sprintf(str, "%s", event->data.discover.url); /* Why are we doing this ? */

			if (global_list_box && event->data.discover.url)
				rtp_gui_list_add_item(global_list_box , (char *)event->data.discover.url, rtp_gui_list_size(global_list_box), 0, 0);
			result = 0; // keep searching
			break;
		}

		case UPNP_CONTROL_EVENT_SEARCH_COMPLETE:
		{
            char* str = (char*) perRequest;
            rtp_gui_conout_two ("     Search Complete event for :", str);
			result = 1; // perform no furthur searches
			result = 0; // keep searching
			break;
		}

		case UPNP_CONTROL_EVENT_DEVICE_ALIVE:
		{
			rtp_gui_conout_two("Device alive rcvd :",(char *)event->data.discover.url);
			rtp_gui_conout_two("               ST :",(char *)event->data.discover.type);
			rtp_gui_conout_two("              USN :",(char *)event->data.discover.usn);
			break;
		}

		case UPNP_CONTROL_EVENT_DEVICE_BYE_BYE:
		{
			rtp_gui_conout("Device bye-bye rcved");
			rtp_gui_conout_two("             ST :",(char *)event->data.discover.type);
			rtp_gui_conout_two("            USN :",(char *)event->data.discover.usn);
			break;
		}

		case UPNP_CONTROL_EVENT_ACTION_COMPLETE:
		{
			struct actionUserData *userData = (struct actionUserData*)perRequest;
			int i;
			const char *outValue;
            if(event->data.action.success == 0)
            {
                userData->errorCode = event->data.action.errorCode;
                rtp_gui_conout_two("Action cannot be executed desc:", event->data.action.errorDescription);
                userData->errorDescription = _cpdemo_safe_rtp_strdup(event->data.action.errorDescription);
            }
            else
            {
                for(i = 0; i < userData->numOutElements; i++)
                {
                    outValue = UPnP_GetPropertyValueByName (
                                    event->data.action.response,
                                    userData->outVariableName[i]);
                    if(outValue)
                    {
                        userData->outVariableValue[i] = _cpdemo_safe_rtp_strdup((char *)outValue);
                    }
                    else
                    {
                        userData->outVariableValue[i] = 0;
                    }
                }

            }
            userData->done = 1;
			break;
		}

		case UPNP_CONTROL_EVENT_SERVICE_GET_INFO_FAILED:
		{
			int* done = (int*) perRequest;
			rtp_gui_conout("UPnP_ControlGetServiceInfoAsync failed!");

            if(done)
            {
			    *done = 1;
            }
			break;
		}

		case UPNP_CONTROL_EVENT_SERVICE_INFO_READ:
		{
			int* done = (int*) perRequest;
			DOMString str = rtpxmlPrintDocument(event->data.service.scpdDoc);

			if (str)
			{
				rtp_gui_conout_two("service info read:", str);
				rtpxmlFreeDOMString(str);
			}
			rtpxmlDocument_free(event->data.service.scpdDoc);
            if(done)
            {
			    *done = 1;
            }
			break;
		}

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_ACCEPTED:
			rtp_gui_conout_two("subscription accepted id is:", (char *)event->data.subscription.serviceId);
			break;

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_CANCELLED:
            rtp_gui_conout("service Unsubscribed");
			break;

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_REJECTED:
			rtp_gui_conout("subscription rejected");
			break;

		case UPNP_CONTROL_EVENT_SERVICE_STATE_UPDATE:
		{
			const UPNP_CHAR* str = UPnP_GetPropertyValue(event->data.notify.stateVars, "LoadLevelStatus");
			if (!str)
			{
				str = "[unknown]";
			}
			rtp_gui_conout_two("Notification: Load level is :", (char *)str);
			break;
		}

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_NEAR_EXPIRATION:
			rtp_gui_conout ("Subscription Expiration Warning!");
			break;

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_EXPIRED:
			rtp_gui_conout ("Subscription Expired.");
			break;

        case UPNP_CONTROL_EVENT_DEVICE_OPEN:
        {
            int* done = (int*) perRequest;

            if(event->data.device.handle)
	        {
		        UPnPControlDeviceInfo info;
		        UPnPControlServiceIterator i;

		        UPnP_ControlGetDeviceInfo(event->data.device.handle, &info);

		        _cpdemo_ui_report_deviceinfo("  success", &info);

		        if (UPnP_ControlGetServices(event->data.device.handle, &i) >= 0)
		        {
			        UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			        while (serviceId)
			        {
			        	rtp_gui_conout_two("Found Service:", serviceId);
			        	rtp_gui_conout_two("        Type :",
			        		UPnP_ControlGetServiceType(event->data.device.handle, serviceId));
				        serviceId = UPnP_ControlNextService(&i);
			        }

			        UPnP_ControlServiceIteratorDone(&i);
		        }

		        UPnP_ControlCloseDevice(event->data.device.handle);
	        }
            if(done)
            {
			    *done = 1;
            }

            break;
        }
        case UPNP_CONTROL_EVENT_DEVICE_OPEN_FAILED:
        {
            int* done = (int*) perRequest;
            rtp_gui_conout("Device Open Failed");
            if(done)
            {
			    *done = 1;
            }

            break;
        }
	}

	return (result);
}



#define DMC_DEMO_PROMPT_QUIT			 "Quit"
#define DMC_DEMO_PROMPT_SELECT_SERVER	 "Select a Media Server (UPnP_ControlOpenDevice (MediaServer:1)...."
#define DMC_DEMO_PROMPT_SELECT_RENDERER  "Select a Media Renderer (UPnP_ControlOpenDevice (MediaRenderer:1)...."
#define DMC_DEMO_PROMPT_BROWSE_CONTAINER "Browse Selected Container (Browse id=current_container BrowseFlag=BrowseDirectChildren)"
#define DMC_DEMO_PROMPT_BROWSE_ITEM      "Browse Selected Item (Browse id=current_item BrowseFlag=BrowseMetadata)"
#define DMC_DEMO_PROMPT_SELECT_FILE		 "Select a File (Search ContainerID=\"\",SearchCriteria=\"\",Filter=\"\",...)"
#define DMC_DEMO_PROMPT_PLAY_FILE		 "Play Selected File (SetAVTransportURI, Stop, Play)"
#define DMC_DEMO_PROMPT_SHOW_SEARCH_CAPS "Show search capabilities list (GetSearchCapabilities)"
#define DMC_DEMO_PROMPT_SHOW_SORT_CAPS   "Show sort capabilities list (GetSortCapabilities)"
#define DMC_DEMO_PROMPT_SEND_MEDIA       "Queue File for Renderer (SetAVTransportURI)"
#define DMC_DEMO_PROMPT_PLAY             "Play queued file on the renderer (Play)"
#define DMC_DEMO_PROMPT_STOP             "Stop file playing on the renderer (Stop)"

static int _cpdemo_ui_main_menu(void)
{
void *pmenu;
int   command, greyed_list[DMC_DEMO_NCOMMANDS];

	dmc_demo_check_online();

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_MENU, "EBS Media Controller Demo", "Main Menu", "Command >");
	if (!pmenu)
		return(-1);
	rtp_memset(&greyed_list[0], 0, sizeof(greyed_list));

   greyed_list[DMC_DEMO_SELECT_SERVER]    = 0;
   greyed_list[DMC_DEMO_SELECT_RENDERER]  = 0;
   if (!global_ServerURI[0])
   {
   greyed_list[DMC_DEMO_BROWSE_CONTAINER] = 1;
   greyed_list[DMC_DEMO_BROWSE_ITEM]      = 1;
   greyed_list[DMC_DEMO_SELECT_FILE]      = 1;
   }
   if (!global_ServerURI[0] || !global_RendererURI[0])
   {
   	greyed_list[DMC_DEMO_PLAY_FILE]        = 1;
   }
   if (!global_ServerURI[0] || !global_RendererURI[0] || !global_fileName[0])
   {
   	greyed_list[DMC_DEMO_SEND_MEDIA]       = 1;
   	greyed_list[DMC_DEMO_PLAY]             = 1;
   	greyed_list[DMC_DEMO_STOP]             = 1;
   }
   if (!global_ServerURI[0])
   {
   	greyed_list[DMC_DEMO_SHOW_SEARCH_CAPS] = 1;
   	greyed_list[DMC_DEMO_SHOW_SORT_CAPS]   = 1;
   }
   greyed_list[DMC_DEMO_QUIT]             = 0;


   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_SELECT_SERVER,DMC_DEMO_SELECT_SERVER, 0, greyed_list[DMC_DEMO_SELECT_SERVER]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_SELECT_RENDERER,DMC_DEMO_SELECT_RENDERER, 0, greyed_list[DMC_DEMO_SELECT_RENDERER]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_BROWSE_CONTAINER,DMC_DEMO_BROWSE_CONTAINER, 0, greyed_list[DMC_DEMO_BROWSE_CONTAINER]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_BROWSE_ITEM,DMC_DEMO_BROWSE_ITEM, 0, greyed_list[DMC_DEMO_BROWSE_ITEM]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_SELECT_FILE,DMC_DEMO_SELECT_FILE, 0, greyed_list[DMC_DEMO_SELECT_FILE]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_PLAY_FILE,DMC_DEMO_PLAY_FILE, 0, greyed_list[DMC_DEMO_PLAY_FILE]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_SEND_MEDIA,DMC_DEMO_SEND_MEDIA, 0, greyed_list[DMC_DEMO_SEND_MEDIA]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_PLAY,DMC_DEMO_PLAY, 0, greyed_list[DMC_DEMO_PLAY]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_STOP,DMC_DEMO_STOP, 0, greyed_list[DMC_DEMO_STOP]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_SHOW_SEARCH_CAPS,DMC_DEMO_SHOW_SEARCH_CAPS, 0, greyed_list[DMC_DEMO_SHOW_SEARCH_CAPS]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_SHOW_SORT_CAPS,DMC_DEMO_SHOW_SORT_CAPS, 0, greyed_list[DMC_DEMO_SHOW_SORT_CAPS]);
   rtp_gui_list_add_item(pmenu, DMC_DEMO_PROMPT_QUIT,DMC_DEMO_QUIT, 0, greyed_list[DMC_DEMO_QUIT]);

   command = rtp_gui_list_execute(pmenu);
   rtp_gui_list_destroy(pmenu);
   return(command);
}

static int _cpdemo_ask_render_command(void)
{
	void *pmenu;
	int command =0;

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_MENU, "EBS Media Controller Demo", "Select Command", "Command >");
	if (!pmenu)
		return(0);

	rtp_gui_list_add_item(pmenu, "Play", CPDEMO_PLAY, 0, 0);
	rtp_gui_list_add_item(pmenu, "Stop", CPDEMO_STOP, 0, 0);
	rtp_gui_list_add_item(pmenu, "Quit", CPDEMO_QUIT, 0, 0);

	command = rtp_gui_list_execute(pmenu);
	rtp_gui_list_destroy(pmenu);
	return(command);
}

static char *_cpdemo_ui_list_or_prompt(char *title, char *prompt, char *choices[], char *inbuffer)
{
void *pmenu;
int n_choices = 0;
int index;

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_MENU, "EBS Media Controller Demo", title, prompt);
	if (!pmenu)
		return(0);
	while(choices[n_choices])
	{
		rtp_gui_list_add_item(pmenu, choices[n_choices], n_choices, 0, 0);
		n_choices += 1;
	}
	index = rtp_gui_list_execute(pmenu);
	rtp_gui_list_destroy(pmenu);
	if (index == n_choices-1)
	{

		rtp_gui_prompt_text("     ", "Enter manually: ", inbuffer);
   		return(inbuffer);
	}
	else if (*choices[index] == '^')
		return("");
	else
		return(choices[index]);
}
/* Display help in a list box */

static char *global_Help[] = {
 "                Follow these instructions for getting started",
 "                ============================================",
 " ",
 ". First select a media server, most commands are disabled until a ",
 "  server is selected.",
 ". Next, you may want to select a media renderer.",
 " ",
 ". Next you should Browse Selected Containers to select a directory. ",
 " ",
 ". Next you should use Search to select a file. ",
 "    Note: The search command allows you to specify search and sort ",
 "   criteria. You may use this to test passing illegal arguments.",
 "    Note: Not all search and sort strings are accepted by all servers",
 "    \"*\" for search and \"\" for sort are universally legal",
 " ",
 ". Next you can use the Queue, Play and Stop to render your selection.",
 "     Note: GetSearchCapabilities and GetSortCapabilities are included ",
 "     for completeness.",
 " ",
 0,
 };


static void _cpdemo_ui_help(void)
{
	int i;
	void *p;
	p = rtp_gui_list_init(0, RTP_LIST_STYLE_HELP, "EBS Media Controller Demo", "Instructions", "");
	if (!p)
		return;
	for (i = 0; global_Help[i]; i++)
		rtp_gui_list_add_item(p, global_Help[i], 0, 0, 1);
	rtp_gui_list_execute(p);
	rtp_gui_list_destroy(p);
}

static void _cpdemo_ui_report_error(char *alert, struct actionUserData *puserData)
{
char str[256];
	rtp_sprintf(str, "%s : Code == %d, Desc == %s\n", puserData->errorCode, puserData->errorDescription);
	rtp_gui_conout(str);
}



static void _cpdemo_ui_report_deviceinfo(char *alert, UPnPControlDeviceInfo *pinfo)
{
		rtp_gui_conout(alert);

        rtp_gui_conout_two("  deviceType      :",	pinfo->deviceType);
		rtp_gui_conout_two("  friendlyName    :",	pinfo->friendlyName);
		rtp_gui_conout_two("  manufacturer    :",	pinfo->manufacturer);
		rtp_gui_conout_two("  manufacturerURL :",	pinfo->manufacturerURL);
		rtp_gui_conout_two("  modelDescription:",	pinfo->modelDescription);
		rtp_gui_conout_two("  modelName       :",	pinfo->modelName);
		rtp_gui_conout_two("  modelNumber     :",	pinfo->modelNumber);
		rtp_gui_conout_two("  modelURL        :",	pinfo->modelURL);
		rtp_gui_conout_two("  serialNumber    :",	pinfo->serialNumber);
		rtp_gui_conout_two("  UDN             :",	pinfo->UDN);
		rtp_gui_conout_two("  UPC             :",	pinfo->UPC);
		rtp_gui_conout_two("  presentationURL :",	pinfo->presentationURL);
}
