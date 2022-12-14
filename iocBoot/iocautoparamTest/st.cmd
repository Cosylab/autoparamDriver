#!../../bin/linux-x86_64/autoparamTest

# SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
#
# SPDX-License-Identifier: MIT-0

#- You may have to change autoparamTest to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/autoparamTest.dbd"
autoparamTest_registerRecordDeviceDriver pdbbase

drvAutoparamTestConfigure("TST1")
asynSetTraceInfoMask("TST1", 0, SOURCE)
asynSetTraceMask("TST1", 0, ERROR+WARNING+FLOW)

## Load record instances
dbLoadRecords("db/test.db","PREFIX=test,PORT=TST1")

cd "${TOP}/iocBoot/${IOC}"
iocInit
