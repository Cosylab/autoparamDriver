# SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
#
# SPDX-License-Identifier: CC0-1.0

TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_CXXFLAGS += --std=c++03

#=============================
# Build the IOC application

PROD_IOC = autoparamTest
# autoparamTest.dbd will be created and installed
DBD += autoparamTest.dbd

# autoparamTest.dbd will be made up from these files:
autoparamTest_DBD += base.dbd
autoparamTest_DBD += autoparamTestCommand.dbd

# Include dbd files from all support applications:
autoparamTest_DBD += asyn.dbd

# Add all the support libraries needed by this IOC
autoparamTest_LIBS += autoparamDriver
autoparamTest_LIBS += asyn

# autoparamTest_registerRecordDeviceDriver.cpp derives from autoparamTest.dbd
autoparamTest_SRCS += autoparamTest_registerRecordDeviceDriver.cpp

autoparamTest_SRCS += autoparamTest.cpp

# Build the main IOC entry point on workstation OSs.
autoparamTest_SRCS_DEFAULT += autoparamTestMain.cpp
autoparamTest_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#autoparamTest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
autoparamTest_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

