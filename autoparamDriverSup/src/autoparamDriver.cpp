// SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
//
// SPDX-License-Identifier: MIT

#include "autoparamDriver.h"

#include <errlog.h>
#include <epicsExit.h>
#include <initHooks.h>

#include <algorithm>
#include <sstream>

namespace Autoparam {

static char const *driverName = "Autoparam::Driver";

static std::map<Driver *, DriverOpts::InitHook> allInitHooks;

DeviceVariable::DeviceVariable(char const *reason, std::string const &function,
                               DeviceAddress *addr)
    : m_reasonString(reason), m_function(function), m_address(addr) {}

DeviceVariable::DeviceVariable(DeviceVariable *other) {
    m_function = other->m_function;
    m_reasonString = other->m_reasonString;
    m_asynParamType = other->m_asynParamType;
    m_asynParamIndex = other->m_asynParamIndex;
    m_address = other->m_address;
    other->m_address = NULL;
}

DeviceVariable::~DeviceVariable() {
    if (m_address) {
        delete m_address;
    }
}

// Copied from asyn. I wish they made this public.
char const *getAsynTypeName(asynParamType type) {
    static const char *typeNames[] = {
        "asynParamTypeUndefined", "asynParamInt32",
        "asynParamInt64",         "asynParamUInt32Digital",
        "asynParamFloat64",       "asynParamOctet",
        "asynParamInt8Array",     "asynParamInt16Array",
        "asynParamInt32Array",    "asynParamInt64Array",
        "asynParamFloat32Array",  "asynParamFloat64Array",
        "asynParamGenericPointer"};
    return typeNames[type];
}

static std::string getDtypName(asynParamType type) {
    std::string dtyp(getAsynTypeName(type));
    std::string const substr("Param");
    size_t idx = dtyp.find(substr);
    dtyp.erase(idx, substr.size());
    return dtyp;
}

void Driver::destroyDriver(void *driver) {
    Driver *drv = static_cast<Driver *>(driver);
    pasynManager->enable(drv->pasynUserSelf, 0);
    delete drv;
}

static void runInitHooks(initHookState state) {
    if (state != initHookAfterScanInit) {
        return;
    }

    for (std::map<Driver *, DriverOpts::InitHook>::iterator
             i = allInitHooks.begin(),
             end = allInitHooks.end();
         i != end; ++i) {
        i->second(i->first);
    }
}

static void addInitHook(Driver *driver, DriverOpts::InitHook hook) {
    static int const isRegistered __attribute__((unused)) =
        initHookRegister(runInitHooks);

    allInitHooks[driver] = hook;
}

Driver::Driver(const char *portName, const DriverOpts &params)
    : asynPortDriver(portName, 1, params.interfaceMask, params.interruptMask,
                     params.asynFlags, params.autoConnect, params.priority,
                     params.stackSize),
      opts(params) {
    if (params.autoDestruct) {
        epicsAtExit(destroyDriver, this);
    }

    if (params.initHook) {
        addInitHook(this, params.initHook);
    }

    installInterruptRegistrars();
}

Driver::~Driver() {
    for (ParamMap::iterator i = m_params.begin(), end = m_params.end();
         i != end; ++i) {
        delete i->second;
    }
}

namespace {

struct cmpDeviceAddress {
    DeviceAddress *addr;

    cmpDeviceAddress(DeviceAddress *p) : addr(p) {}

    bool operator()(std::map<int, DeviceVariable *>::value_type const &x) {
        return x.second->address() == *addr;
    }
};

} // namespace

asynStatus Driver::drvUserCreate(asynUser *pasynUser, const char *reason,
                                 const char **, size_t *) {
    std::string function;
    std::string arguments;
    {
        std::istringstream is(reason);
        if (!(is >> function)) {
            // Nice of us to do this check, but it seems we can't even get here,
            // asyn won't call us with an empty reason :)
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s: port=%s empty reason '%s'\n", driverName, portName,
                      reason);
            return asynError;
        }

        while (is && std::isspace(is.peek())) {
            is.ignore();
        }

        std::ostringstream os;
        os << is.rdbuf();
        arguments = os.str();
    }

    // Let the driver subclass parse the arguments.
    DeviceAddress *addr = parseDeviceAddress(function, arguments);
    if (addr == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: port=%s could not parse '%s'\n", driverName, portName,
                  reason);
        return asynError;
    }

    // Let's check if we already have the variable.
    ParamMap::iterator varIter =
        std::find_if(m_params.begin(), m_params.end(), cmpDeviceAddress(addr));
    if (varIter != m_params.end()) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s: port=%s reusing an existing parameter for '%s'\n",
                  driverName, portName, reason);
        pasynUser->reason = varIter->second->asynIndex();
        delete addr;
    } else {
        // No var found, let's create a new one. It takes ownership of `addr`.
        DeviceVariable baseVar = DeviceVariable(reason, function, addr);
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s: port=%s creating a new parameter for '%s'\n", driverName,
                  portName, baseVar.asString().c_str());

        try {
            baseVar.m_asynParamType = m_functionTypes.at(baseVar.function());
        } catch (std::out_of_range const &) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s: port=%s no handler registered for '%s'\n",
                      driverName, portName, baseVar.function().c_str());
            return asynError;
        }

        createParam(baseVar.asString().c_str(), baseVar.m_asynParamType,
                    &baseVar.m_asynParamIndex);

        // Let the derived driver construct a subclass of DeviceVariable based
        // on ours. Takes ownership of stuff in our `baseVar`.
        DeviceVariable *var = createDeviceVariable(&baseVar);
        if (var == NULL) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s: port=%s could not create DeviceVariable for '%s'\n",
                      driverName, portName, baseVar.asString().c_str());
            return asynError;
        }

        m_params[var->asynIndex()] = var;
        m_interruptRefcount[var] = 0;
        pasynUser->reason = var->asynIndex();
    }

    return asynSuccess;
}

