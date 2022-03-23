// SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
//
// SPDX-License-Identifier: MIT

#include <autoparamDriver.h>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <iocsh.h>
#include <epicsExport.h>
#include <epicsThread.h>

using namespace Autoparam::Convenience;

static const double interruptScanPeriod = 1.5;

class AutoparamTest;

class MyInfo : public PVInfo {
  public:
    MyInfo(PVInfo const &parsed, AutoparamTest *driver)
        : PVInfo(parsed), driver(driver) {}

    AutoparamTest *driver;
};

class AutoparamTest : public Autoparam::Driver {

    // Runs in a thread, updating IO Intr. records.
    class Runnable : public epicsThreadRunable {
      public:
        Runnable(AutoparamTest *self) : self(self) {}

        void run() {
            while (true) {
                epicsThreadSleep(interruptScanPeriod);
                self->lock();
                if (self->quitThread) {
                    self->unlock();
                    return;
                }

                std::vector<PVInfo *> intrs = self->getInterruptPVs();
                for (std::vector<PVInfo *>::iterator i = intrs.begin(),
                                                     end = intrs.end();
                     i != end; ++i) {
                    PVInfo &pv = **i;
                    if (pv.function() == "RANDOM") {
                        self->setParam(pv, (int32_t)rand_r(&self->randomSeed));
                    }
                }

                self->callParamCallbacks();
                self->unlock();
            }
        }

      private:
        AutoparamTest *self;
    };

  public:
    AutoparamTest(char const *portName)
        : Autoparam::Driver(
              portName, Autoparam::DriverOpts().setAutoDestruct().setInitHook(
                            AutoparamTest::testInitHook)),
          randomSeed(time(NULL) + clock()), currentSum(0), shiftedRegister(0),
          thread(runnable, "AutoparamTestThread", epicsThreadStackMedium),
          runnable(this), quitThread(false) {
        registerHandlers<epicsInt32>("RANDOM", randomRead, NULL, interruptReg);
        registerHandlers<epicsInt32>("SUM", readSum, sumArgs, NULL);
        registerHandlers<epicsFloat64>("ERROR", erroredRead, NULL, NULL);
        registerHandlers<Array<epicsInt8> >("WFM8", wfm8Read, wfm8Write, NULL);
        registerHandlers<epicsInt32>("DEFHANDLER", NULL, NULL, NULL);
        registerHandlers<epicsUInt32>("DIGIO", bitsGet, bitsSet, NULL);
        registerHandlers<Octet>("ARGECHO", argEcho, NULL, NULL);
        registerHandlers<Octet>("PRINT", NULL, stringPrint, NULL);

        thread.start();
    }

    ~AutoparamTest() {
        lock();
        quitThread = true;
        unlock();
        thread.exitWait();
    }

  protected:
    MyInfo *createPVInfo(PVInfo const &baseInfo) {
        return new MyInfo(baseInfo, this);
    }

  private:
    static void testInitHook(Autoparam::Driver *driver) {
        AutoparamTest *self = static_cast<AutoparamTest *>(driver);
        printf("Running init hook for Autoparam::Driver 0x%p "
               "with the following PVs:\n",
               driver);
        std::vector<PVInfo *> pvs = self->getAllPVs();
        for (size_t i = 0; i < pvs.size(); ++i) {
            printf("    0x%p: %s\n", pvs[i], pvs[i]->normalized().c_str());
        }
    }

    static asynStatus interruptReg(PVInfo &baseInfo, bool cancel) {
        printf("Interrupt %s: %s\n", (cancel ? "cancelled" : "registered"),
               baseInfo.normalized().c_str());
        return asynSuccess;
    }

    static Int32ReadResult randomRead(PVInfo &baseInfo) {
        Int32ReadResult result;
        MyInfo &pvInfo = static_cast<MyInfo &>(baseInfo);
        AutoparamTest *self = pvInfo.driver;
        result.value = rand_r(&self->randomSeed);
        result.processInterrupts = true;
        return result;
    }

