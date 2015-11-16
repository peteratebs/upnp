/*
|  media_controller_cpdemo.c
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

#include "rtpnet.h"
#include "rtpscnv.h"
#include "ssdpcli.h"
#include "upnpcli.h"
#include "rtptime.h"
#include "rtpthrd.h"
#include "upnpdom.h"
#include "rtpgui.h"
#include "rtplog.h"
#include "rtpprint.h"

/*****************************************************************************/
/* Macros
 *****************************************************************************/
#define MEDIA_RENDERER_UDN          "019297f7-9a9e-4c3a-b419-7e9cb6c124ba"
#define MEDIA_SERVER_UDN            "8d3ccfa4-7349-11db-830a-00107500015c"
#define SEARCH_DURATION             2000
#define BROWSE_ELEMENT_COUNT        100
/* Not to be changed */
#define MEDIA_SERVER_DEVTYPE        "MediaServer:1"
#define MEDIA_RENDERER_DEVTYPE      "MediaRenderer:1"


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

static int dmc_demo_controlPointCallback (
		UPnPControlPoint* cp,
		UPnPControlEvent* event,
		void *perControl,
		void *perRequest
	);

struct dmc_demo_directory
{
	struct dmc_demo_directory *pnext;
	char * directory_title;
	char * directory_id;
	struct dmc_demo_directory *psubdirectory_list;
	struct dmc_demo_file *pfile_list;
};

struct dmc_demo_file
{
	struct dmc_demo_file *pnext;
	char * file_title;
	char * file_uri;
	char * file_id;
};

/* Main menu items */
#define DMC_DEMO_SELECT_SERVER		0
#define DMC_DEMO_SELECT_RENDERER    1
#define DMC_DEMO_BROWSE_CONTAINER   2
#define DMC_DEMO_TREEWALK        	3
#define DMC_DEMO_SELECT_FILE		4
#define DMC_DEMO_PLAY_FILE			5
#define DMC_DEMO_SHOW_SEARCH_CAPS	6
#define DMC_DEMO_SHOW_SORT_CAPS		7
#define DMC_DEMO_SEND_MEDIA		    8
#define DMC_DEMO_PLAY               9
#define DMC_DEMO_STOP			   10
#define DMC_DEMO_SHOW_SELECTIONS   11
#define DMC_DEMO_HELP			   12
#define DMC_DEMO_SELECT_ROOT	   13
#define DMC_DEMO_QUIT			   14 /* Must be last */
#define DMC_DEMO_NCOMMANDS         DMC_DEMO_QUIT+1


/* Render subcommands */
#define DMC_DEMO_RENDER_PLAY 0
#define DMC_DEMO_RENDER_STOP 1
#define DMC_DEMO_RENDER_QUIT 2

static int dmc_demo_parseDirectoryResults(RTPXML_Document *xmlResult,struct dmc_demo_directory **pdir_results);
static int dmc_demo_parseFileResults(RTPXML_Document *xmlResult,struct dmc_demo_file **pfile_results);
static int dmc_demo_countDirectoryResults(RTPXML_Document *xmlResult);
static int dmc_demo_countFileResults(RTPXML_Document *xmlResult);
static int dmc_demo_command_countFilesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectID);
static int dmc_demo_command_countDirectoriesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectID);
static int dmc_demo_ui_ask_render_command(void);
static void dmc_demo_command_openMediaDevice (UPnPRuntime *rt, UPnPControlPoint *cp, char *deviceUrl, char* deviceType);
static void dmc_demo_command_browse (UPnPRuntime *rt, UPnPControlPoint *cp);
struct dmc_demo_directory *dmc_demo_command_browseMediaServer (UPnPRuntime *rt, UPnPControlPoint *cp, char* id, int is_container);
static void dmc_demo_treewalk ( UPnPRuntime *rt,   UPnPControlPoint *cp );
static struct dmc_demo_file * dmc_demo_command_searchFileOnServer (UPnPRuntime *rt, UPnPControlPoint *cp, char *psearch, char *psort, char *id);
static void dmc_demo_command_renderMediaContent(UPnPRuntime *rt, UPnPControlPoint *cp, char*url);
static void dmc_demo_command_playMediaFile(UPnPRuntime *rt, UPnPControlPoint *cp, char*url);
static void dmc_demo_command_searchCapabilitiesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp);
static void dmc_demo_command_sortCapabilitiesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp);
static void dmc_demo_command_sendMediaContent (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectURL);
static void dmc_demo_command_rendererPlayContent (UPnPRuntime *rt, UPnPControlPoint *cp);
static void dmc_demo_command_rendererStopContent (UPnPRuntime *rt, UPnPControlPoint *cp);
static void dmc_demo_set_defaults(void);
static void dmc_demo_show_selections(void);
static int dmc_demo_selectDeviceOfType(UPnPControlPoint* cp, UPNP_CHAR* deviceType, UPNP_INT32 timeoutMsec, void* userData,UPNP_BOOL waitForCompletion);
static void dmc_demo_select_directory(struct dmc_demo_directory *pdir_results);
static void dmc_demo_select_file(struct dmc_demo_file *pdir_results);
static void dmc_demo_freeif(char *p);

static char *dmc_demo_ui_list_or_prompt(char *title, char *prompt, char *choices[], char *inbuffer);
static void dmc_demo_ui_report_error(char *alert, struct actionUserData *puserData);
static void dmc_demo_ui_report_deviceinfo(char *alert, UPnPControlDeviceInfo *pinfo);
static void dmc_demo_help(void);
static int dmc_demo_ui_main_menu(void);
static void dmc_demo_ui_listout_two(void *pmenu,char *item1, char *item2);
static char *dmc_demo_ui_safe_rtp_strdup(char *v);
static void dmc_demo_free_directory_list(struct dmc_demo_directory *presult);
static void dmc_demo_free_file_list(struct dmc_demo_file *presult);

static int dmc_demo_UPnP_ControlInvokeAction (
		UPnPControlDeviceHandle deviceHandle,
		UPNP_CHAR* serviceId,
		const UPNP_CHAR* actionName,
		RTPXML_Document* action,
		void* userData,
		UPNP_BOOL waitForCompletion
	);

/*****************************************************************************/
/* Data
 *****************************************************************************/
static char global_dirName[256];
static char global_dirID[256];
static char global_fileURI[256];
static char global_ServerURI[256];
static char global_RendererURI[256];
static char global_fileName[128];
static char global_fileID[256];
static void *global_list_box;
static char *dmc_demo_getSelectedServer(void);
static char *dmc_demo_getSelectedRenderer(void);
static struct dmc_demo_directory *global_directoryTree;


