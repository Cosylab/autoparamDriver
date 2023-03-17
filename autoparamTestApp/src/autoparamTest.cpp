// SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
//
// SPDX-License-Identifier: MIT

#include <autoparamDriver.h>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <iocsh.h>
#include <epicsThread.h>
#include <epicsExport.h>

#ifdef _MSC_VER
// Windows doesn't have rand_r, instead it reseeds rand, so it's ok.
#define rand_r(x) rand()
#endif /* ifdef _MSC_VER */

using namespace Autoparam::Convenience;

static const double interruptScanPeriod = 1.5;

class AutoparamTest;

typedef std::vector<std::string> ArgumentList;

class MyAddress : public DeviceAddress {
  public:
    bool operator==(DeviceAddress const &other) const {
        MyAddress const &o = static_cast<MyAddress const &>(other);
        return function == o.function && arguments == o.arguments;
    }

    std::string function;
    ArgumentList arguments;
};

class MyVar : public DeviceVariable {
  public:
    MyVar(DeviceVariable *baseVar, AutoparamTest *driver)
        : DeviceVariable(baseVar), driver(driver) {}

    ArgumentList const &arguments() const {
        return static_cast<MyAddress const &>(address()).arguments;
    }

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

                std::vector<DeviceVariable *> intrs =
                    self->getInterruptVariables();
                for (std::vector<DeviceVariable *>::iterator i = intrs.begin(),
                                                             end = intrs.end();
                     i != end; ++i) {
                    DeviceVariable &pv = **i;
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
    DeviceAddress *parseDeviceAddress(std::string const &function,
                                      std::string const &arguments) {
        MyAddress *p = new MyAddress;
        p->function = function;

        std::istringstream is(arguments);
        std::string arg;
        while (is >> arg) {
            p->arguments.push_back(arg);
        }

        return p;
    }

    DeviceVariable *createDeviceVariable(DeviceVariable *baseVar) {
        return new MyVar(baseVar, this);
    }

  private:
    static void testInitHook(Autoparam::Driver *driver) {
        AutoparamTest *self = static_cast<AutoparamTest *>(driver);
        printf("Running init hook for Autoparam::Driver 0x%p "
               "with the following PVs:\n",
               driver);
        std::vector<DeviceVariable *> pvs = self->getAllVariables();
        for (size_t i = 0; i < pvs.size(); ++i) {
            printf("    0x%p: %s\n", pvs[i], pvs[i]->asString().c_str());
        }
    }

    static asynStatus interruptReg(DeviceVariable &baseVar, bool cancel) {
        printf("Interrupt %s: %s\n", (cancel ? "cancelled" : "registered"),
               baseVar.asString().c_str());
        return asynSuccess;
    }

    static Int32ReadResult randomRead(DeviceVariable &baseVar) {
        Int32ReadResult result;
        MyVar &deviceVariable = static_cast<MyVar &>(baseVar);
        AutoparamTest *self = deviceVariable.driver;
        result.value = rand_r(&self->randomSeed);
        result.processInterrupts = true;
        return result;
    }

    static WriteResult sumArgs(DeviceVariable &baseVar, epicsInt32 value) {
        WriteResult result;
        MyVar &deviceVariable = static_cast<MyVar &>(baseVar);
        AutoparamTest *self = deviceVariable.driver;

        if (deviceVariable.arguments().front() == "set") {
            self->currentSum = value;
        } else {
            typedef ArgumentList::const_iterator Iter;
            for (Iter i = deviceVariable.arguments().begin(),
                      end = deviceVariable.arguments().end();
                 i != end; ++i) {
                std::istringstream istr(*i);
                int val;
                istr >> val;
                self->currentSum += val;
            }
        }

        return result;
    }

    static Int32ReadResult readSum(DeviceVariable &baseVar) {
        Int32ReadResult result;
        MyVar &deviceVariable = static_cast<MyVar &>(baseVar);
        AutoparamTest *self = deviceVariable.driver;
        result.value = self->currentSum;
        return result;
    }

    static Float64ReadResult erroredRead(DeviceVariable &baseVar) {
        Float64ReadResult result;
        MyVar &deviceVariable = static_cast<MyVar &>(baseVar);
        std::string const &arg = deviceVariable.arguments().front();
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

    static ArrayReadResult wfm8Read(DeviceVariable &baseVar,
                                    Array<epicsInt8> &value) {
        ArrayReadResult result;
        MyVar &deviceVariable = static_cast<MyVar &>(baseVar);
        AutoparamTest *self = deviceVariable.driver;
        value.fillFrom(self->wfm8Data);
        return result;
    }

    static WriteResult wfm8Write(DeviceVariable &baseVar,
                                 Array<epicsInt8> const &value) {
        WriteResult result;
        MyVar &deviceVariable = static_cast<MyVar &>(baseVar);
        AutoparamTest *self = deviceVariable.driver;

        // Enforce an arbitrary limit
        if (value.size() < 8) {
            self->wfm8Data.assign(value.data(), value.data() + value.size());
        } else {
            result.status = asynOverflow;
        }

        return result;
    }

    static WriteResult bitsSet(DeviceVariable &baseVar, epicsUInt32 value,
                               epicsUInt32 mask) {
        WriteResult result;
        MyVar &deviceVariable = static_cast<MyVar &>(baseVar);
        deviceVariable.driver->shiftedRegister = (value & mask) << 3;
        return result;
    }

    static UInt32ReadResult bitsGet(DeviceVariable &baseVar, epicsUInt32 mask) {
        UInt32ReadResult result;
        MyVar &deviceVariable = static_cast<MyVar &>(baseVar);
        result.value = deviceVariable.driver->shiftedRegister & mask;
        return result;
    }

    static OctetReadResult argEcho(DeviceVariable &baseVar, Octet &value) {
        OctetReadResult result;
        std::string argcat;
        MyVar &deviceVariable = static_cast<MyVar &>(baseVar);
        for (ArgumentList::const_iterator
                 i = deviceVariable.arguments().begin(),
                 end = deviceVariable.arguments().end();
             i != end; ++i) {
            argcat += *i;
        }
        value.fillFrom(argcat);
        return result;
    }

    static WriteResult stringPrint(DeviceVariable &baseVar,
                                   Octet const &value) {
        printf("Got string: '");
        for (size_t i = 0; i < value.size(); ++i) {
            putchar(value.data()[i]);
        }
        putchar('\'');
        putchar('\n');
        return WriteResult();
    }

    unsigned randomSeed;
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