void Driver::handleResultStatus(asynUser *pasynUser, ResultBase const &result) {
    pasynUser->alarmStatus = result.alarmStatus;
    setParamAlarmStatus(pasynUser->reason, result.alarmStatus);
    pasynUser->alarmSeverity = result.alarmSeverity;
    setParamAlarmSeverity(pasynUser->reason, result.alarmSeverity);
}

DeviceVariable *Driver::deviceVariableFromUser(asynUser *pasynUser) {
    try {
        return m_params.at(pasynUser->reason);
    } catch (std::out_of_range const &) {
        char const *paramName;
        asynStatus status = getParamName(pasynUser->reason, &paramName);
        if (status == asynSuccess) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s: port=%s no handler registered for '%s'\n",
                      driverName, portName, paramName);
        } else {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s: port=%s no parameter exists at index %d\n",
                      driverName, portName, pasynUser->reason);
        }
        return NULL;
    }
}

// This function is documented as threadsafe, which it is, based on the fact
// that m_params is not supposed to change at runtime. I wish more functions
// could be made const-correct ...
std::vector<DeviceVariable *> Driver::getAllVariables() const {
    std::vector<DeviceVariable *> pvs;
    pvs.reserve(m_params.size());
    for (ParamMap::const_iterator i = m_params.begin(), end = m_params.end();
         i != end; ++i) {
        pvs.push_back(i->second);
    }
    return pvs;
}

template <typename IntType>
void Driver::getInterruptVarsForInterface(std::vector<DeviceVariable *> &dest,
                                          int canInterrupt, void *ifacePvt) {
    (void)canInterrupt;
    ELLLIST *clients;
    pasynManager->interruptStart(ifacePvt, &clients);
    ELLNODE *node = ellFirst(clients);
    while (node) {
        interruptNode *inode = reinterpret_cast<interruptNode *>(node);
        IntType *interrupt = static_cast<IntType *>(inode->drvPvt);
        if (hasParam(interrupt->pasynUser->reason)) {
            dest.push_back(deviceVariableFromUser(interrupt->pasynUser));
        }
        node = ellNext(node);
    }
    pasynManager->interruptEnd(ifacePvt);
}

std::vector<DeviceVariable *> Driver::getInterruptVariables() {
    std::vector<DeviceVariable *> vars;

    asynStandardInterfaces *ifcs = getAsynStdInterfaces();
    getInterruptVarsForInterface<asynOctetInterrupt>(
        vars, ifcs->octetCanInterrupt, ifcs->octetInterruptPvt);
    getInterruptVarsForInterface<asynUInt32DigitalInterrupt>(
        vars, ifcs->uInt32DigitalCanInterrupt, ifcs->uInt32DigitalInterruptPvt);
    getInterruptVarsForInterface<asynInt32Interrupt>(
        vars, ifcs->int32CanInterrupt, ifcs->int32InterruptPvt);
    getInterruptVarsForInterface<asynInt64Interrupt>(
        vars, ifcs->int64CanInterrupt, ifcs->int64InterruptPvt);
    getInterruptVarsForInterface<asynFloat64Interrupt>(
        vars, ifcs->float64CanInterrupt, ifcs->float64InterruptPvt);
    getInterruptVarsForInterface<asynInt8ArrayInterrupt>(
        vars, ifcs->int8ArrayCanInterrupt, ifcs->int8ArrayInterruptPvt);
    getInterruptVarsForInterface<asynInt16ArrayInterrupt>(
        vars, ifcs->int16ArrayCanInterrupt, ifcs->int16ArrayInterruptPvt);
    getInterruptVarsForInterface<asynInt32ArrayInterrupt>(
        vars, ifcs->int32ArrayCanInterrupt, ifcs->int32ArrayInterruptPvt);
    getInterruptVarsForInterface<asynInt64ArrayInterrupt>(
        vars, ifcs->int64ArrayCanInterrupt, ifcs->int64ArrayInterruptPvt);
    getInterruptVarsForInterface<asynFloat32ArrayInterrupt>(
        vars, ifcs->float32ArrayCanInterrupt, ifcs->float32ArrayInterruptPvt);
    getInterruptVarsForInterface<asynFloat64ArrayInterrupt>(
        vars, ifcs->float64ArrayCanInterrupt, ifcs->float64ArrayInterruptPvt);

    // The list contains all records, so we need to remove duplicates.
    std::sort(vars.begin(), vars.end());
    vars.erase(std::unique(vars.begin(), vars.end()), vars.end());

    return vars;
}

template <typename Ptr, typename Other>
static void assignPtr(Ptr *&ptr, Other *other) {
    ptr = reinterpret_cast<Ptr *>(other);
}

template <typename Iface, typename HType>
void Driver::installAnInterruptRegistrar(void *piface) {
    // I hate doing type erasure like this, but there aren't sane options ...
    Iface *iface = static_cast<Iface *>(piface);
    VoidFuncPtr reg =
        reinterpret_cast<VoidFuncPtr>(iface->registerInterruptUser);
    VoidFuncPtr canc =
        reinterpret_cast<VoidFuncPtr>(iface->cancelInterruptUser);
    m_originalIntrRegister[AsynType<HType>::value] = std::make_pair(reg, canc);
    assignPtr(iface->cancelInterruptUser, Driver::cancelInterrupt<HType>);
    assignPtr(iface->registerInterruptUser, Driver::registerInterrupt<HType>);
}

