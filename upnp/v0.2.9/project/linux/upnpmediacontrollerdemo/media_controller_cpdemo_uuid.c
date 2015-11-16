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
#include "stdio.h"
#include "upnpcli.h"
#include "rtptime.h"
#include "rtpthrd.h"
#include "upnpdom.h"
#include <conio.h>

/*****************************************************************************/
/* Macros
 *****************************************************************************/

#define MEDIA_RENDERER_UDN          "FEEDBABE-5AFE-F00D-0090-3efffe0085d5"
#define MEDIA_SERVER_UDN            "8d3ccfa4-7349-11db-830a-00107500015c"

#define SEARCH_DURATION             2000
#define BROWSE_ELEMENT_COUNT        "10"
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

int controlPointCallback (
		UPnPControlPoint* cp,
		UPnPControlEvent* event,
		void *perControl,
		void *perRequest
	);

void openMediaDevice (UPnPRuntime *rt, UPnPControlPoint *cp, char* udn);
void discoverMediaServer (UPnPRuntime *rt, UPnPControlPoint *cp);
void browseMediaServer (UPnPRuntime *rt, UPnPControlPoint *cp);
void searchFileOnServer (UPnPRuntime *rt, UPnPControlPoint *cp);
void genaTest (UPnPRuntime *rt, UPnPControlPoint *cp);
void renderMediaContent(UPnPRuntime *rt, UPnPControlPoint *cp, char*url);

/*****************************************************************************/
/* Data
 *****************************************************************************/

/*****************************************************************************/
/* Function Definitions
 *****************************************************************************/
