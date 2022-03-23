// SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
//
// SPDX-License-Identifier: MIT

#include "autoparamDriver.h"

#include <errlog.h>
#include <epicsExit.h>
#include <initHooks.h>

#include <algorithm>

namespace Autoparam {

static char const *driverName = "Autoparam::Driver";

static std::map<Driver *, DriverOpts::InitHook> allInitHooks;

static char const *skipSpaces(char const *cursor) {
    while (*cursor != 0 && *cursor == ' ') {
        ++cursor;
    }
    return cursor;
}

static char const *findSpace(char const *cursor) {
    while (*cursor != 0 && *cursor != ' ') {
        ++cursor;
    }
    return cursor;
}

PVInfo::PVInfo(char const *asynReason) {
    // TODO escaping spaces, quoting. Perhaps this shouldn't be allowed at all,
    // and we should simply go with JSON if we ever need that.

    // Skip any initial spaces.
    char const *curr = skipSpaces(asynReason);
    if (*curr == 0) {
        return;
    }

    // Find first space; this determines the function.
    char const *funcEnd = findSpace(curr);
    m_function = std::string(curr, funcEnd);
    curr = skipSpaces(funcEnd);

    // Now let's collect the arguments by jumping over consecutive spaces.
    while (*curr != 0) {
        if (*curr == '{' || *curr == '[') {
            errlogPrintf("Autoparam::PVInfo: error parsing '%s', arguments may "
                         "not start with a curly brace or square bracket\n",
                         asynReason);
            m_function = std::string();
            m_arguments = ArgumentList();
            return;
        }
        char const *argEnd = findSpace(curr);
        m_arguments.push_back(std::string(curr, argEnd));
        curr = skipSpaces(argEnd);
    }
}

PVInfo::PVInfo(PVInfo const &other) { *this = other; }

PVInfo &PVInfo::operator=(PVInfo const &other) {
    m_asynParamIndex = other.m_asynParamIndex;
    m_asynParamType = other.m_asynParamType;
    m_function = other.m_function;
    m_arguments = other.m_arguments;
    return *this;
}

PVInfo::~PVInfo() {
    // Nothing to do here.
}

std::string PVInfo::normalized() const {
    std::string norm(function());
    ArgumentList const &args = arguments();
    for (ArgumentList::const_iterator i = args.begin(), end = args.end();
         i != end; ++i) {
        norm += ' ';
        norm += *i;
    }
    return norm;
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

static void destroyDriver(void *driver) {
    Driver *drv = static_cast<Driver *>(driver);
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

asynStatus Driver::drvUserCreate(asynUser *pasynUser, const char *reason,
                                 const char **, size_t *) {
    PVInfo parsed(reason);
    if (parsed.function().empty()) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: port=%s empty reason '%s'\n", driverName, portName,
                  reason);
        return asynError;
    }

    std::string normalized = parsed.normalized();
    asynParamType type;
    try {
        type = m_functionTypes.at(parsed.function());
    } catch (std::out_of_range const &) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: port=%s no handler registered for '%s'\n", driverName,
                  portName, parsed.function().c_str());
        return asynError;
    }

    int index;
    if (findParam(normalized.c_str(), &index) == asynSuccess) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s: port=%s reusing an existing parameter for '%s'\n",
                  driverName, portName, normalized.c_str());
        pasynUser->reason = index;
    } else {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s: port=%s creating a new parameter for '%s'\n", driverName,
                  portName, normalized.c_str());
        createParam(normalized.c_str(), type, &index);
        parsed.setAsynIndex(index, type);
        PVInfo *pvInfo = createPVInfo(parsed);
        if (pvInfo == NULL) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s: port=%s could not create PVInfo for '%s'\n",
                      driverName, portName, normalized.c_str());
            return asynError;
        }

        m_params[index] = pvInfo;
        pasynUser->reason = index;
        m_interruptRefcount[pvInfo] = 0;
    }

    return asynSuccess;
}

