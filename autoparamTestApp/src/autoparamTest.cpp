#include <autoparamDriver.h>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <iocsh.h>
#include <epicsExport.h>

class AutoparamTest;

class PVInfo : public Autoparam::PVInfo {
  public:
    PVInfo(Autoparam::PVInfo const &parsed, AutoparamTest *driver)
        : Autoparam::PVInfo(parsed), driver(driver) {}

    AutoparamTest *driver;
};

class AutoparamTest : public Autoparam::Driver {
  public:
    AutoparamTest(char const *portName)
        : Autoparam::Driver(
              portName, Autoparam::DriverOpts()
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
    PVInfo *createPVInfo(Autoparam::PVInfo const &baseInfo) {
        return new PVInfo(baseInfo, this);
    }

  private:
    static Int32ReadResult randomRead(Autoparam::PVInfo &baseInfo) {
        Int32ReadResult result;
        PVInfo &pvInfo = static_cast<PVInfo &>(baseInfo);
        AutoparamTest *self = pvInfo.driver;
        result.value = rand_r(&self->randomSeed);
        return result;
    }

    static WriteResult sumArgs(Autoparam::PVInfo &baseInfo, epicsInt32 value) {
        WriteResult result;
        PVInfo &pvInfo = static_cast<PVInfo &>(baseInfo);
        AutoparamTest *self = pvInfo.driver;

        if (pvInfo.arguments().front() == "set") {
            self->currentSum = value;
        } else {
            typedef Autoparam::PVInfo::ArgumentList::const_iterator Iter;
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

    static Int32ReadResult readSum(Autoparam::PVInfo &baseInfo) {
        Int32ReadResult result;
        PVInfo &pvInfo = static_cast<PVInfo &>(baseInfo);
        AutoparamTest *self = pvInfo.driver;
        result.value = self->currentSum;
        return result;
    }

    static Float64ReadResult erroredRead(Autoparam::PVInfo &baseInfo) {
        Float64ReadResult result;
        PVInfo &pvInfo = static_cast<PVInfo &>(baseInfo);
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
