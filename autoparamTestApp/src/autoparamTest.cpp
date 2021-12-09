#include <autoparamDriver.h>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <iocsh.h>
#include <epicsExport.h>

class AutoparamTest;

class Reason : public Autoparam::Reason {
  public:
    Reason(Autoparam::Reason const &parsed, AutoparamTest *driver)
        : Autoparam::Reason(parsed), driver(driver) {}

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
    Reason *createReason(Autoparam::Reason const &baseReason) {
        return new Reason(baseReason, this);
    }

  private:
    static Int32ReadResult randomRead(Autoparam::Reason &baseReason) {
        Int32ReadResult result;
        Reason &reason = static_cast<Reason &>(baseReason);
        AutoparamTest *self = reason.driver;
        result.value = rand_r(&self->randomSeed);
        return result;
    }

    static WriteResult sumArgs(Autoparam::Reason &baseReason,
                               epicsInt32 value) {
        WriteResult result;
        Reason &reason = static_cast<Reason &>(baseReason);
        AutoparamTest *self = reason.driver;

        if (reason.arguments().front() == "set") {
            self->currentSum = value;
        } else {
            typedef Autoparam::Reason::ArgumentList::const_iterator Iter;
            for (Iter i = reason.arguments().begin(),
                      end = reason.arguments().end();
                 i != end; ++i) {
                std::istringstream istr(*i);
                int val;
                istr >> val;
                self->currentSum += val;
            }
        }

        return result;
    }

    static Int32ReadResult readSum(Autoparam::Reason &baseReason) {
        Int32ReadResult result;
        Reason &reason = static_cast<Reason &>(baseReason);
        AutoparamTest *self = reason.driver;
        result.value = self->currentSum;
        return result;
    }

    static Float64ReadResult erroredRead(Autoparam::Reason &baseReason) {
        Float64ReadResult result;
        Reason &reason = static_cast<Reason &>(baseReason);
        std::string const &arg = reason.arguments().front();
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