void Driver::handleResultStatus(asynUser *pasynUser, ResultBase const &result) {
    pasynUser->alarmStatus = result.alarmStatus;
    setParamAlarmStatus(pasynUser->reason, result.alarmStatus);
    pasynUser->alarmSeverity = result.alarmSeverity;
    setParamAlarmSeverity(pasynUser->reason, result.alarmSeverity);
}

PVInfo *Driver::pvInfoFromUser(asynUser *pasynUser) {
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
std::vector<PVInfo *> Driver::getAllPVs() const {
    std::vector<PVInfo *> pvs;
    pvs.reserve(m_params.size());
    for (ParamMap::const_iterator i = m_params.begin(), end = m_params.end();
         i != end; ++i) {
        pvs.push_back(i->second);
    }
    return pvs;
}

template <typename IntType>
void Driver::getInterruptPVsForInterface(std::vector<PVInfo *> &dest,
                                         int canInterrupt, void *ifacePvt) {
    ELLLIST *clients;
    pasynManager->interruptStart(ifacePvt, &clients);
    ELLNODE *node = ellFirst(clients);
    while (node) {
        interruptNode *inode = reinterpret_cast<interruptNode *>(node);
        IntType *interrupt = static_cast<IntType *>(inode->drvPvt);
        if (hasParam(interrupt->pasynUser->reason)) {
            dest.push_back(pvInfoFromUser(interrupt->pasynUser));
        }
        node = ellNext(node);
    }
    pasynManager->interruptEnd(ifacePvt);
}

std::vector<PVInfo *> Driver::getInterruptPVs() {
    std::vector<PVInfo *> infos;

    asynStandardInterfaces *ifcs = getAsynStdInterfaces();
    getInterruptPVsForInterface<asynOctetInterrupt>(
        infos, ifcs->octetCanInterrupt, ifcs->octetInterruptPvt);
    getInterruptPVsForInterface<asynUInt32DigitalInterrupt>(
        infos, ifcs->uInt32DigitalCanInterrupt,
        ifcs->uInt32DigitalInterruptPvt);
    getInterruptPVsForInterface<asynInt32Interrupt>(
        infos, ifcs->int32CanInterrupt, ifcs->int32InterruptPvt);
    getInterruptPVsForInterface<asynInt64Interrupt>(
        infos, ifcs->int64CanInterrupt, ifcs->int64InterruptPvt);
    getInterruptPVsForInterface<asynFloat64Interrupt>(
        infos, ifcs->float64CanInterrupt, ifcs->float64InterruptPvt);
    getInterruptPVsForInterface<asynInt8ArrayInterrupt>(
        infos, ifcs->int8ArrayCanInterrupt, ifcs->int8ArrayInterruptPvt);
    getInterruptPVsForInterface<asynInt16ArrayInterrupt>(
        infos, ifcs->int16ArrayCanInterrupt, ifcs->int16ArrayInterruptPvt);
    getInterruptPVsForInterface<asynInt32ArrayInterrupt>(
        infos, ifcs->int32ArrayCanInterrupt, ifcs->int32ArrayInterruptPvt);
    getInterruptPVsForInterface<asynInt64ArrayInterrupt>(
        infos, ifcs->int64ArrayCanInterrupt, ifcs->int64ArrayInterruptPvt);
    getInterruptPVsForInterface<asynFloat32ArrayInterrupt>(
        infos, ifcs->float32ArrayCanInterrupt, ifcs->float32ArrayInterruptPvt);
    getInterruptPVsForInterface<asynFloat64ArrayInterrupt>(
        infos, ifcs->float64ArrayCanInterrupt, ifcs->float64ArrayInterruptPvt);

    // The list contains all records, so we need to remove duplicates.
    std::sort(infos.begin(), infos.end());
    infos.erase(std::unique(infos.begin(), infos.end()), infos.end());

    return infos;
}

template <typename Ptr, typename Other>
static void assignPtr(Ptr *&ptr, Other *other) {
    ptr = reinterpret_cast<Ptr *>(other);
}

template <typename Iface, typename HType>
void Driver::installAnInterruptRegistrar(void *piface) {
    // I hate doing type erasure like this, but there aren't sane options ...
    Iface *iface = static_cast<Iface *>(piface);
    VoidFuncPtr reg = reinterpret_cast<VoidFuncPtr>(iface->registerInterruptUser);
    VoidFuncPtr canc = reinterpret_cast<VoidFuncPtr>(iface->cancelInterruptUser);
    m_originalIntrRegister[AsynType<HType>::value] = std::make_pair(reg, canc);
    assignPtr(iface->cancelInterruptUser, Driver::cancelInterrupt<HType>);
    if (AsynType<HType>::value == asynParamUInt32Digital) {
        // UInt32Digital has a signature different from other registrars.
        assignPtr(iface->registerInterruptUser,
                  Driver::registerInterruptDigital);
    } else {
        assignPtr(iface->registerInterruptUser,
                  Driver::registerInterrupt<HType>);
    }
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
        if (!handler) {
            throw std::out_of_range("No handler registered");
        }
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
        if (!handler) {
            throw std::out_of_range("No handler registered");
        }
    } catch (std::out_of_range const &) {
        handler = NULL;
    }
    return handler;
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
asynStatus Driver::doCallbacksArray(PVInfo const &pvInfo, Array<T> &value,
                                    asynStatus status, int alarmStatus,
                                    int alarmSeverity) {
    setParamStatus(pvInfo.asynIndex(), status);
    setParamAlarmStatus(pvInfo.asynIndex(), alarmStatus);
    setParamAlarmSeverity(pvInfo.asynIndex(), alarmSeverity);
    return doCallbacksArrayDispatch(pvInfo.asynIndex(), value);
}

template asynStatus Driver::doCallbacksArray<epicsInt8>(PVInfo const &pvInfo,
                                                        Array<epicsInt8> &value,
                                                        asynStatus status,
                                                        int alarmStatus,
                                                        int alarmSeverity);
template asynStatus Driver::doCallbacksArray<epicsInt16>(
    PVInfo const &pvInfo, Array<epicsInt16> &value, asynStatus status,
    int alarmStatus, int alarmSeverity);
template asynStatus Driver::doCallbacksArray<epicsInt32>(
    PVInfo const &pvInfo, Array<epicsInt32> &value, asynStatus status,
    int alarmStatus, int alarmSeverity);
template asynStatus Driver::doCallbacksArray<epicsInt64>(
    PVInfo const &pvInfo, Array<epicsInt64> &value, asynStatus status,
    int alarmStatus, int alarmSeverity);
template asynStatus Driver::doCallbacksArray<epicsFloat32>(
    PVInfo const &pvInfo, Array<epicsFloat32> &value, asynStatus status,
    int alarmStatus, int alarmSeverity);
template asynStatus Driver::doCallbacksArray<epicsFloat64>(
    PVInfo const &pvInfo, Array<epicsFloat64> &value, asynStatus status,
    int alarmStatus, int alarmSeverity);

template <typename T>
asynStatus Driver::setParam(PVInfo const &pvInfo, T value, asynStatus status,
                            int alarmStatus, int alarmSeverity) {
    setParamStatus(pvInfo.asynIndex(), status);
    setParamAlarmStatus(pvInfo.asynIndex(), alarmStatus);
    setParamAlarmSeverity(pvInfo.asynIndex(), alarmSeverity);
    return setParamDispatch(pvInfo.asynIndex(), value);
}

asynStatus Driver::setParam(PVInfo const &pvInfo, epicsUInt32 value,
                            epicsUInt32 mask, asynStatus status,
                            int alarmStatus, int alarmSeverity) {
    setParamStatus(pvInfo.asynIndex(), status);
    setParamAlarmStatus(pvInfo.asynIndex(), alarmStatus);
    setParamAlarmSeverity(pvInfo.asynIndex(), alarmSeverity);
    return setUIntDigitalParam(pvInfo.asynIndex(), value, mask);
}

template asynStatus Driver::setParam<epicsInt32>(PVInfo const &pvInfo,
                                                 epicsInt32 value,
                                                 asynStatus status,
                                                 int alarmStatus,
                                                 int alarmSeverity);
template asynStatus Driver::setParam<epicsInt64>(PVInfo const &pvInfo,
                                                 epicsInt64 value,
                                                 asynStatus status,
                                                 int alarmStatus,
                                                 int alarmSeverity);
template asynStatus Driver::setParam<epicsFloat64>(PVInfo const &pvInfo,
                                                   epicsFloat64 value,
                                                   asynStatus status,
                                                   int alarmStatus,
                                                   int alarmSeverity);
template asynStatus Driver::setParam<Octet>(PVInfo const &pvInfo, Octet value,
                                            asynStatus status, int alarmStatus,
                                            int alarmSeverity);

template <>
asynStatus Driver::setParam<epicsUInt32>(PVInfo const &pvInfo,
                                         epicsUInt32 value, asynStatus status,
                                         int alarmStatus, int alarmSeverity) {
    return setParam(pvInfo, value, 0xffffffff, status, alarmStatus,
                    alarmSeverity);
}

template <typename T>
asynStatus Driver::registerInterrupt(void *drvPvt, asynUser *pasynUser,
                                     void *callback, void *userPvt,
                                     void **registrarPvt) {
    Driver *self = static_cast<Driver *>(drvPvt);
    PVInfo *pvInfo = self->pvInfoFromUser(pasynUser);

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

    self->m_interruptRefcount[pvInfo] += 1;

    if (self->m_interruptRefcount[pvInfo] == 1) {
        InterruptRegistrar registrar =
            self->getHandlerMap<T>().at(pvInfo->function()).intrRegistrar;
        if (registrar != NULL) {
            asynPrint(self->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s: port=%s registering interrupt handler for '%s'\n",
                      driverName, self->portName, pvInfo->normalized().c_str());
            status = registrar(*pvInfo, false);
            if (status != asynSuccess) {
                asynPrint(
                    self->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s: port=%s error %d calling interrupt registrar for "
                    "'%s'\n",
                    driverName, self->portName, status,
                    pvInfo->normalized().c_str());
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
    PVInfo *pvInfo = self->pvInfoFromUser(pasynUser);

    // I hate doing type erasure like this, but there aren't sane options ...
    typedef asynStatus (*CancelIntrFunc)(void *drvPvt, asynUser *pasynUser,
                                         void *registrarPvt);
    CancelIntrFunc original = reinterpret_cast<CancelIntrFunc>(
        self->m_originalIntrRegister.at(AsynType<T>::value).second);
    asynStatus status = original(drvPvt, pasynUser, registrarPvt);
    if (status != asynSuccess) {
        return status;
    }

    self->m_interruptRefcount[pvInfo] -= 1;

    if (self->m_interruptRefcount[pvInfo] < 0) {
        asynPrint(self->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: port=%s logic error: interrupt refcount negative for"
                  "'%s'\n",
                  driverName, self->portName, pvInfo->normalized().c_str());
        self->m_interruptRefcount[pvInfo] = 0;
        return asynError;
    }

    if (self->m_interruptRefcount[pvInfo] == 0) {
        InterruptRegistrar registrar =
            self->getHandlerMap<T>().at(pvInfo->function()).intrRegistrar;
        if (registrar != NULL) {
            asynPrint(self->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s: port=%s cancelling interrupt handler for '%s'\n",
                      driverName, self->portName, pvInfo->normalized().c_str());
            status = registrar(*pvInfo, true);
            if (status != asynSuccess) {
                asynPrint(
                    self->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s: port=%s error %d calling interrupt registrar for "
                    "'%s'\n",
                    driverName, self->portName, status,
                    pvInfo->normalized().c_str());
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
    PVInfo *pvInfo = self->pvInfoFromUser(pasynUser);

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

    self->m_interruptRefcount[pvInfo] += 1;

    if (self->m_interruptRefcount[pvInfo] == 1) {
        InterruptRegistrar registrar =
            self->getHandlerMap<T>().at(pvInfo->function()).intrRegistrar;
        if (registrar != NULL) {
            asynPrint(self->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s: port=%s registering interrupt handler for '%s'\n",
                      driverName, self->portName, pvInfo->normalized().c_str());
            status = registrar(*pvInfo, false);
            if (status != asynSuccess) {
                asynPrint(
                    self->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s: port=%s error %d calling interrupt registrar for "
                    "'%s'\n",
                    driverName, self->portName, status,
                    pvInfo->normalized().c_str());
                return status;
            }
        }
    }

    return status;
}

template <typename T>
asynStatus Driver::readScalar(asynUser *pasynUser, T *value) {
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    typename Handlers<T>::ReadHandler handler =
        getReadHandler<T>(pvInfo->function());
    typename Handlers<T>::ReadResult result = handler(*pvInfo);
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
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    Handlers<epicsUInt32>::ReadHandler handler =
        getReadHandler<epicsUInt32>(pvInfo->function());
    Handlers<epicsUInt32>::ReadResult result = handler(*pvInfo, mask);
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
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    typename Handlers<T>::WriteHandler handler =
        getWriteHandler<T>(pvInfo->function());
    typename Handlers<T>::WriteResult result = handler(*pvInfo, value);
    handleResultStatus(pasynUser, result);
    if (shouldProcessInterrupts(result)) {
        setParamDispatch(pasynUser->reason, value);
        callParamCallbacks();
    }
    return result.status;
}

asynStatus Driver::writeScalar(asynUser *pasynUser, epicsUInt32 value,
                               epicsUInt32 mask) {
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    Handlers<epicsUInt32>::WriteHandler handler =
        getWriteHandler<epicsUInt32>(pvInfo->function());
    Handlers<epicsUInt32>::WriteResult result = handler(*pvInfo, value, mask);
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
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    Array<T> arrayRef(value, maxSize);
    typename Handlers<Array<T> >::ReadHandler handler =
        getHandlerMap<Array<T> >().at(pvInfo->function()).readHandler;
    typename Handlers<Array<T> >::ReadResult result =
        handler(*pvInfo, arrayRef);
    handleResultStatus(pasynUser, result);
    *size = arrayRef.size();
    if (shouldProcessInterrupts(result)) {
        return doCallbacksArrayDispatch(pvInfo->asynIndex(), arrayRef);
    }
    return result.status;
}

template <typename T>
asynStatus Driver::writeArray(asynUser *pasynUser, T *value, size_t size) {
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    Array<T> arrayRef(value, size);
    typename Handlers<Array<T> >::WriteHandler handler =
        getHandlerMap<Array<T> >().at(pvInfo->function()).writeHandler;
    typename Handlers<Array<T> >::WriteResult result =
        handler(*pvInfo, arrayRef);
    handleResultStatus(pasynUser, result);
    if (shouldProcessInterrupts(result)) {
        return doCallbacksArrayDispatch(pvInfo->asynIndex(), arrayRef);
    }
    return result.status;
}

asynStatus Driver::readOctetData(asynUser *pasynUser, char *value,
                                 size_t maxSize, size_t *nRead) {
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    Octet arrayRef(value, maxSize);
    Handlers<Octet>::ReadHandler handler =
        getHandlerMap<Octet>().at(pvInfo->function()).readHandler;
    Handlers<Octet>::ReadResult result = handler(*pvInfo, arrayRef);
    handleResultStatus(pasynUser, result);
    *nRead = arrayRef.size();
    // The handler should have ensured termination, but we can't be sure.
    arrayRef.terminate();
    if (shouldProcessInterrupts(result)) {
        setParamDispatch(pvInfo->asynIndex(), arrayRef);
        callParamCallbacks();
    }
    return result.status;
}

asynStatus Driver::writeOctetData(asynUser *pasynUser, char const *value,
                                  size_t size) {
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    Octet const arrayRef(const_cast<char *>(value), size);
    Handlers<Octet>::WriteHandler handler =
        getHandlerMap<Octet>().at(pvInfo->function()).writeHandler;
    Handlers<Octet>::WriteResult result = handler(*pvInfo, arrayRef);
    handleResultStatus(pasynUser, result);
    if (shouldProcessInterrupts(result)) {
        setParamDispatch(pvInfo->asynIndex(), arrayRef);
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
