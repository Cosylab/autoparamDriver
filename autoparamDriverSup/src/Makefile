# SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
#
# SPDX-License-Identifier: MIT-0

TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_CXXFLAGS_Linux += --std=c++03 -fvisibility=hidden

#==================================================
# build a support library

LIBRARY_IOC += autoparamDriver
SHRLIB_VERSION = 1

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install autoparamDriver.dbd into <top>/dbd
DBD += autoparamDriver.dbd

# specify all source files to be compiled and added to the library
autoparamDriver_SRCS += autoparamDriver.cpp

autoparamDriver_LIBS += $(EPICS_BASE_IOC_LIBS)

# install these include files
INC += autoparamDriver.h
INC += autoparamHandler.h

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