/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/
/*---------------------------------------------------------------------------*/
/* target independent - Main entry point for the media contoller application. */
int media_controller_demo(void)
{
	int result;
	int done = 0;
	int command;
	unsigned char ipAddr[RTP_NET_NI_MAXHOST] = {192, 168, 1, 2};

	UPnPRuntime rt;
	UPnPControlPoint cp;

    rtp_net_init();
	rtp_threads_init();

#ifdef RTP_ENABLE_LOGGING
{

char inbuffer[32];

	rtp_gui_conout("Open a log file if you want to watch the data flow.");
	rtp_gui_conout("If logging is enabled UPnP traffic to and from the");
	rtp_gui_conout("application are logged, with some status messages ");
	rtp_gui_conout("injected in the text. The traffic and messages ");
	rtp_gui_conout("may be viewed with a text editor..");
	rtp_gui_conout("  ");
	rtp_gui_prompt_text("    ", "Type Y or y to enable logging traffic and events", inbuffer);

	if (inbuffer[0] == 'y' || inbuffer[0] == 'Y')
	{
		RTP_LOG_OPEN()
	}
}
#endif

	result = UPnP_RuntimeInit (
			&rt,
			ipAddr,          // serverAddr: IP_ANY_ADDR
			0,                // serverPort: any port
            RTP_NET_TYPE_IPV4,// ip version: ipv4
            0,                // web server root dir
			10,               // maxConnections
			5                 // maxHelperThreads
		);


	if (result >= 0)
	{
		/* set default values for media type, directory etc */
		dmc_demo_set_defaults();
		/* Initialize the control point */
        if (UPnP_ControlPointInit(&cp, &rt, dmc_demo_controlPointCallback, 0) >= 0)
		{
           	while(!done)
			{
				command = dmc_demo_ui_main_menu();
				if (command < 0)
					return(-1);
                switch (command)
                {
                    case DMC_DEMO_SHOW_SELECTIONS:
                    	dmc_demo_show_selections();
                        break;
                    case DMC_DEMO_HELP:
                    	dmc_demo_help();
						break;
                    case DMC_DEMO_SELECT_ROOT:
                    	rtp_strcpy(global_dirID, "0");
                    	rtp_strcpy(global_dirName, "root");
						break;
                    case DMC_DEMO_SELECT_SERVER:
                        dmc_demo_command_openMediaDevice (&rt, &cp, global_ServerURI, MEDIA_SERVER_DEVTYPE);
                        break;
                    case DMC_DEMO_BROWSE_CONTAINER: //Browse Mode
                    	dmc_demo_command_browse (&rt, &cp);
                        break;
                    case DMC_DEMO_TREEWALK: //Browse Mode
                    {
                    	dmc_demo_treewalk ( &rt,   &cp );
                    }
                    break;
                    case DMC_DEMO_PLAY_FILE: //Browse Mode
						if (global_fileName[0])
						{
                        	dmc_demo_command_playMediaFile(&rt, &cp, global_fileURI);
						}
                        break;
                    case DMC_DEMO_SHOW_SEARCH_CAPS: //Browse Mode
                    	dmc_demo_command_searchCapabilitiesOnServer (&rt, &cp);
                        break;
                    case DMC_DEMO_SHOW_SORT_CAPS: //Browse Mode
                    	dmc_demo_command_sortCapabilitiesOnServer (&rt, &cp);
                        break;
                    case DMC_DEMO_SELECT_FILE:
					{
						struct dmc_demo_file *pfile_results;

                        pfile_results = dmc_demo_command_searchFileOnServer (&rt, &cp, 0, 0,global_dirID);
						if (pfile_results)
						{
							dmc_demo_select_file(pfile_results);
							if (global_fileURI[0])
							{
							/* file found */
                        		dmc_demo_command_sendMediaContent (&rt, &cp, global_fileURI);
//								dmc_demo_command_playMediaFile(rt, cp, global_fileURI);
							}
						}
					}
                        break;
                    case DMC_DEMO_SELECT_RENDERER:
                        dmc_demo_command_openMediaDevice (&rt, &cp, global_RendererURI, MEDIA_RENDERER_DEVTYPE);
                        break;
                    case DMC_DEMO_SEND_MEDIA:
                        dmc_demo_command_sendMediaContent (&rt, &cp, global_fileURI);
                        break;
                    case DMC_DEMO_PLAY:
                        dmc_demo_command_rendererPlayContent (&rt, &cp);
                        break;
                    case DMC_DEMO_STOP:
                        dmc_demo_command_rendererStopContent (&rt, &cp);
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
	RTP_LOG_CLOSE()
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
    global_directoryTree = 0;

}

static void dmc_demo_show_selections(void)
{
void *pmenu;

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_LISTBOX|RTP_LIST_STYLE_NOFORMAT, "EBS Media Controller Demo", "Current Selections", "Press Return >");
	if (!pmenu)
		return;
    if (!global_ServerURI[0])
		rtp_gui_list_add_int_item(pmenu, "No Media Server Selected", 0, 1, 1,0);
	else
	{
    	dmc_demo_ui_listout_two(pmenu, "Server :", global_ServerURI);
	}
    if (!global_RendererURI[0])
		rtp_gui_list_add_int_item(pmenu, "No Media Renderer Selected", 0, 1, 1, 0);
	else
	{
    	dmc_demo_ui_listout_two(pmenu,"Renderer:", global_RendererURI);
    }
   	if (!global_fileName[0])
		rtp_gui_list_add_int_item(pmenu, "No Media File Selected", 0, 1, 1, 0);
	else
	{
    	dmc_demo_ui_listout_two(pmenu,"Media File:", global_fileName);
    	dmc_demo_ui_listout_two(pmenu,"      URI :", global_fileURI);
	}

	rtp_gui_list_execute_int(pmenu);
	rtp_gui_list_destroy(pmenu);
}


/*---------------------------------------------------------------------------*/
static void dmc_demo_command_rendererStopContent (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));


    /* Search for a Media Renderer by its type */
	deviceUrl = dmc_demo_getSelectedRenderer();
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

                    if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "Stop", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	dmc_demo_ui_report_error("Error occured in SetAVTransportURI request", &userData);
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
static void dmc_demo_command_rendererPlayContent (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Renderer by its Device type */
	deviceUrl = dmc_demo_getSelectedRenderer();
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

                    if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "Play", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	dmc_demo_ui_report_error("Error occured in Play request", &userData);
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
static void dmc_demo_command_sendMediaContent (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectURL)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Renderer by its type */
	deviceUrl = dmc_demo_getSelectedRenderer();
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

                    if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "SetAVTransportURI", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	dmc_demo_ui_report_error("Error occured in SetAVTransportURI request", &userData);
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
static void dmc_demo_playMediaContent (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectURL)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Renderer by its type */
	deviceUrl = dmc_demo_getSelectedRenderer();
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

                    if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "SetAVTransportURI", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	dmc_demo_ui_report_error("Error occured in SetAVTransportURI request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            int done = 0;
                            while(!done)
                            {
                            	int command;

								command = dmc_demo_ui_ask_render_command();
                                if (command == DMC_DEMO_RENDER_PLAY)
                                {
                                    dmc_demo_command_rendererPlayContent(rt, cp);
                                }
                                else if (command == DMC_DEMO_RENDER_STOP)
                                {
                                    dmc_demo_command_rendererStopContent(rt, cp);
                                }
                                else if (command == DMC_DEMO_RENDER_QUIT)
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
static void dmc_demo_command_renderMediaContent (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectURL)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Renderer by its type */
	deviceUrl = dmc_demo_getSelectedRenderer();
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

                    if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "SetAVTransportURI", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	dmc_demo_ui_report_error("Error occured in SetAVTransportURI request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            int done = 0;
                            while(!done)
                            {
                            	int command;
								command = dmc_demo_ui_ask_render_command();
                                if (command == DMC_DEMO_RENDER_PLAY)
                                {
                                    dmc_demo_command_rendererPlayContent(rt, cp);
                                }
                                else if (command == DMC_DEMO_RENDER_STOP)
                                {
                                    dmc_demo_command_rendererStopContent(rt, cp);
                                }
                                else if (command == DMC_DEMO_RENDER_QUIT)
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
static void dmc_demo_command_openMediaDevice (UPnPRuntime *rt, UPnPControlPoint *cp, char *deviceUrl, char* deviceType)
{
	UPnPControlDeviceHandle dh;
    rtp_gui_conout("Searching Media Devices...");


    /* Search for a UPnP Device by its type */
    if (!dmc_demo_selectDeviceOfType(cp, deviceType, SEARCH_DURATION, deviceUrl, 1))
		return;

    /* open the media server description document */
    rtp_gui_conout_two("Opening device at : ", deviceUrl);
    dh = UPnP_ControlOpenDevice (cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlDeviceInfo info;
		UPnPControlServiceIterator i;

		UPnP_ControlGetDeviceInfo(dh, &info);

		dmc_demo_ui_report_deviceinfo("  success", &info);

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


static void dmc_demo_command_browse ( UPnPRuntime *rt,   UPnPControlPoint *cp )
{
int n_containers, n_items;
	n_containers = dmc_demo_command_countDirectoriesOnServer (rt, cp, global_dirID);
	n_items = dmc_demo_command_countFilesOnServer(rt, cp, global_dirID);

	if (n_containers)
	{
		struct dmc_demo_directory *pdir_results;
		pdir_results = dmc_demo_command_browseMediaServer (rt, cp, global_dirID, 1);
		if (pdir_results)	/* Select from the results list and release the lists */
		{
			dmc_demo_select_directory(pdir_results);
				/* Now free the list of directories */
			dmc_demo_free_directory_list(pdir_results);
		}

	}
	else if (n_items)
	{
	struct dmc_demo_file *pfile_results;
		pfile_results = dmc_demo_command_searchFileOnServer (rt, cp, "*", "",global_dirID);
		if (pfile_results)
		{
				dmc_demo_select_file(pfile_results);
				if (global_fileURI[0])
				{
						/* file found */
						dmc_demo_command_playMediaFile(rt, cp, global_fileURI);
                       	dmc_demo_command_sendMediaContent (rt, cp, global_fileURI);
				}
		}
	}
	else
		rtp_gui_conout("No files or directories found.");
}
void _dmc_demo_treewalk ( UPnPRuntime *rt,   UPnPControlPoint *cp, struct dmc_demo_directory *pdir, char *id)
{
struct dmc_demo_directory *pdir_results;
struct dmc_demo_file *pfile_results;

	pdir->psubdirectory_list = 0;
	pdir->pfile_list = 0;

	pdir_results = dmc_demo_command_browseMediaServer (rt, cp, id, 1);

	if (pdir_results)
	{
		struct dmc_demo_directory *_pdir;
		pdir->psubdirectory_list = _pdir = pdir_results;
		while (_pdir)
		{
			rtp_printf("Descending into title: %s\n", _pdir->directory_title); // , _pdir->directory_id);
			 _dmc_demo_treewalk (rt, cp, _pdir, _pdir->directory_id);
			_pdir = _pdir->pnext;
		}
	}
	else
	{
		pfile_results = dmc_demo_command_searchFileOnServer (rt, cp, "*", "", id);
		if (pfile_results)
		{
			pdir->pfile_list = pfile_results;
		}
	}
//	if (!pdir->psubdirectory_list && !pdir->pfile_list)
//		rtp_gui_conout("No files or directories found.");
}

static void dmc_demo_treedisplay (struct dmc_demo_directory *ptreetop);

static void dmc_demo_treewalk ( UPnPRuntime *rt,   UPnPControlPoint *cp )
{
	if (global_directoryTree)
	{
		dmc_demo_free_directory_list(global_directoryTree);
		global_directoryTree = 0;
	}
	global_directoryTree = (struct dmc_demo_directory *) rtp_malloc(sizeof(*global_directoryTree));
	if (!global_directoryTree)
		return;
	rtp_memset(global_directoryTree, 0, sizeof(*global_directoryTree));
	global_directoryTree->directory_id = rtp_strdup(global_dirID);
	global_directoryTree->directory_title = rtp_strdup(global_dirName);
	_dmc_demo_treewalk (rt,  cp, global_directoryTree,global_dirID);
	dmc_demo_treedisplay (global_directoryTree);
}


static void _dmc_demo_treebuildlist (void *plist_box, struct dmc_demo_directory *pdir)
{
struct dmc_demo_directory *_pdir;

	_pdir = pdir;

	while(_pdir)
	{
		if (_pdir->psubdirectory_list)
		{	/* Make a new sub-menu can call treedisplay */
		void *pchild;
			pchild = rtp_gui_list_init(0, RTP_LIST_STYLE_LISTBOX, "", "", "");
			rtp_gui_list_add_item(plist_box, _pdir->directory_title, (void *) pchild, 0, 0, RTP_LIST_ITEM_STYLE_SUBMENU|RTP_LIST_ITEM_STYLE_EXPANDED);
			_dmc_demo_treebuildlist (pchild, _pdir->psubdirectory_list);
		}
		else if (_pdir->pfile_list)
		{
			struct dmc_demo_file *pfile;
			void *pchild;

			pchild = rtp_gui_list_init(0, RTP_LIST_STYLE_LISTBOX, "", "", "");
			rtp_gui_list_add_item(plist_box, _pdir->directory_title, (void *) pchild, 0, 0, RTP_LIST_ITEM_STYLE_SUBMENU|RTP_LIST_ITEM_STYLE_EXPANDED);
			pfile = _pdir->pfile_list;
			while (pfile)
			{
				rtp_gui_list_add_item(pchild, pfile->file_title, (void *) pfile->file_title, 0, 0, 0);
				pfile = pfile->pnext;
			 }
		}
		else
			rtp_gui_list_add_item(plist_box, _pdir->directory_title, 0, 0, 0, 0);

		_pdir = _pdir->pnext;
	}
}


static void dmc_demo_treedisplay (struct dmc_demo_directory *ptreetop)
{
void *plist_box;

	plist_box = rtp_gui_list_init(0, RTP_LIST_STYLE_LISTBOX, "EBS Media Controller Demo", "Tree Display", "Select >");
	_dmc_demo_treebuildlist (plist_box, ptreetop);
	rtp_gui_list_execute_int(plist_box);
	rtp_gui_list_destroy(plist_box);
}


/*---------------------------------------------------------------------------*/
struct dmc_demo_directory * dmc_demo_command_browseMediaServer (
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
	struct dmc_demo_directory *pdir_results;

	pdir_results = 0;

	if (is_container)
		psearchflag = "BrowseDirectChildren";
	else
		psearchflag = "BrowseMetadata";


    /* Search for a Media Server by its type */
	deviceUrl = dmc_demo_getSelectedServer();
	if (!deviceUrl)
		return(0);

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
                int starting_index;
				int total_matches = 0;
				int total_passes = 0;
				int numReturned = 0;
                starting_index = 0;
				do
				{
					char str[32];
                	/* Invoking 'Browse' action. This action contains 6 'in' arguments. */
                	action = UPnP_CreateAction(
                    	UPnP_ControlGetServiceType(dh,serviceId),"Browse");
                	if (!action)
                	{
						rtp_gui_conout (" could not create action!");
						break;
                	}
					/* In Arguments */
					UPnP_SetActionArg (action, "ObjectID", objectID);
					UPnP_SetActionArg (action, "BrowseFlag", psearchflag);
					UPnP_SetActionArg (action, "Filter", "*");
					rtp_sprintf(str, "%d", starting_index);
					UPnP_SetActionArg (action, "StartingIndex", str);
					rtp_sprintf(str, "%d", BROWSE_ELEMENT_COUNT);
					UPnP_SetActionArg (action, "RequestedCount", str);
					UPnP_SetActionArg (action, "SortCriteria", "");


					/* Out Arguments */
					userData.done = 0;
					userData.errorCode = 0;
					rtp_memset(&userData, 0, sizeof(userData));
                    userData.numOutElements = 4;
                    userData.outVariableName[0] = dmc_demo_ui_safe_rtp_strdup("Result");
                    userData.outVariableName[1] = dmc_demo_ui_safe_rtp_strdup("NumberReturned");
                    userData.outVariableName[2] = dmc_demo_ui_safe_rtp_strdup("TotalMatches");
                    userData.outVariableName[3] = dmc_demo_ui_safe_rtp_strdup("UpdateID");

                    if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "Browse", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	dmc_demo_ui_report_error("Error occured in Browse request", &userData);
                                rtp_free(userData.errorDescription);
								break;
                            }
                        }
                        else
                        {
                        RTPXML_Document *xmlResult;


                            numReturned = rtp_atoi(userData.outVariableValue[1]);
                           	total_matches += numReturned;
                           	total_passes += 1;

							if (numReturned > 0)
							{
                            	/* load the buffer containing xml document into a dom document */
                            	if(rtpxmlParseBufferEx(userData.outVariableValue[0], &xmlResult) == RTPXML_SUCCESS)
                            	{
                      				dmc_demo_parseDirectoryResults(xmlResult,&pdir_results);
                            	}
                            	starting_index += BROWSE_ELEMENT_COUNT;
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
                while(numReturned == BROWSE_ELEMENT_COUNT); /* do .. while */
				{
				char str[32];
				rtp_gui_conout("--------------------------------------");
				rtp_gui_conout(" Browse Results ");
				rtp_sprintf(str, "%d", total_passes);
				rtp_gui_conout_two("Total Passes: ", str);
				rtp_sprintf(str, "%d", total_matches);
				rtp_gui_conout_two("Total Matches: ", str);
				rtp_gui_conout("--------------------------------------");
				}
            }
        }
    }
	return(pdir_results);

}
// HEREHERE
/*---------------------------------------------------------------------------*/
static int dmc_demo_command_countDirectoriesOnServer (
        UPnPRuntime *rt,
        UPnPControlPoint *cp,
        char *objectID
    )
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;
	int index, total_matches = 0;


    /* Search for a Media Server by its type */
	deviceUrl = dmc_demo_getSelectedServer();
	if (!deviceUrl)
		return(0);

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
				int numReturned = 0;
				{
                	/* Invoking 'Browse' action. This action contains 6 'in' arguments. */
                	action = UPnP_CreateAction(
                    	UPnP_ControlGetServiceType(dh,serviceId),"Browse");
                	if (!action)
                	{
						rtp_gui_conout (" could not create action!");
						return(0);
                	}
					/* In Arguments */
					UPnP_SetActionArg (action, "ObjectID", objectID);
					UPnP_SetActionArg (action, "BrowseFlag", "BrowseDirectChildren");
					UPnP_SetActionArg (action, "Filter", "*");
					UPnP_SetActionArg (action, "StartingIndex", "0");
					UPnP_SetActionArg (action, "RequestedCount", "1");
					UPnP_SetActionArg (action, "SortCriteria", "");

					/* Out Arguments */
					userData.done = 0;
					userData.errorCode = 0;
					rtp_memset(&userData, 0, sizeof(userData));
                    userData.numOutElements = 4;
                    userData.outVariableName[0] = dmc_demo_ui_safe_rtp_strdup("Result");
                    userData.outVariableName[1] = dmc_demo_ui_safe_rtp_strdup("NumberReturned");
                    userData.outVariableName[2] = dmc_demo_ui_safe_rtp_strdup("TotalMatches");
                    userData.outVariableName[3] = dmc_demo_ui_safe_rtp_strdup("UpdateID");

                    if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "Browse", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	dmc_demo_ui_report_error("Error occured in Browse request", &userData);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                        RTPXML_Document *xmlResult;


                            numReturned = rtp_atoi(userData.outVariableValue[1]);

							if (numReturned > 0)
							{
                            	/* load the buffer containing xml document into a dom document */
                            	if(rtpxmlParseBufferEx(userData.outVariableValue[0], &xmlResult) == RTPXML_SUCCESS)
                            	{
                            		total_matches = dmc_demo_countDirectoryResults(xmlResult);
								}
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
            }
        }
    }
	return(total_matches);
}


static void dmc_demo_free_directory_list(struct dmc_demo_directory *presult)
{
	while(presult)
	{
	struct dmc_demo_directory *p;
		if (presult->psubdirectory_list)
			dmc_demo_free_directory_list(presult->psubdirectory_list);
		if (presult->pfile_list)
			dmc_demo_free_file_list(presult->pfile_list);
		dmc_demo_freeif(presult->directory_title);
		dmc_demo_freeif(presult->directory_id);
		p = presult;
		presult = presult->pnext;
		rtp_free(p);
	};

}
static void dmc_demo_select_directory(struct dmc_demo_directory *pdir_results)
{
void *plist,*result;
struct dmc_demo_directory *presult;
int isproot,isnochange;

	plist = rtp_gui_list_init(0, RTP_LIST_STYLE_LISTBOX, "EBS Media Controller Demo", "Select Default Directory", "Select >:");
	presult = pdir_results;
	while(presult)
	{
		rtp_gui_list_add_item(plist, presult->directory_title, (void *) presult, 1, 0, 0);
		presult = presult->pnext;
	};
	/* Add one more selection, for the root directory */
	rtp_gui_list_add_item(plist, "do not change", (void *) &isnochange, 1, 0, 0);
	rtp_gui_list_add_item(plist, "root", (void *) &isproot, 1, 0, 0);
	result = rtp_gui_list_execute_void(plist);
	rtp_gui_list_destroy(plist);
	if (result == (void *) &isnochange)
		;
	else if (result == (void *) &isproot)
	{
        rtp_strcpy(global_dirID, "0");
      	rtp_strcpy(global_dirName, "root");
	}
	else if (result)
	{
		presult = (struct dmc_demo_directory *) result;
		rtp_printf("Changing to |%s|\n", presult->directory_id); // HEREHERE
        rtp_strcpy(global_dirID, presult->directory_id);
      	rtp_strcpy(global_dirName, presult->directory_title);
	}
}
static void dmc_demo_free_file_list(struct dmc_demo_file *presult)
{
	while(presult)
	{
	struct dmc_demo_file *p;
		dmc_demo_freeif(presult->file_title);
		dmc_demo_freeif(presult->file_uri);
		dmc_demo_freeif(presult->file_id);
		p = presult;
		presult = presult->pnext;
		rtp_free(p);
	};
}

static void dmc_demo_select_file(struct dmc_demo_file *pfile_results)
{
void *plist,*result;
struct dmc_demo_file *presult;


	plist = rtp_gui_list_init(0, RTP_LIST_STYLE_LISTBOX, "EBS Media Controller Demo", "Select A File", "Select >:");
	presult = pfile_results;
	while(presult)
	{
		rtp_gui_list_add_item(plist, presult->file_title, (void *) presult, 1, 0, 0);
		presult = presult->pnext;
	};
	result = rtp_gui_list_execute_void(plist);
	rtp_gui_list_destroy(plist);

	if (!result)
		return;

	presult = (struct dmc_demo_file *) result;
    rtp_strcpy(global_fileName, presult->file_title);
    rtp_strcpy(global_fileURI, presult->file_uri);
    rtp_strcpy(global_fileID, presult->file_id);

	/* Now free the list of files */
	dmc_demo_free_file_list(pfile_results);
}


/*---------------------------------------------------------------------------*/
static void dmc_demo_command_playMediaFile(UPnPRuntime *rt, UPnPControlPoint *cp, char*url)
{
    int command =0;
	void *pmenu;

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_MENU, "EBS Media Controller Demo", "Select Option", "Option >");
	if (!pmenu)
		return;

	rtp_gui_list_add_int_item(pmenu, "Send selected file to renderer", 0, 0, 0, 0);
	rtp_gui_list_add_int_item(pmenu, "Continue without sending", 1, 0, 0, 0);

	command = rtp_gui_list_execute_int(pmenu);
	rtp_gui_list_destroy(pmenu);

	if (command == 0)
	{
       rtp_gui_conout("Sending file to renderer....");
       dmc_demo_command_renderMediaContent(rt, cp, url);
	}
}

/*---------------------------------------------------------------------------*/
static int dmc_demo_parseDirectoryResults(RTPXML_Document *xmlResult,struct dmc_demo_directory **pdir_results)
{
    RTPXML_NodeList* elementList;
    RTPXML_Element *element;
    int numberOfObjects;
    struct dmc_demo_directory *pnew_result, *pcurrent_result;

	/* If we already have a results list go to the end */
	if (*pdir_results)
	{
		pcurrent_result = *pdir_results;
		while(pcurrent_result->pnext)
			pcurrent_result = pcurrent_result->pnext;
	}
	else
		pcurrent_result = 0;

    /* get a list of all nodes containing tag - container */
    elementList = rtpxmlDocument_getElementsByTagName (xmlResult, "container");
    if(elementList)
    {
		int index;
        /* get the number of containers */
        numberOfObjects = (int) rtpxmlNodeList_length (elementList);

        /* enumerate each object */
        for(index = 0; index < (int)numberOfObjects; index++)
        {
            /* extract elements from the list */
    	    element = (RTPXML_Element *) rtpxmlNodeList_item(elementList, index);
            /* get the container name */
			if (element)
			{
				char *temp;
				pnew_result = (struct dmc_demo_directory *) rtp_malloc(sizeof(*pnew_result));
				rtp_memset(pnew_result, 0, sizeof(*pnew_result));
				temp = UPnP_DOMGetElemTextInElem (element, "dc:title", 0);
				if (temp)
					pnew_result->directory_title = rtp_strdup(temp);
				temp = rtpxmlElement_getAttribute (element, "id");
				if (temp)
					pnew_result->directory_id = rtp_strdup(temp);
				if (pcurrent_result)
					pcurrent_result->pnext = pnew_result;
				else
					*pdir_results = pnew_result;
				pcurrent_result = pnew_result;
			}
        }
		rtpxmlNodeList_free(elementList);
	}
	return(0);
}
static int  dmc_demo_countDirectoryResults(RTPXML_Document *xmlResult)
{
	struct dmc_demo_directory *pdir_results;
	int result = 0;
	pdir_results = 0;
	dmc_demo_parseDirectoryResults(xmlResult,&pdir_results);
	if (pdir_results)
	{
		struct dmc_demo_directory *presult = pdir_results;
		while(presult)
		{
			presult = presult->pnext;
			result += 1;
		};
		dmc_demo_free_directory_list(pdir_results);
	}
	return(result);
}

static int dmc_demo_parseFileResults(RTPXML_Document *xmlResult,struct dmc_demo_file **pfile_results)
{
    RTPXML_NodeList* elementList;
    RTPXML_Element *element;
    int matched = 0;
    struct dmc_demo_file *pnew_result, *pcurrent_result;
    int numberOfObjects;

	/* If we already have a results list go to the end */
	if (*pfile_results)
	{
		pcurrent_result = *pfile_results;
		while(pcurrent_result->pnext)
			pcurrent_result = pcurrent_result->pnext;
	}
	else
		pcurrent_result = 0;

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
			{
               /* get the container name */
            	temp = UPnP_DOMGetElemTextInElem (element, "dc:title", 0);
               	if (temp)
               	{
					pnew_result = (struct dmc_demo_file *) rtp_malloc(sizeof(*pnew_result));
					rtp_memset(pnew_result, 0, sizeof(*pnew_result));
					pnew_result->file_title = rtp_strdup(temp);
					temp = UPnP_DOMGetElemTextInElem (element, "res", 0);
					if (temp)
						pnew_result->file_uri = rtp_strdup(temp);
					temp = rtpxmlElement_getAttribute (element, "id");
					if (temp)
						pnew_result->file_id = rtp_strdup(temp);
					if (pcurrent_result)
						pcurrent_result->pnext = pnew_result;
					else
						*pfile_results = pnew_result;
					pcurrent_result = pnew_result;
				}
            }
		}
		rtpxmlNodeList_free(elementList);
	}
    return (0);
}

static int  dmc_demo_countFileResults(RTPXML_Document *xmlResult)
{
	struct dmc_demo_file *pfile_results;
	int result = 0;
	pfile_results = 0;
	dmc_demo_parseFileResults(xmlResult,&pfile_results);
	if (pfile_results)
	{
		struct dmc_demo_file *presult = pfile_results;
		while(presult)
		{
			presult = presult->pnext;
			result += 1;
		};
		dmc_demo_free_file_list(pfile_results);
	}
	return(result);
}

/*---------------------------------------------------------------------------*/
static char *dmc_demo_parseSearchResult(char *result, struct dmc_demo_file **ppfile_results)
{
    RTPXML_Document *xmlResult;

    /* load the buffer containing xml document into a dom document */
    if(rtpxmlParseBufferEx(result, &xmlResult) != RTPXML_SUCCESS)
	{
	    return(0);
    }
    dmc_demo_parseFileResults(xmlResult,ppfile_results);
    return(0);
}

static char *global_SearchOptions[] = {
"*",
"upnp:class = object.item.audioItem.music",
"upnp:class = object.item.imageItem.photo",
"upnp:class = object.item.videoItem.movie",
"upnp:class derivedfrom object.item.audioItem",
"upnp:class derivedfrom object.item.audioItem",
"upnp:class derivedfrom object.item.imageItem",
"upnp:class derivedfrom object.item.videoItem",
"^\"\" (empty string)",
"# I want to type in my own search option",
0
};
static char *global_SortOptions[] = {
"^\"\" (empty string)",
"+dc:title",
"+dc:artist",
"+dc:date",
"-dc:date",
"# I want to type in my own sort option",
0
};

/*---------------------------------------------------------------------------*/
static struct dmc_demo_file * dmc_demo_command_searchFileOnServer (UPnPRuntime *rt, UPnPControlPoint *cp, char *psearch, char *psort, char *id)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;
    int index;
    int numReturned;
	struct dmc_demo_file *pfile_results;
	pfile_results = 0;


    /* Search for a Media Server by its type */
	deviceUrl = dmc_demo_getSelectedServer();
	if (!deviceUrl)
		return(0);

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
				int starting_index = 0;
				char search_buffer[128];
				char sort_buffer[128];

				/* Prompt for search and sort if not reqested */
				if (!psearch)
					psearch = dmc_demo_ui_list_or_prompt("Select a search (note: * is always safe)", "Select : >", global_SearchOptions, search_buffer);
				if (!psort)
					psort = dmc_demo_ui_list_or_prompt("Select a sort (note: \"\" is always safe)", "Select : >", global_SortOptions, sort_buffer);

				do
				{
                /* Out Arguments */
                userData.done = 0;
                userData.errorCode = 0;
                rtp_memset(&userData, 0, sizeof(userData));
                userData.numOutElements = 4;
                userData.outVariableName[0] = dmc_demo_ui_safe_rtp_strdup("Result");
                userData.outVariableName[1] = dmc_demo_ui_safe_rtp_strdup("NumberReturned");
                userData.outVariableName[2] = dmc_demo_ui_safe_rtp_strdup("TotalMatches");
                userData.outVariableName[3] = dmc_demo_ui_safe_rtp_strdup("UpdateID");

                numReturned = 0;

                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"Search");
                if (action)
				{
				char str[32];

						/* In Arguments */
						UPnP_SetActionArg (action, "ContainerID", id);

						UPnP_SetActionArg (action, "SearchCriteria", psearch); /*   "upnp:class = object.item.audioItem.music" */
						UPnP_SetActionArg (action, "Filter", "*");
						rtp_sprintf(str, "%d", starting_index);
						UPnP_SetActionArg (action, "StartingIndex", str);
						rtp_sprintf(str, "%d", BROWSE_ELEMENT_COUNT);
						UPnP_SetActionArg (action, "RequestedCount", str);
						UPnP_SetActionArg (action, "SortCriteria", psort);

	                    if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "Search", action, &userData, 1) >= 0)
						{
	                        /* catch errors if any */
	                        if (userData.errorCode != 0)
	                        {
	                            if (userData.errorDescription)
	                            {
	                            	dmc_demo_ui_report_error("Error occured in Search request", &userData);
	                                rtp_free(userData.errorDescription);
									break;
	                            }
	                        }
	                        else
	                        {
	                            numReturned = rtp_atoi(userData.outVariableValue[1]);
	                            if(numReturned > 0)
	                            {
									/* Add to the file list */
	                                dmc_demo_parseSearchResult(userData.outVariableValue[0], &pfile_results);
	                                starting_index += BROWSE_ELEMENT_COUNT;
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
	                } /* if (action) */
                } while(numReturned == BROWSE_ELEMENT_COUNT);

            } /* if (serviceid) */
        }
    }
	return(pfile_results);
}


