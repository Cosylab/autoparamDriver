TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = autoparamTest
# autoparamTest.dbd will be created and installed
DBD += autoparamTest.dbd

# autoparamTest.dbd will be made up from these files:
autoparamTest_DBD += base.dbd

# Include dbd files from all support applications:
#autoparamTest_DBD += xxx.dbd

# Add all the support libraries needed by this IOC
#autoparamTest_LIBS += xxx

# autoparamTest_registerRecordDeviceDriver.cpp derives from autoparamTest.dbd
autoparamTest_SRCS += autoparamTest_registerRecordDeviceDriver.cpp

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

