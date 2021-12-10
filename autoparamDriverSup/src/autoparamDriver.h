#pragma once

#include "autoparamHandler.h"
#include <map>
#include <stdexcept>

namespace Autoparam {

struct DriverOpts {
    int interfaceMask;
    int interruptMask;
    int asynFlags;
    int autoConnect;
    int priority;
    int stackSize;
    bool autodestruct;

    DriverOpts &setInterfaceMask(int mask) {
        interfaceMask |= mask;
        return *this;
    }

    DriverOpts &setInterruptMask(int mask) {
        interruptMask |= mask;
        return *this;
    }

    DriverOpts &setBlocking(bool enable = true) {
        if (enable) {
            asynFlags |= ASYN_CANBLOCK;
        } else {
            asynFlags &= ~ASYN_CANBLOCK;
        }
        return *this;
    }

    DriverOpts &setAutoconnect(bool enable = true) {
        autoConnect = enable;
        return *this;
    }

    DriverOpts &setPriority(int prio) {
        priority = prio;
        return *this;
    }

    DriverOpts &setStacksize(int size) {
        stackSize = size;
        return *this;
    }

    DriverOpts &setAutodestruct(bool enable = true) {
        autodestruct = enable;
        return *this;
    }

    static const int defaultMask = asynCommonMask | asynDrvUserMask;

    DriverOpts()
        : interfaceMask(defaultMask), interruptMask(0), asynFlags(0),
          autoConnect(1), priority(0), stackSize(0), autodestruct(false) {}
};

class Driver : public asynPortDriver {
  public:
    // Convenience typedefs
    typedef Result<void> WriteResult;
    typedef Result<epicsInt32> Int32ReadResult;
    typedef Result<epicsInt64> Int64ReadResult;
    typedef Result<epicsUInt32> UInt32ReadResult;
    typedef Result<epicsFloat64> Float64ReadResult;
    typedef Result<Octet> OctetReadResult;
    typedef Result<Array<epicsInt8> > Int8ArrayReadResult;
    typedef Result<Array<epicsInt16> > Int16ArrayReadResult;
    typedef Result<Array<epicsInt32> > Int32ArrayReadResult;
    typedef Result<Array<epicsInt64> > Int64ArrayReadResult;
    typedef Result<Array<epicsFloat32> > Float32ArrayReadResult;
    typedef Result<Array<epicsFloat64> > Float64ArrayReadResult;

    explicit Driver(const char *portName, DriverOpts const &params);

    virtual ~Driver();

  protected:
    virtual PVInfo *createPVInfo(PVInfo const &baseInfo) {
        return new PVInfo(baseInfo);
    }

    template <typename T>
    void registerHandlers(std::string const &function,
                          typename Handlers<T>::ReadHandler reader,
                          typename Handlers<T>::WriteHandler writer) {
        getHandlerMap<T>()[function].readHandler = reader;
        getHandlerMap<T>()[function].writeHandler = writer;
        m_functionTypes[function] = Handlers<T>::type;
    }

    template <typename T>
    asynStatus doCallbacksArray(PVInfo const &pvInfo, Array<T> &value,
                                int alarmStatus = epicsAlarmNone,
                                int alarmSeverity = epicsSevNone) {
        setParamAlarmStatus(pvInfo.index(), alarmStatus);
        setParamAlarmSeverity(pvInfo.index(), alarmSeverity);
        return doCallbacksArrayDispatch(pvInfo.index(), value);
    }

    template <typename T>
    asynStatus setParam(PVInfo const &pvInfo, T value,
                        int alarmStatus = epicsAlarmNone,
                        int alarmSeverity = epicsSevNone) {
        setParamAlarmStatus(pvInfo.index(), alarmStatus);
        setParamAlarmSeverity(pvInfo.index(), alarmSeverity);
        return setParamDispatch(pvInfo.index(), value);
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

    PVInfo *pvInfoFromUser(asynUser *pasynUser);

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

    typedef std::map<int, PVInfo *> ParamMap;
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
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    if (!pvInfo) {
        return asynError;
    }

    typename Handlers<T>::ReadResult result;
    typename Handlers<T>::ReadHandler handler;
    try {
        handler = getHandlerMap<T>().at(pvInfo->function()).readHandler;
        if (!handler) {
            throw std::out_of_range("No handler registered");
        }
    } catch (std::out_of_range const &) {
        asynPrint(
            this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s: port=%s no read handler registered for '%s' of type %s\n",
            driverName, portName, pvInfo->function().c_str(),
            AsynType<T>::name);
        result.status = asynError;
        result.alarmStatus = epicsAlarmSoft;
        result.alarmSeverity = epicsSevInvalid;
    }

    result = handler(*pvInfo);
    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        *value = result.value;
        setParamDispatch(pasynUser->reason, result.value);
    }
    return result.status;
}

template <typename T>
asynStatus Driver::writeScalar(asynUser *pasynUser, T value) {
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    if (!pvInfo) {
        return asynError;
    }

    typename Handlers<T>::WriteResult result;
    typename Handlers<T>::WriteHandler handler;
    try {
        handler = getHandlerMap<T>().at(pvInfo->function()).writeHandler;
        if (!handler) {
            throw std::out_of_range("No handler registered");
        }
    } catch (std::out_of_range const &) {
        asynPrint(
            this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s: port=%s no write handler registered for '%s' of type %s\n",
            driverName, portName, pvInfo->function().c_str(),
            AsynType<T>::name);
        result.status = asynError;
        result.alarmStatus = epicsAlarmSoft;
        result.alarmSeverity = epicsSevInvalid;
    }

    result = handler(*pvInfo, value);
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
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    if (!pvInfo) {
        return asynError;
    }

    typename Handlers<Array<T> >::ReadResult result;
    typename Handlers<Array<T> >::ReadHandler handler;
    try {
        handler = getHandlerMap<Array<T> >().at(pvInfo->function()).readHandler;
        if (!handler) {
            throw std::out_of_range("No handler registered");
        }
    } catch (std::out_of_range const &) {
        asynPrint(
            this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s: port=%s no read handler registered for '%s' of type %s\n",
            driverName, portName, pvInfo->function().c_str(),
            AsynType<Array<T> >::name);
        result.status = asynError;
        result.alarmStatus = epicsAlarmSoft;
        result.alarmSeverity = epicsSevInvalid;
    }

    result = handler(*pvInfo, maxSize);
    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        *size = std::min(result.value.size(), maxSize);
        std::copy(result.value.data(), result.value.data() + *size, value);
    }
    return result.status;
}

template <typename T>
asynStatus Driver::writeArray(asynUser *pasynUser, T *value, size_t size) {
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    if (!pvInfo) {
        return asynError;
    }

    Array<T> arrayRef(value, size);
    typename Handlers<Array<T> >::WriteResult result;
    typename Handlers<Array<T> >::WriteHandler handler;
    try {
        handler =
            getHandlerMap<Array<T> >().at(pvInfo->function()).writeHandler;
        if (!handler) {
            throw std::out_of_range("No handler registered");
        }
    } catch (std::out_of_range const &) {
        asynPrint(
            this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s: port=%s no write handler registered for '%s' of type %s\n",
            driverName, portName, pvInfo->function().c_str(),
            AsynType<Array<T> >::name);
        result.status = asynError;
        result.alarmStatus = epicsAlarmSoft;
        result.alarmSeverity = epicsSevInvalid;
    }

    result = handler(*pvInfo, arrayRef);
    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        return doCallbacksArrayDispatch(pvInfo->index(), arrayRef);
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
    // TODO
    return setStringParam(index, std::string());
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