/*---------------------------------------------------------------------------*/
static int dmc_demo_command_countFilesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp, char *objectID)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;
    int index, numReturned;
	int total_matches = 0;
	struct dmc_demo_file *pfile_results;
	pfile_results = 0;


    /* Search for a Media Server by its type */
	deviceUrl = dmc_demo_getSelectedServer();
	if (!deviceUrl)
		return(0);

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
				int starting_index = 0;

                /* Out Arguments */
                userData.done = 0;
                userData.errorCode = 0;
                rtp_memset(&userData, 0, sizeof(userData));
                userData.numOutElements = 4;
                userData.outVariableName[0] = dmc_demo_ui_safe_rtp_strdup("Result");
                userData.outVariableName[1] = dmc_demo_ui_safe_rtp_strdup("NumberReturned");
                userData.outVariableName[2] = dmc_demo_ui_safe_rtp_strdup("TotalMatches");
                userData.outVariableName[3] = dmc_demo_ui_safe_rtp_strdup("UpdateID");

                numReturned = 0;

                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"Search");
                if (action)
				{

					/* In Arguments */
					UPnP_SetActionArg (action, "ContainerID", objectID);

					UPnP_SetActionArg (action, "SearchCriteria", "*");
					UPnP_SetActionArg (action, "Filter", "*");
					UPnP_SetActionArg (action, "StartingIndex", "0");
					UPnP_SetActionArg (action, "RequestedCount", "1");
					UPnP_SetActionArg (action, "SortCriteria", "");

	                if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "Search", action, &userData, 1) >= 0)
					{
	                    /* catch errors if any */
	                    if (userData.errorCode != 0)
	                    {
	                        if (userData.errorDescription)
	                        {
	                        	dmc_demo_ui_report_error("Error occured in Search request", &userData);
	                            rtp_free(userData.errorDescription);
	                        }
	                    }
	                    else
	                    {
	                        numReturned = rtp_atoi(userData.outVariableValue[1]);
	                        if(numReturned > 0)
	                        {
	                        	RTPXML_Document *xmlResult;
                            	/* load the buffer containing xml document into a dom document */
                            	if(rtpxmlParseBufferEx(userData.outVariableValue[0], &xmlResult) == RTPXML_SUCCESS)
                            	{
                            		total_matches = dmc_demo_countFileResults(xmlResult);
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
	            } /* if (action) */
            } /* if (serviceid) */
        }
    }
	return(total_matches);
}



