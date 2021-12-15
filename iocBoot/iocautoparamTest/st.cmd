#!../../bin/linux-x86_64/autoparamTest

#- You may have to change autoparamTest to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/autoparamTest.dbd"
autoparamTest_registerRecordDeviceDriver pdbbase

drvAutoparamTestConfigure("TST1")

## Load record instances
dbLoadRecords("db/test.db","PREFIX=test,PORT=TST1")

cd "${TOP}/iocBoot/${IOC}"
iocInit
