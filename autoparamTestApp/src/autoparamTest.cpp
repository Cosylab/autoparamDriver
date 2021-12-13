#include <autoparamDriver.h>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <iocsh.h>
#include <epicsExport.h>

using namespace Autoparam::Convenience;

class AutoparamTest;

class MyInfo : public PVInfo {
  public:
    MyInfo(PVInfo const &parsed, AutoparamTest *driver)
        : PVInfo(parsed), driver(driver) {}

    AutoparamTest *driver;
};

class AutoparamTest : public Autoparam::Driver {
  public:
    AutoparamTest(char const *portName)
        : Autoparam::Driver(
              portName, Autoparam::DriverOpts()
                            .setAutodestruct()
                            .setInterfaceMask(asynInt32Mask | asynFloat64Mask |
                                              asynFloat32ArrayMask)
                            .setInterruptMask(asynInt32Mask | asynFloat64Mask |
                                              asynFloat32ArrayMask)
                            .setAutoconnect()),
          randomSeed(time(NULL) + clock()), currentSum(0) {
        registerHandlers<epicsInt32>("RANDOM", randomRead, NULL);
        registerHandlers<epicsInt32>("SUM", readSum, sumArgs);
        registerHandlers<epicsFloat64>("ERROR", erroredRead, NULL);
    }

  protected:
    MyInfo *createPVInfo(PVInfo const &baseInfo) {
        return new MyInfo(baseInfo, this);
    }

  private:
    static Int32ReadResult randomRead(PVInfo &baseInfo) {
        Int32ReadResult result;
        MyInfo &pvInfo = static_cast<MyInfo &>(baseInfo);
        AutoparamTest *self = pvInfo.driver;
        result.value = rand_r(&self->randomSeed);
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

    uint randomSeed;
    epicsInt32 currentSum;
};

static int const num_args = 1;
static iocshArg const arg1 = {"port name", iocshArgString};
static iocshArg const *const args[num_args] = {&arg1};
static iocshFuncDef command = {"AutoparamTestPort", num_args, args};

static void call(iocshArgBuf const *args) { new AutoparamTest(args[0].sval); }

extern "C" {

static void autoparamTestCommandRegistrar() { iocshRegister(&command, call); }

epicsExportRegistrar(autoparamTestCommandRegistrar);
}