// UInt32Digital has a signature different from other registrars, so we need to
// do a different cast.
template <>
void Driver::installAnInterruptRegistrar<asynUInt32Digital, epicsUInt32>(
    void *piface) {
    // I hate doing type erasure like this, but there aren't sane options ...
    asynUInt32Digital *iface = static_cast<asynUInt32Digital *>(piface);
    VoidFuncPtr reg =
        reinterpret_cast<VoidFuncPtr>(iface->registerInterruptUser);
    VoidFuncPtr canc =
        reinterpret_cast<VoidFuncPtr>(iface->cancelInterruptUser);
    m_originalIntrRegister[AsynType<epicsUInt32>::value] =
        std::make_pair(reg, canc);
    assignPtr(iface->cancelInterruptUser, Driver::cancelInterrupt<epicsUInt32>);
    assignPtr(iface->registerInterruptUser, Driver::registerInterruptDigital);
}

void Driver::installInterruptRegistrars() {
    asynStandardInterfaces *ifcs = getAsynStdInterfaces();
    installAnInterruptRegistrar<asynInt32, epicsInt32>(ifcs->int32.pinterface);
    installAnInterruptRegistrar<asynInt64, epicsInt64>(ifcs->int64.pinterface);
    installAnInterruptRegistrar<asynFloat64, epicsFloat64>(
        ifcs->float64.pinterface);
    installAnInterruptRegistrar<asynOctet, Octet>(ifcs->octet.pinterface);
    installAnInterruptRegistrar<asynUInt32Digital, epicsUInt32>(
        ifcs->uInt32Digital.pinterface);
    installAnInterruptRegistrar<asynInt8Array, Array<epicsInt8> >(
        ifcs->int8Array.pinterface);
    installAnInterruptRegistrar<asynInt16Array, Array<epicsInt16> >(
        ifcs->int16Array.pinterface);
    installAnInterruptRegistrar<asynInt32Array, Array<epicsInt32> >(
        ifcs->int32Array.pinterface);
    installAnInterruptRegistrar<asynInt64Array, Array<epicsInt64> >(
        ifcs->int64Array.pinterface);
    installAnInterruptRegistrar<asynFloat32Array, Array<epicsFloat32> >(
        ifcs->float32Array.pinterface);
    installAnInterruptRegistrar<asynFloat64Array, Array<epicsFloat64> >(
        ifcs->float64Array.pinterface);
}

template <typename T>
typename Handlers<T>::ReadHandler
Driver::getReadHandler(std::string const &function) {
    typename Handlers<T>::ReadHandler handler;
    try {
        handler = getHandlerMap<T>().at(function).readHandler;
    } catch (std::out_of_range const &) {
        handler = NULL;
    }
    return handler;
}

template <typename T>
typename Handlers<T>::WriteHandler
Driver::getWriteHandler(std::string const &function) {
    typename Handlers<T>::WriteHandler handler;
    try {
        handler = getHandlerMap<T>().at(function).writeHandler;
    } catch (std::out_of_range const &) {
        handler = NULL;
    }
    return handler;
}

template <typename T>
bool Driver::checkHandlersVerbosely(std::string const &function) {
    try {
        getHandlerMap<T>().at(function);
    } catch (std::out_of_range &) {
        std::stringstream msg;
        msg << "record of DTYP " << getDtypName(AsynType<T>::value)
            << " cannot handle function " << function << ". ";

        try {
            asynParamType type = m_functionTypes.at(function);
            msg << "Perhaps you meant DTYP = " << getDtypName(type) << "?\n";
        } catch (std::out_of_range &) {
            msg << "No other DTYP can handle this either.\n";
        }

        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s: port=%s %s.",
                  driverName, portName, msg.str().c_str());
        return false;
    }
    return true;
}

bool Driver::hasParam(int index) {
    return m_params.find(index) != m_params.end();
}

template <typename T> bool Driver::hasReadHandler(int index) {
    return getReadHandler<T>(m_params.at(index)->function()) != NULL;
}

template <typename T> bool Driver::hasWriteHandler(int index) {
    return getWriteHandler<T>(m_params.at(index)->function()) != NULL;
}

bool Driver::shouldProcessInterrupts(WriteResult const &result) const {
    return result.status == asynSuccess &&
           (result.processInterrupts == ProcessInterrupts::ON ||
            (result.processInterrupts == ProcessInterrupts::DEFAULT &&
             opts.autoInterrupts));
}

bool Driver::shouldProcessInterrupts(ResultBase const &result) const {
    return result.status == asynSuccess &&
           result.processInterrupts == ProcessInterrupts::ON;
}

template <>
asynStatus Driver::setParamDispatch<epicsInt32>(int index, epicsInt32 value) {
    return setIntegerParam(index, value);
}

template <>
asynStatus Driver::setParamDispatch<epicsInt64>(int index, epicsInt64 value) {
    return setInteger64Param(index, value);
}

template <>
asynStatus Driver::setParamDispatch<epicsFloat64>(int index,
                                                  epicsFloat64 value) {
    return setDoubleParam(index, value);
}

template <> asynStatus Driver::setParamDispatch<Octet>(int index, Octet value) {
    return setStringParam(index, value.data());
}

template <>
asynStatus
Driver::doCallbacksArrayDispatch<epicsInt8>(int index,
                                            Array<epicsInt8> &value) {
    return asynPortDriver::doCallbacksInt8Array(value.data(), value.size(),
                                                index, 0);
}

template <>
asynStatus
Driver::doCallbacksArrayDispatch<epicsInt16>(int index,
                                             Array<epicsInt16> &value) {
    return asynPortDriver::doCallbacksInt16Array(value.data(), value.size(),
                                                 index, 0);
}