    static WriteResult sumArgs(PVInfo &baseInfo, epicsInt32 value) {
        WriteResult result;
        MyInfo &pvInfo = static_cast<MyInfo &>(baseInfo);
        AutoparamTest *self = pvInfo.driver;

        if (pvInfo.arguments().front() == "set") {
            self->currentSum = value;
        } else {
            typedef PVInfo::ArgumentList::const_iterator Iter;
            for (Iter i = pvInfo.arguments().begin(),
                      end = pvInfo.arguments().end();
                 i != end; ++i) {
                std::istringstream istr(*i);
                int val;
                istr >> val;
                self->currentSum += val;
            }
        }

        return result;
    }

    static Int32ReadResult readSum(PVInfo &baseInfo) {
        Int32ReadResult result;
        MyInfo &pvInfo = static_cast<MyInfo &>(baseInfo);
        AutoparamTest *self = pvInfo.driver;
        result.value = self->currentSum;
        return result;
    }

    static Float64ReadResult erroredRead(PVInfo &baseInfo) {
        Float64ReadResult result;
        MyInfo &pvInfo = static_cast<MyInfo &>(baseInfo);
        std::string const &arg = pvInfo.arguments().front();
        if (arg == "error") {
            result.status = asynError;
        } else if (arg == "timeout") {
            result.status = asynTimeout;
        } else if (arg == "hwlimit") {
            result.alarmStatus = epicsAlarmHwLimit;
            result.alarmSeverity = epicsSevMajor;
        } else {
            result.alarmStatus = epicsAlarmSoft;
            result.alarmSeverity = epicsSevInvalid;
        }
        return result;
    }

    static ArrayReadResult wfm8Read(PVInfo &baseInfo, Array<epicsInt8> &value) {
        ArrayReadResult result;
        MyInfo &pvInfo = static_cast<MyInfo &>(baseInfo);
        AutoparamTest *self = pvInfo.driver;
        value.fillFrom(self->wfm8Data);
        return result;
    }

    static WriteResult wfm8Write(PVInfo &baseInfo,
                                 Array<epicsInt8> const &value) {
        WriteResult result;
        MyInfo &pvInfo = static_cast<MyInfo &>(baseInfo);
        AutoparamTest *self = pvInfo.driver;

        // Enforce an arbitrary limit
        if (value.size() < 8) {
            self->wfm8Data.assign(value.data(), value.data() + value.size());
        } else {
            result.status = asynOverflow;
        }

        return result;
    }

    static WriteResult bitsSet(PVInfo &baseInfo, epicsUInt32 value,
                               epicsUInt32 mask) {
        WriteResult result;
        MyInfo &pvInfo = static_cast<MyInfo &>(baseInfo);
        pvInfo.driver->shiftedRegister = (value & mask) << 3;
        return result;
    }

    static UInt32ReadResult bitsGet(PVInfo &baseInfo, epicsUInt32 mask) {
        UInt32ReadResult result;
        MyInfo &pvInfo = static_cast<MyInfo &>(baseInfo);
        result.value = pvInfo.driver->shiftedRegister & mask;
        return result;
    }

    static OctetReadResult argEcho(PVInfo &info, Octet &value) {
        OctetReadResult result;
        std::string argcat;
        for (PVInfo::ArgumentList::const_iterator i = info.arguments().begin(),
                                                  end = info.arguments().end();
             i != end; ++i) {
            argcat += *i;
        }
        value.fillFrom(argcat);
        return result;
    }

    static WriteResult stringPrint(PVInfo &baseInfo, Octet const &value) {
        printf("Got string: '");
        for (size_t i = 0; i < value.size(); ++i) {
            putchar(value.data()[i]);
        }
        putchar('\'');
        putchar('\n');
        return WriteResult();
    }

    uint randomSeed;
    epicsInt32 currentSum;
    std::vector<epicsInt8> wfm8Data;
    epicsUInt32 shiftedRegister;
    epicsThread thread;
    Runnable runnable;
    bool quitThread;
};

static int const num_args = 1;
static iocshArg const arg1 = {"port name", iocshArgString};
static iocshArg const *const args[num_args] = {&arg1};
static iocshFuncDef command = {"drvAutoparamTestConfigure", num_args, args};

static void call(iocshArgBuf const *args) { new AutoparamTest(args[0].sval); }

extern "C" {

static void autoparamTestCommandRegistrar() { iocshRegister(&command, call); }

epicsExportRegistrar(autoparamTestCommandRegistrar);
}
