#include "autoparamDriver.h"

namespace Autoparam {

Reason::Reason(char const *asynReason) {
    // TODO
}

Reason::Reason(Reason const &other) { *this = other; }

Reason &Reason::operator=(Reason const &other) {
    m_asynParamIndex = other.m_asynParamIndex;
    m_function = other.m_function;
    m_arguments = other.m_arguments;
    return *this;
}

Reason::~Reason() {
    // Nothing to do here.
}

std::string Reason::normalized() const {
    // TODO
    return std::string();
}

Driver::Driver(const char *portName, int interfaceMask, int interruptMask,
               int asynFlags, int autoConnect, int priority, int stackSize)
    : asynPortDriver(portName, 1, interfaceMask, interruptMask, asynFlags,
                     autoConnect, priority, stackSize) {
    // TODO
}

Driver::~Driver() {
    for (ParamMap::iterator i = m_params.begin(), end = m_params.end();
         i != end; ++i) {
        delete i->second;
    }
}

asynStatus Driver::drvUserCreate(asynUser *pasynUser, const char *reason,
                                 const char **, size_t *) {
    Reason parsed(reason);
    std::string normalized = parsed.normalized();
    asynParamType type;
    try {
        type = m_functionTypes.at(parsed.function());
    } catch (std::out_of_range const &) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s: port=%s no handler registered for '%s'", driverName,
                  portName, parsed.function().c_str());
        return asynError;
    }

    int index;
    if (findParam(normalized.c_str(), &index) == asynSuccess) {
        pasynUser->reason = index;
    } else {
        createParam(normalized.c_str(), type, &index);
        m_params[index] = createReason(parsed);
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

Reason *Driver::reasonFromUser(asynUser *pasynUser) {
    try {
        return m_params.at(pasynUser->reason);
    } catch (std::out_of_range const &) {
        char const *paramName;
        asynStatus status = getParamName(pasynUser->reason, &paramName);
        if (status == asynSuccess) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s: port=%s no handler registered for '%s'", driverName,
                      portName, paramName);
        } else {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s: port=%s no parameter exists at index %d", driverName,
                      portName, pasynUser->reason);
        }
        return NULL;
    }
}

} // namespace Autoparam