template <>
asynStatus
Driver::doCallbacksArrayDispatch<epicsInt32>(int index,
                                             Array<epicsInt32> &value) {
    return asynPortDriver::doCallbacksInt32Array(value.data(), value.size(),
                                                 index, 0);
}

template <>
asynStatus
Driver::doCallbacksArrayDispatch<epicsInt64>(int index,
                                             Array<epicsInt64> &value) {
    return asynPortDriver::doCallbacksInt64Array(value.data(), value.size(),
                                                 index, 0);
}

template <>
asynStatus
Driver::doCallbacksArrayDispatch<epicsFloat32>(int index,
                                               Array<epicsFloat32> &value) {
    return asynPortDriver::doCallbacksFloat32Array(value.data(), value.size(),
                                                   index, 0);
}

template <>
asynStatus
Driver::doCallbacksArrayDispatch<epicsFloat64>(int index,
                                               Array<epicsFloat64> &value) {
    return asynPortDriver::doCallbacksFloat64Array(value.data(), value.size(),
                                                   index, 0);
}

template <>
std::map<std::string, Handlers<epicsInt32> > &
Driver::getHandlerMap<epicsInt32>() {
    return m_Int32HandlerMap;
}

template <>
std::map<std::string, Handlers<epicsInt64> > &
Driver::getHandlerMap<epicsInt64>() {
    return m_Int64HandlerMap;
}

template <>
std::map<std::string, Handlers<epicsUInt32> > &
Driver::getHandlerMap<epicsUInt32>() {
    return m_UInt32HandlerMap;
}

template <>
std::map<std::string, Handlers<epicsFloat64> > &
Driver::getHandlerMap<epicsFloat64>() {
    return m_Float64HandlerMap;
}

template <>
std::map<std::string, Handlers<Octet> > &Driver::getHandlerMap<Octet>() {
    return m_OctetHandlerMap;
}

template <>
std::map<std::string, Handlers<Array<epicsInt8> > > &
Driver::getHandlerMap<Array<epicsInt8> >() {
    return m_Int8ArrayHandlerMap;
}

template <>
std::map<std::string, Handlers<Array<epicsInt16> > > &
Driver::getHandlerMap<Array<epicsInt16> >() {
    return m_Int16ArrayHandlerMap;
}

template <>
std::map<std::string, Handlers<Array<epicsInt32> > > &
Driver::getHandlerMap<Array<epicsInt32> >() {
    return m_Int32ArrayHandlerMap;
}

template <>
std::map<std::string, Handlers<Array<epicsInt64> > > &
Driver::getHandlerMap<Array<epicsInt64> >() {
    return m_Int64ArrayHandlerMap;
}

template <>
std::map<std::string, Handlers<Array<epicsFloat32> > > &
Driver::getHandlerMap<Array<epicsFloat32> >() {
    return m_Float32ArrayHandlerMap;
}

template <>
std::map<std::string, Handlers<Array<epicsFloat64> > > &
Driver::getHandlerMap<Array<epicsFloat64> >() {
    return m_Float64ArrayHandlerMap;
}

template <typename T>
void Driver::registerHandlers(std::string const &function,
                              typename Handlers<T>::ReadHandler reader,
                              typename Handlers<T>::WriteHandler writer,
                              InterruptRegistrar intrRegistrar) {
    if (m_functionTypes.find(function) != m_functionTypes.end() &&
        m_functionTypes[function] != Handlers<T>::type) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: port=%s function %s already has handlers for type "
                  "%s, can't register another for type %s\n",
                  driverName, portName, function.c_str(),
                  getAsynTypeName(m_functionTypes[function]),
                  getAsynTypeName(AsynType<T>::value));
        return;
    }

    getHandlerMap<T>()[function].readHandler = reader;
    getHandlerMap<T>()[function].writeHandler = writer;
    getHandlerMap<T>()[function].intrRegistrar = intrRegistrar;
    m_functionTypes[function] = Handlers<T>::type;
}

template void
Driver::registerHandlers<epicsInt32>(std::string const &function,
                                     Handlers<epicsInt32>::ReadHandler reader,
                                     Handlers<epicsInt32>::WriteHandler writer,
                                     InterruptRegistrar intrRegistrar);
template void
Driver::registerHandlers<epicsInt64>(std::string const &function,
                                     Handlers<epicsInt64>::ReadHandler reader,
                                     Handlers<epicsInt64>::WriteHandler writer,
                                     InterruptRegistrar intrRegistrar);
template void Driver::registerHandlers<epicsFloat64>(
    std::string const &function, Handlers<epicsFloat64>::ReadHandler reader,
    Handlers<epicsFloat64>::WriteHandler writer,
    InterruptRegistrar intrRegistrar);
template void Driver::registerHandlers<epicsUInt32>(
    std::string const &function, Handlers<epicsUInt32>::ReadHandler reader,
    Handlers<epicsUInt32>::WriteHandler writer,
    InterruptRegistrar intrRegistrar);
template void Driver::registerHandlers<Octet>(
    std::string const &function, Handlers<Octet>::ReadHandler reader,
    Handlers<Octet>::WriteHandler writer, InterruptRegistrar intrRegistrar);
template void Driver::registerHandlers<Array<epicsInt8> >(
    std::string const &function,
    Handlers<Array<epicsInt8> >::ReadHandler reader,
    Handlers<Array<epicsInt8> >::WriteHandler writer,
    InterruptRegistrar intrRegistrar);
template void Driver::registerHandlers<Array<epicsInt16> >(
    std::string const &function,
    Handlers<Array<epicsInt16> >::ReadHandler reader,
    Handlers<Array<epicsInt16> >::WriteHandler writer,
    InterruptRegistrar intrRegistrar);
