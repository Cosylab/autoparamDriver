#include "autoparamDriver.h"

#include <errlog.h>
#include <epicsExit.h>

namespace Autoparam {

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

Driver::Driver(const char *portName, const DriverOpts &params)
    : asynPortDriver(portName, 1, params.interfaceMask, params.interruptMask,
                     params.asynFlags, params.autoConnect, params.priority,
                     params.stackSize),
      opts(params) {
    if (params.autoDestruct) {
        epicsAtExit(destroyDriver, this);
    }
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
        pasynUser->reason = index;
    } else {
        createParam(normalized.c_str(), type, &index);
        m_params[index] = createPVInfo(parsed);
        m_params[index]->setIndex(index);
        pasynUser->reason = index;
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

    return infos;
}

} // namespace Autoparam
