TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += autoparamDriver

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install autoparamDriver.dbd into <top>/dbd
DBD += autoparamDriver.dbd

# specify all source files to be compiled and added to the library
#autoparamDriver_SRCS += xxx

autoparamDriver_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

