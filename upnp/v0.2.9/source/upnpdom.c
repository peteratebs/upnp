//
// UPNPDOM.C -
//
// EBS -
//
// Copyright EBS Inc. , 2006
// All rights reserved.
// This code may not be redistributed in source or linkable object form
// without the consent of its author.
//

/*****************************************************************************/
// Header files
/*****************************************************************************/

#include "upnp.h"
#include "rtpstr.h"
#include "rtpscnv.h"

/*****************************************************************************/
// Macros
/*****************************************************************************/

/*****************************************************************************/
// Types
/*****************************************************************************/

/*****************************************************************************/
// Data
/*****************************************************************************/

/*****************************************************************************/
// Function Prototypes
/*****************************************************************************/

/*****************************************************************************/
// Function Definitions
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
RTPXML_Element * UPnP_DOMGetElemInDoc   (
		RTPXML_Document *descDoc,
		char *tagName,
		int num)
{
RTPXML_Node *elementNode;
RTPXML_NodeList *nodeList;

    nodeList = rtpxmlDocument_getElementsByTagName(descDoc, tagName);
    if (!nodeList)
    {
    	return (0);
	}

    elementNode = rtpxmlNodeList_item(nodeList, num);
    rtpxmlNodeList_free(nodeList);

    return ((RTPXML_Element *) elementNode);
}

/*---------------------------------------------------------------------------*/
RTPXML_Element * UPnP_DOMGetElemInDocNS   (
		RTPXML_Document *descDoc,
		char *tagName,
		char *nameSpace,
		int num)
{
RTPXML_Node *elementNode;
RTPXML_NodeList *nodeList;

    nodeList = rtpxmlDocument_getElementsByTagNameNS(descDoc, nameSpace, tagName);
    if (!nodeList)
    {
    	return (0);
	}

    elementNode = rtpxmlNodeList_item(nodeList, num);
    rtpxmlNodeList_free(nodeList);

    return ((RTPXML_Element *) elementNode);
}

/*---------------------------------------------------------------------------*/
RTPXML_Element * UPnP_DOMGetElemInElem   (
		RTPXML_Element *parent,
		char *tagName,
		int num)
{
RTPXML_Node *elementNode;
RTPXML_NodeList *nodeList;

    nodeList = rtpxmlElement_getElementsByTagName(parent, tagName);
    if (!nodeList)
    {
    	return (0);
	}

    elementNode = rtpxmlNodeList_item(nodeList, num);
    rtpxmlNodeList_free(nodeList);

    return ((RTPXML_Element *) elementNode);
}

/*---------------------------------------------------------------------------*/
DOMString UPnP_DOMGetElemTextInDoc   (
		RTPXML_Document *descDoc,
		char *tagName,
		int num)
{
RTPXML_Node *elementNode;
RTPXML_Node *textNode;
RTPXML_NodeList *nodeList;

    nodeList = rtpxmlDocument_getElementsByTagName(descDoc, tagName);
    if (!nodeList)
    {
    	return (0);
	}

    elementNode = rtpxmlNodeList_item(nodeList, num);
    rtpxmlNodeList_free(nodeList);

    if (!elementNode)
    {
    	return (0);
    }

    textNode = rtpxmlNode_getFirstChild(elementNode);
    if (!textNode)
    {
    	return (0);
    }

    return (rtpxmlNode_getNodeValue(textNode));
}

/*---------------------------------------------------------------------------*/
int UPnP_DOMSetElemTextInDoc  (
		RTPXML_Document *descDoc,
		char *tagName,
		int num,
		char *value)
{
RTPXML_Node *elementNode;
RTPXML_Node *textNode;
RTPXML_NodeList *nodeList;

    nodeList = rtpxmlDocument_getElementsByTagName(descDoc, tagName);
    if (!nodeList)
    {
    	return (-1);
	}

    elementNode = rtpxmlNodeList_item(nodeList, num);
    rtpxmlNodeList_free(nodeList);

    if (!elementNode)
    {
    	return (-1);
    }

    textNode = rtpxmlNode_getFirstChild(elementNode);
    if (!textNode)
    {
    	return (-1);
    }

    return (rtpxmlNode_setNodeValue(textNode, value));

}

/*---------------------------------------------------------------------------*/
DOMString UPnP_DOMGetElemTextInElem (
		RTPXML_Element *parent,
		char *tagName,
		int num)
{
RTPXML_Node *elementNode;
RTPXML_Node *textNode;
RTPXML_NodeList *nodeList;

    nodeList = rtpxmlElement_getElementsByTagName(parent, tagName);
    if (!nodeList)
    {
    	return (0);
	}

    elementNode = rtpxmlNodeList_item(nodeList, num);
    rtpxmlNodeList_free(nodeList);

    if (!elementNode)
    {
    	return (0);
    }

    textNode = rtpxmlNode_getFirstChild(elementNode);
    if (!textNode)
    {
    	return (0);
    }

    return (rtpxmlNode_getNodeValue(textNode));
}

/*---------------------------------------------------------------------------*/
char *UPnP_DOMSubstituteAddr (
		char *baseURL,
		RTP_NET_ADDR *netAddr)
{
	char temp[32];
	long protocolLen = 0, basePathLen = 0;
	char *ipPos, *pathPos, *str;

	if (!baseURL)
	{
		return (0);
	}

	rtp_net_ip_to_str(temp, netAddr->ipAddr, netAddr->type);

	ipPos = rtp_strstr(baseURL, "//");
	if (ipPos)
	{
		ipPos += 2;
		pathPos = rtp_strstr(ipPos, "/");
		if (pathPos)
		{
			basePathLen = rtp_strlen(pathPos);
		}
		protocolLen = ipPos - baseURL;
	}
	else
	{
		protocolLen = 7;
		pathPos = rtp_strstr(baseURL, "/");
		if (pathPos)
		{
			basePathLen = rtp_strlen(pathPos);
		}
	}

	str = (char *) rtp_malloc((rtp_strlen(temp) + protocolLen +
	                           basePathLen + 10) * sizeof(char));

	if (str)
	{
		if (ipPos)
		{
			rtp_strncpy(str, baseURL, protocolLen);
		}
		else
		{
			rtp_strcpy(str, "http://");
		}
		rtp_strcpy(str + protocolLen, temp);
		rtp_strcat(str, ":");
		rtp_itoa(netAddr->port, str + rtp_strlen(str), 10);
		if (pathPos)
		{
			rtp_strcat(str, pathPos);
		}
	}

	return (str);
}

/*---------------------------------------------------------------------------*/
char * UPnP_DOMGetServerPath (
		RTPXML_Document *descDoc,
		DOMString relUrl)
{
DOMString baseUrl;
char *str;

	str = rtp_strstr(relUrl, "//");
	if (str)
	{
		relUrl = str + 2;
		str = rtp_strstr(relUrl, "/");
		if (str)
		{
			relUrl = str;
		}

		return (rtp_strdup(relUrl));
	}

	baseUrl = UPnP_DOMGetElemTextInDoc(descDoc, "URLBase", 0);

	if (baseUrl)
	{
		str = rtp_strstr(baseUrl, "//");
		if (str)
		{
			baseUrl = str + 2;
		}

		baseUrl = rtp_strstr(baseUrl, "/");
	}

	if (baseUrl)
	{
		str = (char *) rtp_malloc((rtp_strlen(relUrl) + rtp_strlen(baseUrl) + 1) * sizeof(char));
		if (str)
		{
			rtp_strcpy(str, baseUrl);
			rtp_strcat(str, relUrl);
		}
	}
	else
	{
		return (rtp_strdup(relUrl));
	}

	return (str);
}
