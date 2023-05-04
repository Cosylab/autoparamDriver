# SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
#
# SPDX-License-Identifier: MIT-0

TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_CXXFLAGS_Linux += --std=c++03 -fvisibility=hidden

ifndef BASE_7_0
  $(error "Unsupported EPICS version, autoparamDriver needs EPICS 7")
endif

#==================================================
# build a support library

LIBRARY_IOC += autoparamDriver
SHRLIB_VERSION = 2

# Install the API header. If EPICS is older than 7.0.4, use a pre-built header.
ifeq ($(if $(wildcard $(TOOLS)/makeAPIheader.pl),YES,NO),YES)
API_HEADER = autoparamDriverAPI.h
autoparamDriver_API = autoparamDriver
else
INC = autoparamDriverAPI.h
$(COMMON_DIR)/autoparamDriverAPI.h: autoparamDriverAPI_prebuilt.h
	$(CP) $< $@
USR_CPPFLAGS += -DBUILDING_autoparamDriver_API
endif

#DBDINC += xxxRecord
# install autoparamDriver.dbd into <top>/dbd
DBD += autoparamDriver.dbd

# specify all source files to be compiled and added to the library
autoparamDriver_SRCS += autoparamDriver.cpp

autoparamDriver_LIBS += asyn $(EPICS_BASE_IOC_LIBS)

# install these include files
INC += autoparamDriver.h
INC += autoparamHandler.h

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

