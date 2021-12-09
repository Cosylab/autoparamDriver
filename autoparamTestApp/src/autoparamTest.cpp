#include <autoparamDriver.h>

class AutoparamTest;

class Reason : public Autoparam::Reason {
  public:
    Reason(Autoparam::Reason const &parsed, AutoparamTest *driver)
        : Autoparam::Reason(parsed), driver(driver) {}

    AutoparamTest *driver;
};

class AutoparamTest : public Autoparam::Driver {
    AutoparamTest(char const *portName)
        : Autoparam::Driver(
              portName, Autoparam::DriverOpts()
                            .setInterfaceMask(asynInt32Mask | asynFloat64Mask |
                                              asynFloat32ArrayMask)
                            .setInterruptMask(asynInt32Mask | asynFloat64Mask |
                                              asynFloat32ArrayMask)
                            .setAutoconnect()) {
        registerHandlers<epicsInt32>("LONG", int32ReadHandler,
                                     int32WriteHandler);
    }

    Reason *createReason(Autoparam::Reason const &baseReason) {
        return new Reason(baseReason, this);
    }

    static Int32ReadResult int32ReadHandler(Autoparam::Reason &baseReason) {
        Int32ReadResult result;
        Reason &reason = static_cast<Reason &>(baseReason);
        AutoparamTest *self = reason.driver;
        result.value = 42;
        return result;
    }

    static WriteResult int32WriteHandler(Autoparam::Reason &baseReason,
                                         epicsInt32 value) {
        WriteResult result;
        Reason &reason = static_cast<Reason &>(baseReason);
        AutoparamTest *self = reason.driver;
        result.status = asynError;
        return result;
    }
};