/*---------------------------------------------------------------------------*/
static void dmc_demo_command_sortCapabilitiesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;
    int index;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Server by its type */
	deviceUrl = dmc_demo_getSelectedServer();
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
                userData.outVariableName[0] = dmc_demo_ui_safe_rtp_strdup("SortCaps");

                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"GetSortCapabilities");
                if (action)
				{
					/* In Arguments */

                    if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "GetSortCapabilities", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	dmc_demo_ui_report_error("Error occured in Sort request", &userData);
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
static void dmc_demo_command_searchCapabilitiesOnServer (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char *deviceUrl;
    int index;

    userData.done = 0;
    userData.errorCode = 0;
	rtp_memset(&userData, 0, sizeof(userData));

    /* Search for a Media Server by its type */
	deviceUrl = dmc_demo_getSelectedServer();
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
                userData.outVariableName[0] = dmc_demo_ui_safe_rtp_strdup("SearchCaps");

                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"GetSearchCapabilities");
                if (action)
				{
					/* In Arguments */

                    if (dmc_demo_UPnP_ControlInvokeAction (dh, serviceId, "GetSearchCapabilities", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                            	dmc_demo_ui_report_error("Error occured in Search request", &userData);
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

static char *dmc_demo_getSelectedServer(void)
{
	if (global_ServerURI[0])
		return(&global_ServerURI[0]);
	rtp_gui_conout("No Media server is selected");
	return(0);
}

static char *dmc_demo_getSelectedRenderer(void)
{
	if (global_RendererURI[0])
		return(&global_RendererURI[0]);
	rtp_gui_conout("No Media Renderer is selected");
	return(0);
}


static int dmc_demo_selectDeviceOfType(UPnPControlPoint* cp, UPNP_CHAR* deviceType, UPNP_INT32 timeoutMsec, void* userData,UPNP_BOOL waitForCompletion)
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
	void *index;
        index = rtp_gui_list_execute_void(global_list_box);
		rtp_strcpy(dev_url, rtp_gui_list_item_value(global_list_box, index,0));
        rtp_gui_list_destroy(global_list_box);
		global_list_box = 0;
		return(1);
	}
}
/*---------------------------------------------------------------------------*/
static int dmc_demo_controlPointCallback (
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

            rtp_sprintf(str, "%s", event->data.discover.url); /* Why are we doing this ? */

			if (global_list_box && event->data.discover.url)
			{
				if (rtp_gui_list_check_duplicate_item(global_list_box, (char *)event->data.discover.url))
				{
					; // rtp_gui_conout("Disgarding duplicate reply--------");
				}
				else
				{
					rtp_gui_conout_two(" Device found at: ", (char *)event->data.discover.url);
					rtp_gui_list_add_int_item(global_list_box , (char *)event->data.discover.url, rtp_gui_list_size(global_list_box), 0, 0, 0);
//            rtp_gui_conout_two("         Service: ", (char *)event->data.discover.type);
//            rtp_gui_conout("--------------------------------------------------");
				}
			}
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
                userData->errorDescription = dmc_demo_ui_safe_rtp_strdup(event->data.action.errorDescription);
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
                        userData->outVariableValue[i] = dmc_demo_ui_safe_rtp_strdup((char *)outValue);
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

		        dmc_demo_ui_report_deviceinfo("  success", &info);

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



#define DMC_DEMO_PROMPT_HELP			 "Help"
#define DMC_DEMO_PROMPT_QUIT			 "Quit"
#define DMC_DEMO_PROMPT_SHOW_SELECTIONS	 "Show current selections for server, renderer, file.."
#define DMC_DEMO_PROMPT_SELECT_SERVER	 "Select a Media Server (UPnP_ControlOpenDevice (MediaServer:1)...."
#define DMC_DEMO_PROMPT_SELECT_RENDERER  "Select a Media Renderer (UPnP_ControlOpenDevice (MediaRenderer:1)...."
#define DMC_DEMO_PROMPT_BROWSE_CONTAINER "Browse Selected Container (Browse id=current_container BrowseFlag=BrowseDirectChildren)"
#define DMC_DEMO_PROMPT_TREEWALK         "Browse all subdirectories below current directory."
#define DMC_DEMO_PROMPT_SELECT_FILE		 "Select a File (Search ContainerID=\"\",SearchCriteria=\"\",Filter=\"\",...)"
#define DMC_DEMO_PROMPT_PLAY_FILE		 "Play Selected File (SetAVTransportURI, Stop, Play)"
#define DMC_DEMO_PROMPT_SHOW_SEARCH_CAPS "Show search capabilities list (GetSearchCapabilities)"
#define DMC_DEMO_PROMPT_SHOW_SORT_CAPS   "Show sort capabilities list (GetSortCapabilities)"
#define DMC_DEMO_PROMPT_SEND_MEDIA       "Queue File for Renderer (SetAVTransportURI)"
#define DMC_DEMO_PROMPT_PLAY             "Play queued file on the renderer (Play)"
#define DMC_DEMO_PROMPT_STOP             "Stop file playing on the renderer (Stop)"
#define DMC_DEMO_PROMPT_SELECT_ROOT		 "Return to the root directory"
static int dmc_demo_ui_main_menu(void)
{
void *pmenu;
int   command, greyed_list[DMC_DEMO_NCOMMANDS];

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_MENU, "EBS Media Controller Demo", "Main Menu", "Command >");
	if (!pmenu)
		return(-1);
	rtp_memset(&greyed_list[0], 0, sizeof(greyed_list));

   greyed_list[DMC_DEMO_SELECT_SERVER]    = 0;
   greyed_list[DMC_DEMO_SELECT_ROOT]    = 0;
   greyed_list[DMC_DEMO_SELECT_RENDERER]  = 0;
   if (!global_ServerURI[0])
   {
   greyed_list[DMC_DEMO_BROWSE_CONTAINER] = 1;
   greyed_list[DMC_DEMO_TREEWALK]      = 1;
   greyed_list[DMC_DEMO_SELECT_FILE]      = 1;
   greyed_list[DMC_DEMO_SELECT_ROOT]      = 1;
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


   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_HELP,DMC_DEMO_HELP, 0, greyed_list[DMC_DEMO_HELP], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_SHOW_SELECTIONS,DMC_DEMO_SHOW_SELECTIONS, 0, greyed_list[DMC_DEMO_SHOW_SELECTIONS], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_SELECT_SERVER,DMC_DEMO_SELECT_SERVER, 0, greyed_list[DMC_DEMO_SELECT_SERVER], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_SELECT_RENDERER,DMC_DEMO_SELECT_RENDERER, 0, greyed_list[DMC_DEMO_SELECT_RENDERER], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_SELECT_ROOT,DMC_DEMO_SELECT_ROOT, 0, greyed_list[DMC_DEMO_SELECT_ROOT], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_BROWSE_CONTAINER,DMC_DEMO_BROWSE_CONTAINER, 0, greyed_list[DMC_DEMO_BROWSE_CONTAINER], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_TREEWALK,DMC_DEMO_TREEWALK, 0, greyed_list[DMC_DEMO_TREEWALK], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_SELECT_FILE,DMC_DEMO_SELECT_FILE, 0, greyed_list[DMC_DEMO_SELECT_FILE], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_PLAY_FILE,DMC_DEMO_PLAY_FILE, 0, greyed_list[DMC_DEMO_PLAY_FILE], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_SEND_MEDIA,DMC_DEMO_SEND_MEDIA, 0, greyed_list[DMC_DEMO_SEND_MEDIA], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_PLAY,DMC_DEMO_PLAY, 0, greyed_list[DMC_DEMO_PLAY], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_STOP,DMC_DEMO_STOP, 0, greyed_list[DMC_DEMO_STOP], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_SHOW_SEARCH_CAPS,DMC_DEMO_SHOW_SEARCH_CAPS, 0, greyed_list[DMC_DEMO_SHOW_SEARCH_CAPS], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_SHOW_SORT_CAPS,DMC_DEMO_SHOW_SORT_CAPS, 0, greyed_list[DMC_DEMO_SHOW_SORT_CAPS], 0);
   rtp_gui_list_add_int_item(pmenu, DMC_DEMO_PROMPT_QUIT,DMC_DEMO_QUIT, 0, greyed_list[DMC_DEMO_QUIT], 0);

   command = rtp_gui_list_execute_int(pmenu);
   rtp_gui_list_destroy(pmenu);
   return(command);
}

static int dmc_demo_ui_ask_render_command(void)
{
	void *pmenu;
	int command =0;

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_MENU, "EBS Media Controller Demo", "Select Command", "Command >");
	if (!pmenu)
		return(0);

	rtp_gui_list_add_int_item(pmenu, "Play", DMC_DEMO_RENDER_PLAY, 0, 0, 0);
	rtp_gui_list_add_int_item(pmenu, "Stop", DMC_DEMO_RENDER_STOP, 0, 0, 0);
	rtp_gui_list_add_int_item(pmenu, "Quit", DMC_DEMO_RENDER_QUIT, 0, 0, 0);

	command = rtp_gui_list_execute_int(pmenu);
	rtp_gui_list_destroy(pmenu);
	return(command);
}

static char *dmc_demo_ui_list_or_prompt(char *title, char *prompt, char *choices[], char *inbuffer)
{
void *pmenu;
int n_choices = 0;
int index;

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_MENU, "EBS Media Controller Demo", title, prompt);
	if (!pmenu)
		return(0);
	while(choices[n_choices])
	{
		rtp_gui_list_add_int_item(pmenu, choices[n_choices], n_choices, 0, 0, 0);
		n_choices += 1;
	}
	index = rtp_gui_list_execute_int(pmenu);
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


static void dmc_demo_help(void)
{
	int i;
	void *p;
	p = rtp_gui_list_init(0, RTP_LIST_STYLE_HELP|RTP_LIST_STYLE_NOFORMAT, "EBS Media Controller Demo", "Instructions", "");
	if (!p)
		return;
	for (i = 0; global_Help[i]; i++)
		rtp_gui_list_add_int_item(p, global_Help[i], 0, 0, 1, 0);
	rtp_gui_list_execute_int(p);
	rtp_gui_list_destroy(p);
}

static void dmc_demo_ui_report_error(char *alert, struct actionUserData *puserData)
{
void *p;
	p = rtp_gui_list_init(0, RTP_LIST_STYLE_HELP|RTP_LIST_STYLE_NOFORMAT, "EBS Media Controller Demo", "Error Status", "");
	if (p)
	{
		char str[32];
		if (alert)
			dmc_demo_ui_listout_two(p, "alert      : ", alert);
		rtp_sprintf(str, "%d", puserData->errorCode);
		dmc_demo_ui_listout_two(p, "Error Code : ", str);
		if (puserData->errorDescription);
			dmc_demo_ui_listout_two(p, "Description: ", puserData->errorDescription);
		rtp_gui_list_execute_int(p);
		rtp_gui_list_destroy(p);
	}
}



static void dmc_demo_ui_report_deviceinfo(char *alert, UPnPControlDeviceInfo *pinfo)
{
void *pmenu;

	pmenu = rtp_gui_list_init(0, RTP_LIST_STYLE_LISTBOX|RTP_LIST_STYLE_NOFORMAT, "EBS Media Controller Demo", "Device Description", "Press Return >");
	if (!pmenu)
		return;

    dmc_demo_ui_listout_two(pmenu, "  deviceType      :",	pinfo->deviceType);
	dmc_demo_ui_listout_two(pmenu, "  friendlyName    :",	pinfo->friendlyName);
	dmc_demo_ui_listout_two(pmenu, "  manufacturer    :",	pinfo->manufacturer);
	dmc_demo_ui_listout_two(pmenu, "  manufacturerURL :",	pinfo->manufacturerURL);
	dmc_demo_ui_listout_two(pmenu, "  modelDescription:",	pinfo->modelDescription);
	dmc_demo_ui_listout_two(pmenu, "  modelName       :",	pinfo->modelName);
	dmc_demo_ui_listout_two(pmenu, "  modelNumber     :",	pinfo->modelNumber);
	dmc_demo_ui_listout_two(pmenu, "  modelURL        :",	pinfo->modelURL);
	dmc_demo_ui_listout_two(pmenu, "  serialNumber    :",	pinfo->serialNumber);
	dmc_demo_ui_listout_two(pmenu, "  UDN             :",	pinfo->UDN);
	dmc_demo_ui_listout_two(pmenu, "  UPC             :",	pinfo->UPC);
	dmc_demo_ui_listout_two(pmenu, "  presentationURL :",	pinfo->presentationURL);

	rtp_gui_list_execute_int(pmenu);
	rtp_gui_list_destroy(pmenu);
}

static void dmc_demo_ui_listout_two(void *pmenu,char *item1, char *item2)
{
	if (!item1)
		item1 = "Null";
	if (!item2)
		item2 = "Null";
   	rtp_gui_list_add_int_item(pmenu, item1, 0, 0, 1, RTP_LIST_ITEM_STYLE_NOBREAK);
   	rtp_gui_list_add_int_item(pmenu, item2, 0, 0, 1, 0);
}
static char *dmc_demo_ui_safe_rtp_strdup(char *v)
{
	if (v)
		return(rtp_strdup(v));
	return(rtp_strdup("dmc_demo_ui_safe_rtp_strdup passed null"));
}

static void dmc_demo_freeif(char *p)
{
	if (p)
		rtp_free(p);
}

static int dmc_demo_UPnP_ControlInvokeAction (
		UPnPControlDeviceHandle deviceHandle,
		UPNP_CHAR* serviceId,
		const UPNP_CHAR* actionName,
		RTPXML_Document* action,
		void* userData,
		UPNP_BOOL waitForCompletion
	)
{
int retval;

	retval = UPnP_ControlInvokeAction ( deviceHandle, serviceId, actionName, action, userData, waitForCompletion);

	if (retval < 0)
	{
		void *p;
		p = rtp_gui_list_init(0, RTP_LIST_STYLE_HELP|RTP_LIST_STYLE_NOFORMAT, "EBS Media Controller Demo", "UPnP_ControlInvokeAction failed executing action", "");
		if (p)
		{
			char str[256];
    		rtp_sprintf(str,"UPnP_ControlInvokeAction failed executing action: \"%s\"", actionName);
			rtp_gui_list_add_int_item(p, str, 0, 0, 1, 0);
			rtp_gui_list_execute_int(p);
			rtp_gui_list_destroy(p);
		}
	}

	return(retval);
}