template void Driver::registerHandlers<Array<epicsInt32> >(
    std::string const &function,
    Handlers<Array<epicsInt32> >::ReadHandler reader,
    Handlers<Array<epicsInt32> >::WriteHandler writer,
    InterruptRegistrar intrRegistrar);
template void Driver::registerHandlers<Array<epicsInt64> >(
    std::string const &function,
    Handlers<Array<epicsInt64> >::ReadHandler reader,
    Handlers<Array<epicsInt64> >::WriteHandler writer,
    InterruptRegistrar intrRegistrar);
template void Driver::registerHandlers<Array<epicsFloat32> >(
    std::string const &function,
    Handlers<Array<epicsFloat32> >::ReadHandler reader,
    Handlers<Array<epicsFloat32> >::WriteHandler writer,
    InterruptRegistrar intrRegistrar);
template void Driver::registerHandlers<Array<epicsFloat64> >(
    std::string const &function,
    Handlers<Array<epicsFloat64> >::ReadHandler reader,
    Handlers<Array<epicsFloat64> >::WriteHandler writer,
    InterruptRegistrar intrRegistrar);

template <typename T>
asynStatus Driver::doCallbacksArray(DeviceVariable const &var, Array<T> &value,
                                    asynStatus status, int alarmStatus,
                                    int alarmSeverity) {
    setParamStatus(var.asynIndex(), status);
    setParamAlarmStatus(var.asynIndex(), alarmStatus);
    setParamAlarmSeverity(var.asynIndex(), alarmSeverity);
    return doCallbacksArrayDispatch(var.asynIndex(), value);
}

template asynStatus
Driver::doCallbacksArray<epicsInt8>(DeviceVariable const &var,
                                    Array<epicsInt8> &value, asynStatus status,
                                    int alarmStatus, int alarmSeverity);
template asynStatus Driver::doCallbacksArray<epicsInt16>(
    DeviceVariable const &var, Array<epicsInt16> &value, asynStatus status,
    int alarmStatus, int alarmSeverity);
template asynStatus Driver::doCallbacksArray<epicsInt32>(
    DeviceVariable const &var, Array<epicsInt32> &value, asynStatus status,
    int alarmStatus, int alarmSeverity);
template asynStatus Driver::doCallbacksArray<epicsInt64>(
    DeviceVariable const &var, Array<epicsInt64> &value, asynStatus status,
    int alarmStatus, int alarmSeverity);
template asynStatus Driver::doCallbacksArray<epicsFloat32>(
    DeviceVariable const &var, Array<epicsFloat32> &value, asynStatus status,
    int alarmStatus, int alarmSeverity);
template asynStatus Driver::doCallbacksArray<epicsFloat64>(
    DeviceVariable const &var, Array<epicsFloat64> &value, asynStatus status,
    int alarmStatus, int alarmSeverity);

template <typename T>
asynStatus Driver::setParam(DeviceVariable const &var, T value,
                            asynStatus status, int alarmStatus,
                            int alarmSeverity) {
    setParamStatus(var.asynIndex(), status);
    setParamAlarmStatus(var.asynIndex(), alarmStatus);
    setParamAlarmSeverity(var.asynIndex(), alarmSeverity);
    return setParamDispatch(var.asynIndex(), value);
}

asynStatus Driver::setParam(DeviceVariable const &var, epicsUInt32 value,
                            epicsUInt32 mask, asynStatus status,
                            int alarmStatus, int alarmSeverity) {
    setParamStatus(var.asynIndex(), status);
    setParamAlarmStatus(var.asynIndex(), alarmStatus);
    setParamAlarmSeverity(var.asynIndex(), alarmSeverity);
    return setUIntDigitalParam(var.asynIndex(), value, mask);
}

template asynStatus Driver::setParam<epicsInt32>(DeviceVariable const &var,
                                                 epicsInt32 value,
                                                 asynStatus status,
                                                 int alarmStatus,
                                                 int alarmSeverity);
template asynStatus Driver::setParam<epicsInt64>(DeviceVariable const &var,
                                                 epicsInt64 value,
                                                 asynStatus status,
                                                 int alarmStatus,
                                                 int alarmSeverity);
template asynStatus Driver::setParam<epicsFloat64>(DeviceVariable const &var,
                                                   epicsFloat64 value,
                                                   asynStatus status,
                                                   int alarmStatus,
                                                   int alarmSeverity);
template asynStatus Driver::setParam<Octet>(DeviceVariable const &var,
                                            Octet value, asynStatus status,
                                            int alarmStatus, int alarmSeverity);

template <>
asynStatus Driver::setParam<epicsUInt32>(DeviceVariable const &var,
                                         epicsUInt32 value, asynStatus status,
                                         int alarmStatus, int alarmSeverity) {
    return setParam(var, value, 0xffffffff, status, alarmStatus, alarmSeverity);
}

template <typename T>
asynStatus Driver::registerInterrupt(void *drvPvt, asynUser *pasynUser,
                                     void *callback, void *userPvt,
                                     void **registrarPvt) {
    Driver *self = static_cast<Driver *>(drvPvt);
    DeviceVariable *var = self->deviceVariableFromUser(pasynUser);

    // I hate doing type erasure like this, but there aren't sane options ...
    typedef asynStatus (*RegisterIntrFunc)(void *drvPvt, asynUser *pasynUser,
                                           void *callback, void *userPvt,
                                           void **registrarPvt);
    RegisterIntrFunc original = reinterpret_cast<RegisterIntrFunc>(
        self->m_originalIntrRegister.at(AsynType<T>::value).first);
    asynStatus status =
        original(drvPvt, pasynUser, callback, userPvt, registrarPvt);
    if (status != asynSuccess) {
        return status;
    }

    self->m_interruptRefcount[var] += 1;

    if (self->m_interruptRefcount[var] == 1) {
        if (!self->checkHandlersVerbosely<T>(var->function())) {
            return asynError;
        }
        InterruptRegistrar registrar =
            self->getHandlerMap<T>().at(var->function()).intrRegistrar;
        if (registrar != NULL) {
            asynPrint(self->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s: port=%s registering interrupt handler for '%s'\n",
                      driverName, self->portName, var->asString().c_str());
            status = registrar(*var, false);
            if (status != asynSuccess) {
                asynPrint(
                    self->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s: port=%s error %d calling interrupt registrar for "
                    "'%s'\n",
                    driverName, self->portName, status,
                    var->asString().c_str());
                return status;
            }
        }
    }

    return status;
}