/*---------------------------------------------------------------------------*/
int main (int* argc, char* agrv[])
{
	int result;
	int done = 0;
    char ch;
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
		/* Initialize the control point */
        if (UPnP_ControlPointInit(&cp, &rt, controlPointCallback, 0) >= 0)
		{
           	while(!done)
            {
                printf("\n********************************\n");
                printf("   Media Controller Demo Menu\n");
                printf("********************************\n\n");
                printf("Media Server - \n");
                printf("--> S to View Media Server\n");
                printf("--> B to Browse Root Directory\n");
                printf("--> f to search a mp3 file on server\n");
                printf("\nMedia Renderer - \n");
                printf("--> R to View Media Renderer\n");
                printf("\n--> Q to Exit Demo\n");
                ch = getch();
                ch = tolower(ch);
                switch (ch)
                {
                    case 's':
                        openMediaDevice (&rt, &cp, MEDIA_SERVER_UDN);
                        break;
                    case 'b': //Browse Mode
                        browseMediaServer (&rt, &cp);
                        break;
                    case 'f':
                        searchFileOnServer (&rt, &cp);
                        break;
                    case 'r':
                        openMediaDevice (&rt, &cp, MEDIA_RENDERER_UDN);
                        break;
                    case 'q':
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

/*---------------------------------------------------------------------------*/
void rendererStopContent (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char deviceUrl[256];

    userData.done = 0;
    userData.errorCode = 0;

    /* Search by Media Server's UUID for unique media server */
    UPnP_ControlFindDevicesByUUID(cp, MEDIA_RENDERER_UDN,
	                              SEARCH_DURATION, deviceUrl, 1);

    printf("\nConnecting to Renderer at %s...", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		printf("device open.\n\nLooking for AVTransport:1 service...");

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
                                printf("\nError occured in SetAVTransportURI request \n");
                                printf("Error Code - %d\n", userData.errorCode);
                                printf("Error Description - %s\n", userData.errorDescription);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            printf("--------------------------------------\n");
                            printf("..Stopping...\n");
                            printf("--------------------------------------\n");
                        }
					}
				}
				else
				{
					printf (" could not create action!\n");
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
	char deviceUrl[256];

    userData.done = 0;
    userData.errorCode = 0;

    /* Search by Media Server's UUID for unique media server */
    UPnP_ControlFindDevicesByUUID(cp, MEDIA_RENDERER_UDN,
	                              SEARCH_DURATION, deviceUrl, 1);

    printf("\nConnecting to Renderer at %s...", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		printf("device open.\n\nLooking for AVTransport:1 service...");

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
                                printf("\nError occured in SetAVTransportURI request \n");
                                printf("Error Code - %d\n", userData.errorCode);
                                printf("Error Description - %s\n", userData.errorDescription);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            printf("..Starting play...\n");
                        }
					}
				}
				else
				{
					printf (" could not create action!\n");
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
	char deviceUrl[256];

    userData.done = 0;
    userData.errorCode = 0;

    /* Search by Media Server's UUID for unique media server */
    UPnP_ControlFindDevicesByUUID(cp, MEDIA_RENDERER_UDN,
	                              SEARCH_DURATION, deviceUrl, 1);

    printf("\nConnecting to Renderer at %s...", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		printf("device open.\n\nLooking for AVTransport:1 service...");

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
                                printf("\nError occured in SetAVTransportURI request \n");
                                printf("Error Code - %d\n", userData.errorCode);
                                printf("Error Description - %s\n", userData.errorDescription);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            int done = 0;
                            char ch;
                            printf("\n--------------------------------------\n");
                            printf("... Renderer Menu ...\n");
                            printf("\n--------------------------------------\n");
                            while(!done)
                            {
                                printf("\n P to Play \n");
                                printf("\n S to Stop  \n");
                                printf("\n Q to Quit  \n");
                                ch = getch();
                                ch = tolower(ch);
                                if (ch == 'p')
                                {
                                    rendererPlayContent(rt, cp);
                                }
                                if (ch == 's')
                                {
                                    rendererStopContent(rt, cp);
                                }
                                if (ch == 'q')
                                {
                                    done = 1;
                                }
                            }
                        }
					}
				}
				else
				{
					printf (" could not create action!\n");
				}
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
void openMediaDevice (UPnPRuntime *rt, UPnPControlPoint *cp, char* deviceUDN)
{
	UPnPControlDeviceHandle dh;
    char str[256];
    printf("\nSearching for Media Devices...\n\n");

    UPnP_ControlFindDevicesByUUID(cp, deviceUDN,
	                              SEARCH_DURATION, str, 1);

    /* open the media server description document */
    printf("\nOpening device at %s...", str);
    dh = UPnP_ControlOpenDevice (cp, str);
	if (dh != 0)
	{
		UPnPControlDeviceInfo info;
		UPnPControlServiceIterator i;

		UPnP_ControlGetDeviceInfo(dh, &info);

		printf (" success!\n\n"
				"  deviceType:       %.60s\n"
				"  friendlyName:     %.60s\n"
				"  manufacturer:     %.60s\n"
				"  manufacturerURL:  %.60s\n"
				"  modelDescription: %.60s\n"
				"  modelName:        %.60s\n"
				"  modelNumber:      %.60s\n"
				"  modelURL:         %.60s\n"
				"  serialNumber:     %.60s\n"
				"  UDN:              %.60s\n"
				"  UPC:              %.60s\n"
				"  presentationURL:  %.60s\n\n",
				info.deviceType,
				info.friendlyName,
	 			info.manufacturer,
				info.manufacturerURL,
				info.modelDescription,
				info.modelName,
				info.modelNumber,
				info.modelURL,
				info.serialNumber,
				info.UDN,
				info.UPC,
				info.presentationURL);

		if (UPnP_ControlGetServices(dh, &i) >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			while (serviceId)
			{
				printf ("Found Service: %.60s\n", serviceId);
				printf ("   (type: %.65s)\n", UPnP_ControlGetServiceType(dh, serviceId));
				serviceId = UPnP_ControlNextService(&i);
			}

			UPnP_ControlServiceIteratorDone(&i);
		}

		UPnP_ControlCloseDevice(dh);
	}
	else
	{
		printf (" open failed.\n");
	}

	printf("\n");
}

/*---------------------------------------------------------------------------*/
int parseBrowseResult(char *result)
{
    RTPXML_Document *xmlResult;
    RTPXML_NodeList* elementList;
    RTPXML_Element *element;
    unsigned long numberOfObjects;
    int index;

    /* load the buffer containing xml document into a dom document */
    if(rtpxmlParseBufferEx(result, &xmlResult) != RTPXML_SUCCESS)
	{
	    return(-1);
    }
    /* directories */
    /* get a list of all nodes containing tag - container */
    elementList = rtpxmlDocument_getElementsByTagName (xmlResult, "container");
    if(elementList)
    {
        /* get the number of containers */
        numberOfObjects = rtpxmlNodeList_length (elementList);

        /* enumerate each object */
        for(index = 0; index < (int)numberOfObjects; index++)
        {
            char *temp;
            /* extract elements from the list */
    	    element = (RTPXML_Element *) rtpxmlNodeList_item(elementList, index);
            /* get the container name */
            temp = UPnP_DOMGetElemTextInElem (element, "dc:title", 0);
            printf("Directory Name - %s\t\t", temp);
            /* Get the number of Children */
            temp = rtpxmlElement_getAttribute (element, "childCount");
            printf("Number of Children - %s\n", temp);
        }
        rtpxmlNodeList_free(elementList);
    }
    /* files */
    else
    {
        elementList = rtpxmlDocument_getElementsByTagName (xmlResult, "item");
        if(elementList)
        {
            /* get the number of containers */
            numberOfObjects = rtpxmlNodeList_length (elementList);

            /* enumerate each object */
            for(index = 0; index < (int)numberOfObjects; index++)
            {
                char *temp;
                /* extract elements from the list */
        	    element = (RTPXML_Element *) rtpxmlNodeList_item(elementList, index);
                /* get the container name */
                temp = UPnP_DOMGetElemTextInElem (element, "dc:title", 0);
                printf("File Name - %s\n\n", temp);
                temp = rtpxmlElement_getAttribute (element, "id");
                return atoi(temp);
                rtpxmlElement_free (element);
            }
            rtpxmlNodeList_free(elementList);
        }
    }
    return (0);
}

/*---------------------------------------------------------------------------*/
void browseMediaServer (
        UPnPRuntime *rt,
        UPnPControlPoint *cp
    )
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char deviceUrl[256];
    int index;

    userData.done = 0;
    userData.errorCode = 0;

    /* Search by Media Server's UUID for unique media server */
    UPnP_ControlFindDevicesByUUID(cp, MEDIA_SERVER_UDN,
	                              SEARCH_DURATION, deviceUrl, 1);

    printf("\nOpening device at %s...", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		printf("device open.\n\nLooking for ContentDirectory:1 service...");

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
					UPnP_SetActionArg (action, "ObjectID", "0");
					UPnP_SetActionArg (action, "BrowseFlag", "BrowseDirectChildren");
					UPnP_SetActionArg (action, "Filter", "*");
					UPnP_SetActionArg (action, "StartingIndex", "0");
					UPnP_SetActionArg (action, "RequestedCount", BROWSE_ELEMENT_COUNT);
					UPnP_SetActionArg (action, "SortCriteria", "");

					/* Out Arguments */
                    userData.numOutElements = 4;
                    userData.outVariableName[0] = rtp_strdup("Result");
                    userData.outVariableName[1] = rtp_strdup("NumberReturned");
                    userData.outVariableName[2] = rtp_strdup("TotalMatches");
                    userData.outVariableName[3] = rtp_strdup("UpdateID");

                    if (UPnP_ControlInvokeAction (dh, serviceId, "Browse", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                                printf("\nError occured in Browse request \n");
                                printf("Error Code - %d\n", userData.errorCode);
                                printf("Error Description - %s\n", userData.errorDescription);
                                rtp_free(userData.errorDescription);
                            }
                        }
                        else
                        {
                            printf("--------------------------------------\n");
                            printf(" Browse Results \n");
                            printf("Total Matches - %s \n", userData.outVariableValue[2]);
                            parseBrowseResult(userData.outVariableValue[0]);
                            printf("--------------------------------------\n");
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
					printf (" could not create action!\n");
				}
            }
        }
    }
}


