#pragma once

#include "autoparamHandler.h"
#include <map>
#include <stdexcept>

namespace Autoparam {

class Driver : public asynPortDriver {
  protected:
    virtual Reason *createReason(Reason const &baseReason) {
        return new Reason(baseReason);
    }

  public:
    explicit Driver(const char *portName, int interfaceMask, int interruptMask,
                    int asynFlags, int autoConnect, int priority,
                    int stackSize);

    virtual ~Driver();

  protected:
    // reader ali writer je lahko NULL, če ga ni
    template <typename T>
    void registerHandlers(std::string const &function,
                          typename Handlers<T>::ReadHandler reader,
                          typename Handlers<T>::WriteHandler writer) {
        getHandlerMap<T>()[function].readHandler = reader;
        getHandlerMap<T>()[function].writeHandler = writer;
        m_functionTypes[function] = AsynType<T>::value;
    }

    // Priročni overloadi za uporabo io-interrupt recordov v derived driverju.
    // Tole je skupno vsem arrayem, zato šablona
    template <typename T>
    asynStatus doCallbacksArray(Reason const &reason, Array<T> &value,
                                int alarmStatus = epicsAlarmNone,
                                int alarmSeverity = epicsSevNone) {
        setParamAlarmStatus(reason.index(), alarmStatus);
        setParamAlarmSeverity(reason.index(), alarmSeverity);
        return doCallbacksArrayDispatch(reason.index(), value);
    }

    // Priročna funkcija za uporabo io-interrupt recordov v derived driverju. Ne
    // kličejo sama callParamCallbacks(), da lahko nastaviš več skalarjev,
    // preden ga pokličeš.
    template <typename T>
    asynStatus setParam(Reason const &reason, T value,
                        int alarmStatus = epicsAlarmNone,
                        int alarmSeverity = epicsSevNone) {
        setParamAlarmStatus(reason.index(), alarmStatus);
        setParamAlarmSeverity(reason.index(), alarmSeverity);
        return setParamDispatch(reason.index(), value);
    }

  public:
    // Beyond this point, the methods are public because they are part of the
    // asyn interface, but derived classes shouldn't need to override them.

    // TODO UInt32Digital, Octet

    asynStatus drvUserCreate(asynUser *pasynUser, const char *reason,
                             const char **, size_t *);

    asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value) {
        return readScalar(pasynUser, value);
    }

    asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value) {
        return writeScalar(pasynUser, value);
    }

    asynStatus readInt64(asynUser *pasynUser, epicsInt64 *value) {
        return readScalar(pasynUser, value);
    }

    asynStatus writeInt64(asynUser *pasynUser, epicsInt64 value) {
        return writeScalar(pasynUser, value);
    }

    asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value) {
        return readScalar(pasynUser, value);
    }

    asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value) {
        return writeScalar(pasynUser, value);
    }

    asynStatus readInt8Array(asynUser *pasynUser, epicsInt8 *value,
                             size_t maxSize, size_t *size) {
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeInt8Array(asynUser *pasynUser, epicsInt8 *value,
                              size_t size) {
        return writeArray(pasynUser, value, size);
    }

    asynStatus readInt16Array(asynUser *pasynUser, epicsInt16 *value,
                              size_t maxSize, size_t *size) {
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeInt16Array(asynUser *pasynUser, epicsInt16 *value,
                               size_t size) {
        return writeArray(pasynUser, value, size);
    }

    asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                              size_t maxSize, size_t *size) {
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeInt32Array(asynUser *pasynUser, epicsInt32 *value,
                               size_t size) {
        return writeArray(pasynUser, value, size);
    }

    asynStatus readInt64Array(asynUser *pasynUser, epicsInt64 *value,
                              size_t maxSize, size_t *size) {
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeInt64Array(asynUser *pasynUser, epicsInt64 *value,
                               size_t size) {
        return writeArray(pasynUser, value, size);
    }

    asynStatus readFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
                                size_t maxSize, size_t *size) {
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
                                 size_t size) {
        return writeArray(pasynUser, value, size);
    }

    asynStatus readFloat64Array(asynUser *pasynUser, epicsFloat64 *value,
                                size_t maxSize, size_t *size) {
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeFloat64Array(asynUser *pasynUser, epicsFloat64 *value,
                                 size_t size) {
        return writeArray(pasynUser, value, size);
    }

  private:
    void handleResultStatus(asynUser *pasynUser, ResultBase const &result);

    Reason *reasonFromUser(asynUser *pasynUser);

    template <typename T>
    asynStatus doCallbacksArrayDispatch(int index, Array<T> &value);
    template <typename T> asynStatus setParamDispatch(int index, T value);
    template <typename T> asynStatus readScalar(asynUser *pasynUser, T *value);
    template <typename T> asynStatus writeScalar(asynUser *pasynUser, T value);
    template <typename T>
    asynStatus readArray(asynUser *pasynUser, T *value, size_t maxSize,
                         size_t *size);
    template <typename T>
    asynStatus writeArray(asynUser *pasynUser, T *value, size_t size);

    template <typename T> std::map<std::string, Handlers<T> > &getHandlerMap();

    static char const *driverName;

    typedef std::map<int, Reason *> ParamMap;
    ParamMap m_params;
    std::map<std::string, asynParamType> m_functionTypes;

    std::map<std::string, Handlers<epicsInt32> > m_Int32HandlerMap;
    std::map<std::string, Handlers<epicsInt64> > m_Int64HandlerMap;
    std::map<std::string, Handlers<epicsUInt32> > m_UInt32HandlerMap;
    std::map<std::string, Handlers<epicsFloat64> > m_Float64HandlerMap;
    std::map<std::string, Handlers<Octet> > m_OctetHandlerMap;
    std::map<std::string, Handlers<Array<epicsInt8 > > > m_Int8ArrayHandlerMap;
    std::map<std::string, Handlers<Array<epicsInt16 > > >
        m_Int16ArrayHandlerMap;
    std::map<std::string, Handlers<Array<epicsInt32 > > >
        m_Int32ArrayHandlerMap;
    std::map<std::string, Handlers<Array<epicsInt64 > > >
        m_Int64ArrayHandlerMap;
    std::map<std::string, Handlers<Array<epicsFloat32 > > >
        m_Float32ArrayHandlerMap;
    std::map<std::string, Handlers<Array<epicsFloat64 > > >
        m_Float64ArrayHandlerMap;
};

char const *Driver::driverName = "Autoparam::Driver";

template <typename T>
asynStatus Driver::readScalar(asynUser *pasynUser, T *value) {
    Reason *reason = reasonFromUser(pasynUser);
    if (!reason) {
        return asynError;
    }

    typename Handlers<T>::ReadResult result;
    try {
        result = getHandlerMap<T>().at(reason->function()).readHandler(*reason);
    } catch (std::out_of_range const &) {
        // nimamo handlerja, parameter pa obstaja. Branje takega ni kul,
        // zato pustimo, da result vsebuje soft alarm.
    }

    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        *value = result.value;
    }
    return result.status;
}

template <typename T>
asynStatus Driver::writeScalar(asynUser *pasynUser, T value) {
    Reason *reason = reasonFromUser(pasynUser);
    if (!reason) {
        return asynError;
    }

    typename Handlers<T>::WriteResult result;
    try {
        result = getHandlerMap<T>()
                     .at(reason->function())
                     .writeHandler(*reason, value);
    } catch (std::out_of_range const &) {
        // TODO
        // nimamo handlerja, parameter pa obstaja. Pisanje v takega ni kul,
        // zato pustimo, da result vsebuje soft alarm.
    }

    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        setParamDispatch(pasynUser->reason, value);
        callParamCallbacks();
    }
    return result.status;
}

template <typename T>
asynStatus Driver::readArray(asynUser *pasynUser, T *value, size_t maxSize,
                             size_t *size) {
    Reason *reason = reasonFromUser(pasynUser);
    if (!reason) {
        return asynError;
    }

    typename Handlers<Array<T> >::ReadResult result;
    try {
        result = getHandlerMap<Array<T> >()
                     .at(reason->function())
                     .readHandler(*reason);
    } catch (std::out_of_range const &) {
        // No handler
    }

    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        *size = std::min(result.value.size(), maxSize);
        std::copy(result.value.data(), result.value.data() + *size, value);
    }
    return result.status;
}

template <typename T>
asynStatus Driver::writeArray(asynUser *pasynUser, T *value, size_t size) {
    Reason *reason = reasonFromUser(pasynUser);
    if (!reason) {
        return asynError;
    }

    typename Handlers<Array<T> >::ReadResult result;
    try {
        result = getHandlerMap<Array<T> >()
                     .at(reason->function())
                     .readHandler(*reason);
    } catch (std::out_of_range const &) {
        // No handler
    }

    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        return doCallbacksArrayDispatch(reason->index(), result.value);
    }
    return result.status;
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
    return setStringParam(index, value);
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

} // namespace Autoparam