template <typename T>
asynStatus Driver::cancelInterrupt(void *drvPvt, asynUser *pasynUser,
                                   void *registrarPvt) {
    Driver *self = static_cast<Driver *>(drvPvt);
    DeviceVariable *var = self->deviceVariableFromUser(pasynUser);

    // I hate doing type erasure like this, but there aren't sane options ...
    typedef asynStatus (*CancelIntrFunc)(void *drvPvt, asynUser *pasynUser,
                                         void *registrarPvt);
    CancelIntrFunc original = reinterpret_cast<CancelIntrFunc>(
        self->m_originalIntrRegister.at(AsynType<T>::value).second);
    asynStatus status = original(drvPvt, pasynUser, registrarPvt);
    if (status != asynSuccess) {
        return status;
    }

    self->m_interruptRefcount[var] -= 1;

    if (self->m_interruptRefcount[var] < 0) {
        asynPrint(self->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: port=%s logic error: interrupt refcount negative for"
                  "'%s'\n",
                  driverName, self->portName, var->asString().c_str());
        self->m_interruptRefcount[var] = 0;
        return asynError;
    }

    if (self->m_interruptRefcount[var] == 0) {
        if (!self->checkHandlersVerbosely<T>(var->function())) {
            return asynError;
        }
        InterruptRegistrar registrar =
            self->getHandlerMap<T>().at(var->function()).intrRegistrar;
        if (registrar != NULL) {
            asynPrint(self->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s: port=%s cancelling interrupt handler for '%s'\n",
                      driverName, self->portName, var->asString().c_str());
            status = registrar(*var, true);
            if (status != asynSuccess) {
                asynPrint(
                    self->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s: port=%s error %d calling interrupt registrar for "
                    "'%s'\n",
                    driverName, self->portName, status,
                    var->asString().c_str());
                return status;
            }
        }
    }

    return status;
}

asynStatus Driver::registerInterruptDigital(void *drvPvt, asynUser *pasynUser,
                                            void *callback, void *userPvt,
                                            epicsUInt32 mask,
                                            void **registrarPvt) {
    typedef epicsUInt32 T;
    Driver *self = static_cast<Driver *>(drvPvt);
    DeviceVariable *var = self->deviceVariableFromUser(pasynUser);

    // UInt32Digital has a signature different from other registrars.
    typedef asynStatus (*RegisterIntrFunc)(
        void *drvPvt, asynUser *pasynUser, void *callback, void *userPvt,
        epicsUInt32 mask, void **registrarPvt);
    RegisterIntrFunc original = reinterpret_cast<RegisterIntrFunc>(
        self->m_originalIntrRegister.at(AsynType<T>::value).first);
    asynStatus status =
        original(drvPvt, pasynUser, callback, userPvt, mask, registrarPvt);
    if (status != asynSuccess) {
        return status;
    }

    self->m_interruptRefcount[var] += 1;

    if (self->m_interruptRefcount[var] == 1) {
        if (!self->checkHandlersVerbosely<T>(var->function())) {
            return asynError;
        }
        InterruptRegistrar registrar =
            self->getHandlerMap<T>().at(var->function()).intrRegistrar;
        if (registrar != NULL) {
            asynPrint(self->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s: port=%s registering interrupt handler for '%s'\n",
                      driverName, self->portName, var->asString().c_str());
            status = registrar(*var, false);
            if (status != asynSuccess) {
                asynPrint(
                    self->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s: port=%s error %d calling interrupt registrar for "
                    "'%s'\n",
                    driverName, self->portName, status,
                    var->asString().c_str());
                return status;
            }
        }
    }

    return status;
}

template <typename T>
asynStatus Driver::readScalar(asynUser *pasynUser, T *value) {
    DeviceVariable *var = deviceVariableFromUser(pasynUser);
    typename Handlers<T>::ReadHandler handler =
        getReadHandler<T>(var->function());
    typename Handlers<T>::ReadResult result = handler(*var);
    handleResultStatus(pasynUser, result);
    *value = result.value;
    if (shouldProcessInterrupts(result)) {
        setParamDispatch(pasynUser->reason, result.value);
        callParamCallbacks();
    }
    return result.status;
}

asynStatus Driver::readScalar(asynUser *pasynUser, epicsUInt32 *value,
                              epicsUInt32 mask) {
    DeviceVariable *var = deviceVariableFromUser(pasynUser);
    Handlers<epicsUInt32>::ReadHandler handler =
        getReadHandler<epicsUInt32>(var->function());
    Handlers<epicsUInt32>::ReadResult result = handler(*var, mask);
    handleResultStatus(pasynUser, result);
    *value = result.value;
    if (shouldProcessInterrupts(result)) {
        setUIntDigitalParam(pasynUser->reason, result.value, mask);
        callParamCallbacks();
    }
    return result.status;
}