/*---------------------------------------------------------------------------*/
char *parseSearchResult(char *result)
{
    RTPXML_Document *xmlResult;
    RTPXML_NodeList* elementList;
    RTPXML_Element *element;
    unsigned long numberOfObjects;
    int index;

    /* load the buffer containing xml document into a dom document */
    if(rtpxmlParseBufferEx(result, &xmlResult) != RTPXML_SUCCESS)
	{
	    return(0);
    }

    /* files */
    /* get a list of all nodes containing tag - item */
    elementList = rtpxmlDocument_getElementsByTagName (xmlResult, "item");
    if(elementList)
    {
        /* get the number of items */
        numberOfObjects = rtpxmlNodeList_length (elementList);

        /* enumerate each object */
        for(index = 0; index < (int)numberOfObjects; index++)
        {
            char *temp;
            /* extract elements from the list */
    	    element = (RTPXML_Element *) rtpxmlNodeList_item(elementList, index);
            /* get the container name */
            temp = UPnP_DOMGetElemTextInElem (element, "dc:title", 0);
            printf("Found Audio File On Server-\n");
            printf("\tName - %s\n", temp);
            temp = rtp_strdup(UPnP_DOMGetElemTextInElem (element, "res", 0));
            printf("\tFile path - %s\n", temp);
            return (temp);
        }
        rtpxmlNodeList_free(elementList);
    }
    return (0);
}

