# This file evaluates the variables set on the command line and acts on them.
# targ is set to something such as POLG68K, RTPXMSP3, etc.
# RELENV should be N for ebsenv, Y otherwise.
# v=v for debug, v=o for optimized.

########################################################################
targ = LINUX

# set the following to "Y" for RTUPnP, "N" otherwise.
dortupnp = "Y"


########################################################################

root    = ..
kernel  = c:

#source path
lib_to_upnp_common_path      = $(UPNP_PATH)/v0.2.9/source
lib_to_upnp_server_path      = $(UPNP_PATH)/v0.2.9/source/device
lib_to_upnp_client_path      = $(UPNP_PATH)/v0.2.9/source/controlPoint
lib_to_av_common_path        = $(UPNP_PATH)/av0.1.1/source/device
lib_to_av_renderer_path      = $(UPNP_PATH)/av0.1.1/source/device/mediaRenderer
lib_to_av_server_path        = $(UPNP_PATH)/av0.1.1/source/device/mediaServer
lib_to_av_examples_path      = $(UPNP_PATH)/av0.1.1/source/examples
lib_to_upnp_examples_path    = $(UPNP_PATH)/v0.2.9/source/examples
lib_to_xml_path              = $(UPNP_PATH)/../ixml/v1.2/src
lib_to_http_path             = $(UPNP_PATH)/../http/v0.2.5/source
lib_to_rtptarget             = $(UPNP_PATH)/../rtplatform/v0.4.2/source/linux
lib_to_rtpgeneric            = $(UPNP_PATH)/../rtplatform/v0.4.2/source/generic

#include path
lib_to_upnp_common_include   = $(UPNP_PATH)/v0.2.9/include
lib_to_upnp_server_include   = $(UPNP_PATH)/v0.2.9/include/device
lib_to_upnp_client_include   = $(UPNP_PATH)/v0.2.9/include/controlPoint
lib_to_av_common_include     = $(UPNP_PATH)/av0.1.1/include/device
lib_to_av_renderer_include   = $(UPNP_PATH)/av0.1.1/include/device/mediaRenderer
lib_to_av_server_include     = $(UPNP_PATH)/av0.1.1/include/device/mediaServer
lib_to_xml1_include          = $(UPNP_PATH)/../ixml/v1.2/inc
lib_to_xml2_include          = $(UPNP_PATH)/../ixml/v1.2/src/inc
lib_to_http_include          = $(UPNP_PATH)/../http/v0.2.5/include
lib_to_rtptarget_include     = $(UPNP_PATH)/../rtplatform/v0.4.2/include/linux
lib_to_rtpgeneric_include    = $(UPNP_PATH)/../rtplatform/v0.4.2/include

########################################################################

known = "N"

ifeq ($(targ),LINUX)
known = "Y"
# Set the compiler type for later conditionals
compiler = GNUCOMPILER
endif

ifneq ($(known),"Y")
.PHONY: error
error:
ifneq ($(targ),)
	@echo Incorrect target: $(targ)
else
	@echo Please set the targ and v variables.
endif
	@echo Please edit
endif

########################################################################

ifeq ($(compiler),GNUCOMPILER)
# Set the file extensions
obj = o
lib = a
exe = out
endif

############################ THE END ###################################