template <typename T>
asynStatus Driver::writeScalar(asynUser *pasynUser, T value) {
    DeviceVariable *var = deviceVariableFromUser(pasynUser);
    typename Handlers<T>::WriteHandler handler =
        getWriteHandler<T>(var->function());
    typename Handlers<T>::WriteResult result = handler(*var, value);
    handleResultStatus(pasynUser, result);
    if (shouldProcessInterrupts(result)) {
        setParamDispatch(pasynUser->reason, value);
        callParamCallbacks();
    }
    return result.status;
}

asynStatus Driver::writeScalar(asynUser *pasynUser, epicsUInt32 value,
                               epicsUInt32 mask) {
    DeviceVariable *var = deviceVariableFromUser(pasynUser);
    Handlers<epicsUInt32>::WriteHandler handler =
        getWriteHandler<epicsUInt32>(var->function());
    Handlers<epicsUInt32>::WriteResult result = handler(*var, value, mask);
    handleResultStatus(pasynUser, result);
    if (shouldProcessInterrupts(result)) {
        setUIntDigitalParam(pasynUser->reason, value, mask);
        callParamCallbacks();
    }
    return result.status;
}

template <typename T>
asynStatus Driver::readArray(asynUser *pasynUser, T *value, size_t maxSize,
                             size_t *size) {
    DeviceVariable *var = deviceVariableFromUser(pasynUser);
    Array<T> arrayRef(value, maxSize);
    typename Handlers<Array<T> >::ReadHandler handler =
        getHandlerMap<Array<T> >().at(var->function()).readHandler;
    typename Handlers<Array<T> >::ReadResult result = handler(*var, arrayRef);
    handleResultStatus(pasynUser, result);
    *size = arrayRef.size();
    if (shouldProcessInterrupts(result)) {
        return doCallbacksArrayDispatch(var->asynIndex(), arrayRef);
    }
    return result.status;
}

template <typename T>
asynStatus Driver::writeArray(asynUser *pasynUser, T *value, size_t size) {
    DeviceVariable *var = deviceVariableFromUser(pasynUser);
    Array<T> arrayRef(value, size);
    typename Handlers<Array<T> >::WriteHandler handler =
        getHandlerMap<Array<T> >().at(var->function()).writeHandler;
    typename Handlers<Array<T> >::WriteResult result = handler(*var, arrayRef);
    handleResultStatus(pasynUser, result);
    if (shouldProcessInterrupts(result)) {
        return doCallbacksArrayDispatch(var->asynIndex(), arrayRef);
    }
    return result.status;
}

asynStatus Driver::readOctetData(asynUser *pasynUser, char *value,
                                 size_t maxSize, size_t *nRead) {
    DeviceVariable *var = deviceVariableFromUser(pasynUser);
    Octet arrayRef(value, maxSize);
    Handlers<Octet>::ReadHandler handler =
        getHandlerMap<Octet>().at(var->function()).readHandler;
    Handlers<Octet>::ReadResult result = handler(*var, arrayRef);
    handleResultStatus(pasynUser, result);
    *nRead = arrayRef.size();
    // The handler should have ensured termination, but we can't be sure.
    arrayRef.terminate();
    if (shouldProcessInterrupts(result)) {
        setParamDispatch(var->asynIndex(), arrayRef);
        callParamCallbacks();
    }
    return result.status;
}

asynStatus Driver::writeOctetData(asynUser *pasynUser, char const *value,
                                  size_t size) {
    DeviceVariable *var = deviceVariableFromUser(pasynUser);
    Octet const arrayRef(const_cast<char *>(value), size);
    Handlers<Octet>::WriteHandler handler =
        getHandlerMap<Octet>().at(var->function()).writeHandler;
    Handlers<Octet>::WriteResult result = handler(*var, arrayRef);
    handleResultStatus(pasynUser, result);
    if (shouldProcessInterrupts(result)) {
        setParamDispatch(var->asynIndex(), arrayRef);
        callParamCallbacks();
    }
    return result.status;
}

asynStatus Driver::readInt32(asynUser *pasynUser, epicsInt32 *value) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<epicsInt32>(pasynUser->reason)) {
        return asynPortDriver::readInt32(pasynUser, value);
    }
    return readScalar(pasynUser, value);
}

asynStatus Driver::writeInt32(asynUser *pasynUser, epicsInt32 value) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<epicsInt32>(pasynUser->reason)) {
        return asynPortDriver::writeInt32(pasynUser, value);
    }
    return writeScalar(pasynUser, value);
}

asynStatus Driver::readInt64(asynUser *pasynUser, epicsInt64 *value) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<epicsInt64>(pasynUser->reason)) {
        return asynPortDriver::readInt64(pasynUser, value);
    }
    return readScalar(pasynUser, value);
}

asynStatus Driver::writeInt64(asynUser *pasynUser, epicsInt64 value) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<epicsInt64>(pasynUser->reason)) {
        return asynPortDriver::writeInt64(pasynUser, value);
    }
    return writeScalar(pasynUser, value);
}

asynStatus Driver::readFloat64(asynUser *pasynUser, epicsFloat64 *value) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<epicsFloat64>(pasynUser->reason)) {
        return asynPortDriver::readFloat64(pasynUser, value);
    }
    return readScalar(pasynUser, value);
}

asynStatus Driver::writeFloat64(asynUser *pasynUser, epicsFloat64 value) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<epicsFloat64>(pasynUser->reason)) {
        return asynPortDriver::writeFloat64(pasynUser, value);
    }
    return writeScalar(pasynUser, value);
}

asynStatus Driver::readInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                 size_t maxSize, size_t *size) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<Array<epicsInt8> >(pasynUser->reason)) {
        return asynPortDriver::readInt8Array(pasynUser, value, maxSize, size);
    }
    return readArray(pasynUser, value, maxSize, size);
}