/*---------------------------------------------------------------------------*/
void searchFileOnServer (UPnPRuntime *rt, UPnPControlPoint *cp)
{
    UPnPControlDeviceHandle dh;
    struct actionUserData userData;
	char deviceUrl[256];
    int index;
    int numReturned;

    userData.done = 0;
    userData.errorCode = 0;

    /* Search by Media Server's UUID for unique media server */
    UPnP_ControlFindDevicesByUUID(cp, MEDIA_SERVER_UDN,
	                              SEARCH_DURATION, deviceUrl, 1);

    printf("\nOpening device at %s...", deviceUrl);
	dh = UPnP_ControlOpenDevice(cp, deviceUrl);
	if (dh != 0)
	{
		UPnPControlServiceIterator i;

		printf("device open.\n\nLooking for ContentDirectory:1 service...");

		/* find the ContentDirectory:1 service and invoke an action on it */
		if (UPnP_ControlGetServicesByType(dh, &i, "ContentDirectory:1") >= 0)
		{
			UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			if (serviceId)
			{
                RTPXML_Document* action;

                /* Out Arguments */
                userData.numOutElements = 4;
                userData.outVariableName[0] = rtp_strdup("Result");
                userData.outVariableName[1] = rtp_strdup("NumberReturned");
                userData.outVariableName[2] = rtp_strdup("TotalMatches");
                userData.outVariableName[3] = rtp_strdup("UpdateID");

                printf("Searching Root ....\n");
                action = UPnP_CreateAction(
                    UPnP_ControlGetServiceType(dh,serviceId),"Search");
                if (action)
				{
					/* In Arguments */
					UPnP_SetActionArg (action, "ContainerID", "0");
					UPnP_SetActionArg (action, "SearchCriteria",
					                    "upnp:class = object.item.audioItem.musicTrack");
					UPnP_SetActionArg (action, "Filter", "*");
					UPnP_SetActionArg (action, "StartingIndex", "0");
					UPnP_SetActionArg (action, "RequestedCount", "1");
					UPnP_SetActionArg (action, "SortCriteria", "");

                    if (UPnP_ControlInvokeAction (dh, serviceId, "Search", action, &userData, 1) >= 0)
					{
                        /* catch errors if any */
                        if (userData.errorCode != 0)
                        {
                            if (userData.errorDescription)
                            {
                                printf("\nError occured in search request\n");
                                printf("Error Code - %d\n", userData.errorCode);
                                printf("Error Description - %s\n", userData.errorDescription);
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
                                    while(!done)
                                    {
                                        printf("\n HIT Y TO SEND THIS FILE TO RENDERER \n");
                                        printf("\n N TO CONTINUE WITHOUT SENDING \n\n");
                                        ch = getch();
                                        ch = tolower(ch);
                                        if (ch == 'y')
                                        {
                                            done = 1;
                                            printf("Sending file to renderer....\n");
                                            renderMediaContent(rt, cp, objectURL);
                                        }
                                        if (ch == 'n')
                                        {
                                            done = 1;
                                        }
                                    }

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
            printf ("\n    Device/Service found at %.50s"
			        "\n    Type:  %.55s\n"
					, event->data.discover.url, event->data.discover.type);
            printf("---------------------------------------------------\n");
            sprintf(str, "%s", event->data.discover.url);
			result = 1; // if 1 is returned no further searches will be performed
			break;
		}

		case UPNP_CONTROL_EVENT_SEARCH_COMPLETE:
		{
			printf ("   Search Complete.\n\n\n");
			result = 1; // perform no furthur searches
			break;
		}

		case UPNP_CONTROL_EVENT_DEVICE_ALIVE:
		{
			printf ("\n\n    Device alive at %.50s"
			        "\n\n       ST:  %.55s"
					"\n       USN: %.55s", event->data.discover.url, event->data.discover.type, event->data.discover.usn);
			break;
		}

		case UPNP_CONTROL_EVENT_DEVICE_BYE_BYE:
		{
			printf ("\n\n    Device bye-bye "
			        "\n\n       ST:  %.55s"
					"\n       USN: %.55s", event->data.discover.type, event->data.discover.usn);
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
                printf("Action cannot be executed....\n");
                printf("Description: %s\n", event->data.action.errorDescription);
                userData->errorDescription = rtp_strdup(event->data.action.errorDescription);
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
                        userData->outVariableValue[i] = rtp_strdup(outValue);
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
			printf ("UPnP_ControlGetServiceInfoAsync failed!");
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

			printf ("service info read:\n\n");

			if (str)
			{
				printf("%s\n\n", str);
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
			printf("subscription accepted\n\n");
			printf("subscription id is: %s\n", event->data.subscription.serviceId);
			break;

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_CANCELLED:
            printf("service Unsubscribed\n\n");
			break;

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_REJECTED:
			printf("subscription rejected\n\n");
			break;

		case UPNP_CONTROL_EVENT_SERVICE_STATE_UPDATE:
		{
			const UPNP_CHAR* str = UPnP_GetPropertyValue(event->data.notify.stateVars, "LoadLevelStatus");
			if (!str)
			{
				str = "[unknown]";
			}
			printf ("Notification: Load level is =%s\n", str);
			break;
		}

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_NEAR_EXPIRATION:
			printf ("Subscription Expiration Warning!\n");
			break;

		case UPNP_CONTROL_EVENT_SUBSCRIPTION_EXPIRED:
			printf ("Subscription Expired.\n");
			break;

        case UPNP_CONTROL_EVENT_DEVICE_OPEN:
        {
            int* done = (int*) perRequest;

            if(event->data.device.handle)
	        {
		        UPnPControlDeviceInfo info;
		        UPnPControlServiceIterator i;

		        UPnP_ControlGetDeviceInfo(event->data.device.handle, &info);

		        printf (" success!\n\n"
				        "  deviceType:       %.60s\n"
				        "  friendlyName:     %.60s\n"
				        "  manufacturer:     %.60s\n"
				        "  manufacturerURL:  %.60s\n"
				        "  modelDescription: %.60s\n"
				        "  modelName:        %.60s\n"
				        "  modelNumber:      %.60s\n"
				        "  modelURL:         %.60s\n"
				        "  serialNumber:     %.60s\n"
				        "  UDN:              %.60s\n"
				        "  UPC:              %.60s\n"
				        "  presentationURL:  %.60s\n\n",
				        info.deviceType,
				        info.friendlyName,
	 			        info.manufacturer,
				        info.manufacturerURL,
				        info.modelDescription,
				        info.modelName,
				        info.modelNumber,
				        info.modelURL,
				        info.serialNumber,
				        info.UDN,
				        info.UPC,
				        info.presentationURL);

		        if (UPnP_ControlGetServices(event->data.device.handle, &i) >= 0)
		        {
			        UPNP_CHAR* serviceId = UPnP_ControlNextService(&i);

			        while (serviceId)
			        {
				        printf ("Found Service: %.60s\n", serviceId);
				        printf ("  (type: %.65s)\n", UPnP_ControlGetServiceType
                                (event->data.device.handle,
                                serviceId)
                            );
				        serviceId = UPnP_ControlNextService(&i);
			        }

			        UPnP_ControlServiceIteratorDone(&i);
		        }

		        UPnP_ControlCloseDevice(event->data.device.handle);
	        }
	        printf("\n");
            if(done)
            {
			    *done = 1;
            }

            break;
        }
        case UPNP_CONTROL_EVENT_DEVICE_OPEN_FAILED:
        {
            int* done = (int*) perRequest;
            printf("Device Open Failed\n");
            if(done)
            {
			    *done = 1;
            }

            break;
        }
	}

	return (result);
}