asynStatus Driver::writeInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                  size_t size) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<Array<epicsInt8> >(pasynUser->reason)) {
        return asynPortDriver::writeInt8Array(pasynUser, value, size);
    }
    return writeArray(pasynUser, value, size);
}

asynStatus Driver::readInt16Array(asynUser *pasynUser, epicsInt16 *value,
                                  size_t maxSize, size_t *size) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<Array<epicsInt16> >(pasynUser->reason)) {
        return asynPortDriver::readInt16Array(pasynUser, value, maxSize, size);
    }
    return readArray(pasynUser, value, maxSize, size);
}

asynStatus Driver::writeInt16Array(asynUser *pasynUser, epicsInt16 *value,
                                   size_t size) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<Array<epicsInt16> >(pasynUser->reason)) {
        return asynPortDriver::writeInt16Array(pasynUser, value, size);
    }
    return writeArray(pasynUser, value, size);
}

asynStatus Driver::readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                  size_t maxSize, size_t *size) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<Array<epicsInt32> >(pasynUser->reason)) {
        return asynPortDriver::readInt32Array(pasynUser, value, maxSize, size);
    }
    return readArray(pasynUser, value, maxSize, size);
}

asynStatus Driver::writeInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                   size_t size) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<Array<epicsInt32> >(pasynUser->reason)) {
        return asynPortDriver::writeInt32Array(pasynUser, value, size);
    }
    return writeArray(pasynUser, value, size);
}

asynStatus Driver::readInt64Array(asynUser *pasynUser, epicsInt64 *value,
                                  size_t maxSize, size_t *size) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<Array<epicsInt64> >(pasynUser->reason)) {
        return asynPortDriver::readInt64Array(pasynUser, value, maxSize, size);
    }
    return readArray(pasynUser, value, maxSize, size);
}

asynStatus Driver::writeInt64Array(asynUser *pasynUser, epicsInt64 *value,
                                   size_t size) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<Array<epicsInt64> >(pasynUser->reason)) {
        return asynPortDriver::writeInt64Array(pasynUser, value, size);
    }
    return writeArray(pasynUser, value, size);
}

asynStatus Driver::readFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
                                    size_t maxSize, size_t *size) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<Array<epicsFloat32> >(pasynUser->reason)) {
        return asynPortDriver::readFloat32Array(pasynUser, value, maxSize,
                                                size);
    }
    return readArray(pasynUser, value, maxSize, size);
}

asynStatus Driver::writeFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
                                     size_t size) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<Array<epicsFloat32> >(pasynUser->reason)) {
        return asynPortDriver::writeFloat32Array(pasynUser, value, size);
    }
    return writeArray(pasynUser, value, size);
}

asynStatus Driver::readFloat64Array(asynUser *pasynUser, epicsFloat64 *value,
                                    size_t maxSize, size_t *size) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<Array<epicsFloat64> >(pasynUser->reason)) {
        return asynPortDriver::readFloat64Array(pasynUser, value, maxSize,
                                                size);
    }
    return readArray(pasynUser, value, maxSize, size);
}

asynStatus Driver::writeFloat64Array(asynUser *pasynUser, epicsFloat64 *value,
                                     size_t size) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<Array<epicsFloat64> >(pasynUser->reason)) {
        return asynPortDriver::writeFloat64Array(pasynUser, value, size);
    }
    return writeArray(pasynUser, value, size);
}

asynStatus Driver::readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value,
                                     epicsUInt32 mask) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<epicsUInt32>(pasynUser->reason)) {
        return asynPortDriver::readUInt32Digital(pasynUser, value, mask);
    }
    return readScalar(pasynUser, value, mask);
}

asynStatus Driver::writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value,
                                      epicsUInt32 mask) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<epicsUInt32>(pasynUser->reason)) {
        return asynPortDriver::writeUInt32Digital(pasynUser, value, mask);
    }
    return writeScalar(pasynUser, value, mask);
}

asynStatus Driver::readOctet(asynUser *pasynUser, char *value, size_t nChars,
                             size_t *nActual, int *eomReason) {
    if (!hasParam(pasynUser->reason) ||
        !hasReadHandler<Octet>(pasynUser->reason)) {
        return asynPortDriver::readOctet(pasynUser, value, nChars, nActual,
                                         eomReason);
    }
    // Only complete reads are supported.
    *eomReason = ASYN_EOM_END;
    return readOctetData(pasynUser, value, nChars, nActual);
}

asynStatus Driver::writeOctet(asynUser *pasynUser, const char *value,
                              size_t nChars, size_t *nActual) {
    if (!hasParam(pasynUser->reason) ||
        !hasWriteHandler<Octet>(pasynUser->reason)) {
        return asynPortDriver::writeOctet(pasynUser, value, nChars, nActual);
    }
    // Only complete writes are supported.
    *nActual = nChars;
    return writeOctetData(pasynUser, value, nChars);
}

const asynParamType AsynType<epicsInt32>::value;
const asynParamType AsynType<epicsInt64>::value;
const asynParamType AsynType<epicsFloat64>::value;
const asynParamType AsynType<epicsUInt32>::value;
const asynParamType AsynType<Octet>::value;
const asynParamType AsynType<Array<epicsInt8> >::value;
const asynParamType AsynType<Array<epicsInt16> >::value;
const asynParamType AsynType<Array<epicsInt32> >::value;
const asynParamType AsynType<Array<epicsInt64> >::value;
const asynParamType AsynType<Array<epicsFloat32> >::value;
const asynParamType AsynType<Array<epicsFloat64> >::value;

} // namespace Autoparam
