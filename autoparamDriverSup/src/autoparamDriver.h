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
    bool autoDestruct;

    DriverOpts &setBlocking(bool enable = true) {
        if (enable) {
            asynFlags |= ASYN_CANBLOCK;
        } else {
            asynFlags &= ~ASYN_CANBLOCK;
        }
        return *this;
    }

    DriverOpts &setAutoConnect(bool enable = true) {
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

    DriverOpts &setAutoDestruct(bool enable = true) {
        autoDestruct = enable;
        return *this;
    }

    static const int minimalInterfaceMask = asynCommonMask | asynDrvUserMask;
    static const int defaultMask =
        asynInt32Mask | asynInt64Mask | asynUInt32DigitalMask |
        asynFloat64Mask | asynOctetMask | asynInt8ArrayMask |
        asynInt16ArrayMask | asynInt32ArrayMask | asynInt64ArrayMask |
        asynFloat32ArrayMask | asynFloat64ArrayMask;

    DriverOpts()
        : interfaceMask(minimalInterfaceMask | defaultMask),
          interruptMask(defaultMask), asynFlags(0), autoConnect(1), priority(0),
          stackSize(0), autoDestruct(false) {}
};

class Driver : public asynPortDriver {
  public:
    explicit Driver(const char *portName, DriverOpts const &params);

    virtual ~Driver();

  protected:
    virtual PVInfo *createPVInfo(PVInfo const &baseInfo) = 0;

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

    bool hasParam(int index);

    PVInfo *pvInfoFromUser(asynUser *pasynUser);

  public:
    // Beyond this point, the methods are public because they are part of the
    // asyn interface, but derived classes shouldn't need to override them.

    asynStatus drvUserCreate(asynUser *pasynUser, const char *reason,
                             const char **, size_t *);

    asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value) {
        if (!hasParam(pasynUser->reason) ||
            !hasReadHandler<epicsInt32>(pasynUser->reason)) {
            return asynPortDriver::readInt32(pasynUser, value);
        }
        return readScalar(pasynUser, value);
    }

    asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<epicsInt32>(pasynUser->reason)) {
            return asynPortDriver::writeInt32(pasynUser, value);
        }
        return writeScalar(pasynUser, value);
    }

    asynStatus readInt64(asynUser *pasynUser, epicsInt64 *value) {
        if (!hasParam(pasynUser->reason) ||
            !hasReadHandler<epicsInt64>(pasynUser->reason)) {
            return asynPortDriver::readInt64(pasynUser, value);
        }
        return readScalar(pasynUser, value);
    }

    asynStatus writeInt64(asynUser *pasynUser, epicsInt64 value) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<epicsInt64>(pasynUser->reason)) {
            return asynPortDriver::writeInt64(pasynUser, value);
        }
        return writeScalar(pasynUser, value);
    }

    asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value) {
        if (!hasParam(pasynUser->reason) ||
            !hasReadHandler<epicsFloat64>(pasynUser->reason)) {
            return asynPortDriver::readFloat64(pasynUser, value);
        }
        return readScalar(pasynUser, value);
    }

    asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<epicsFloat64>(pasynUser->reason)) {
            return asynPortDriver::writeFloat64(pasynUser, value);
        }
        return writeScalar(pasynUser, value);
    }

    asynStatus readInt8Array(asynUser *pasynUser, epicsInt8 *value,
                             size_t maxSize, size_t *size) {
        if (!hasParam(pasynUser->reason) ||
            !hasReadHandler<Array<epicsInt8> >(pasynUser->reason)) {
            return asynPortDriver::readInt8Array(pasynUser, value, maxSize,
                                                 size);
        }
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeInt8Array(asynUser *pasynUser, epicsInt8 *value,
                              size_t size) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<Array<epicsInt8> >(pasynUser->reason)) {
            return asynPortDriver::writeInt8Array(pasynUser, value, size);
        }
        return writeArray(pasynUser, value, size);
    }

    asynStatus readInt16Array(asynUser *pasynUser, epicsInt16 *value,
                              size_t maxSize, size_t *size) {
        if (!hasParam(pasynUser->reason) ||
            !hasReadHandler<Array<epicsInt16> >(pasynUser->reason)) {
            return asynPortDriver::readInt16Array(pasynUser, value, maxSize,
                                                  size);
        }
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeInt16Array(asynUser *pasynUser, epicsInt16 *value,
                               size_t size) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<Array<epicsInt16> >(pasynUser->reason)) {
            return asynPortDriver::writeInt16Array(pasynUser, value, size);
        }
        return writeArray(pasynUser, value, size);
    }

    asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                              size_t maxSize, size_t *size) {
        if (!hasParam(pasynUser->reason) ||
            !hasReadHandler<Array<epicsInt32> >(pasynUser->reason)) {
            return asynPortDriver::readInt32Array(pasynUser, value, maxSize,
                                                  size);
        }
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeInt32Array(asynUser *pasynUser, epicsInt32 *value,
                               size_t size) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<Array<epicsInt32> >(pasynUser->reason)) {
            return asynPortDriver::writeInt32Array(pasynUser, value, size);
        }
        return writeArray(pasynUser, value, size);
    }

    asynStatus readInt64Array(asynUser *pasynUser, epicsInt64 *value,
                              size_t maxSize, size_t *size) {
        if (!hasParam(pasynUser->reason) ||
            !hasReadHandler<Array<epicsInt64> >(pasynUser->reason)) {
            return asynPortDriver::readInt64Array(pasynUser, value, maxSize,
                                                  size);
        }
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeInt64Array(asynUser *pasynUser, epicsInt64 *value,
                               size_t size) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<Array<epicsInt64> >(pasynUser->reason)) {
            return asynPortDriver::writeInt64Array(pasynUser, value, size);
        }
        return writeArray(pasynUser, value, size);
    }

    asynStatus readFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
                                size_t maxSize, size_t *size) {
        if (!hasParam(pasynUser->reason) ||
            !hasReadHandler<Array<epicsFloat32> >(pasynUser->reason)) {
            return asynPortDriver::readFloat32Array(pasynUser, value, maxSize,
                                                    size);
        }
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
                                 size_t size) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<Array<epicsFloat32> >(pasynUser->reason)) {
            return asynPortDriver::writeFloat32Array(pasynUser, value, size);
        }
        return writeArray(pasynUser, value, size);
    }

    asynStatus readFloat64Array(asynUser *pasynUser, epicsFloat64 *value,
                                size_t maxSize, size_t *size) {
        if (!hasParam(pasynUser->reason) ||
            !hasReadHandler<Array<epicsFloat64> >(pasynUser->reason)) {
            return asynPortDriver::readFloat64Array(pasynUser, value, maxSize,
                                                    size);
        }
        return readArray(pasynUser, value, maxSize, size);
    }

    asynStatus writeFloat64Array(asynUser *pasynUser, epicsFloat64 *value,
                                 size_t size) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<Array<epicsFloat64> >(pasynUser->reason)) {
            return asynPortDriver::writeFloat64Array(pasynUser, value, size);
        }
        return writeArray(pasynUser, value, size);
    }

    asynStatus readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value,
                                 epicsUInt32 mask) {
        if (!hasParam(pasynUser->reason) ||
            !hasReadHandler<epicsUInt32>(pasynUser->reason)) {
            return asynPortDriver::readUInt32Digital(pasynUser, value, mask);
        }
        return readScalar(pasynUser, value, mask);
    }

    asynStatus writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value,
                                  epicsUInt32 mask) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<epicsUInt32>(pasynUser->reason)) {
            return asynPortDriver::writeUInt32Digital(pasynUser, value, mask);
        }
        return writeScalar(pasynUser, value, mask);
    }

    asynStatus readOctet(asynUser *pasynUser, char *value, size_t nChars,
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

    asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t nChars,
                          size_t *nActual) {
        if (!hasParam(pasynUser->reason) ||
            !hasWriteHandler<Octet>(pasynUser->reason)) {
            return asynPortDriver::writeOctet(pasynUser, value, nChars,
                                              nActual);
        }
        // Only complete writes are supported.
        *nActual = nChars;
        return writeOctetData(pasynUser, value, nChars);
    }

  private:
    void handleResultStatus(asynUser *pasynUser, ResultBase const &result);

    template <typename T> bool hasReadHandler(int index);
    template <typename T> bool hasWriteHandler(int index);
    asynStatus doCallbacksArrayDispatch(int index, Octet const &value);
    template <typename T>
    asynStatus doCallbacksArrayDispatch(int index, Array<T> &value);
    template <typename T> asynStatus setParamDispatch(int index, T value);
    template <typename T>
    typename Handlers<T>::ReadHandler
    getReadHandler(std::string const &function);
    template <typename T>
    typename Handlers<T>::WriteHandler
    getWriteHandler(std::string const &function);
    template <typename T> asynStatus readScalar(asynUser *pasynUser, T *value);
    asynStatus readScalar(asynUser *pasynUser, epicsUInt32 *value,
                          epicsUInt32 mask);
    template <typename T> asynStatus writeScalar(asynUser *pasynUser, T value);
    asynStatus writeScalar(asynUser *pasynUser, epicsUInt32 value,
                           epicsUInt32 mask);
    template <typename T>
    asynStatus readArray(asynUser *pasynUser, T *value, size_t maxSize,
                         size_t *size);
    template <typename T>
    asynStatus writeArray(asynUser *pasynUser, T *value, size_t size);
    asynStatus readOctetData(asynUser *pasynUser, char *value, size_t maxSize,
                             size_t *nRead);
    asynStatus writeOctetData(asynUser *pasynUser, char const *value,
                              size_t size);

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

template <typename T>
asynStatus Driver::readScalar(asynUser *pasynUser, T *value) {
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    typename Handlers<T>::ReadHandler handler =
        getReadHandler<T>(pvInfo->function());
    typename Handlers<T>::ReadResult result = handler(*pvInfo);
    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        *value = result.value;
        setParamDispatch(pasynUser->reason, result.value);
        if (result.processInterrupts) {
            callParamCallbacks();
        }
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
    if (result.status == asynSuccess) {
        *value = result.value;
        setUIntDigitalParam(pasynUser->reason, result.value, mask);
        if (result.processInterrupts) {
            callParamCallbacks();
        }
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
    if (result.status == asynSuccess) {
        setParamDispatch(pasynUser->reason, value);
        if (result.processInterrupts) {
            callParamCallbacks();
        }
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
    if (result.status == asynSuccess) {
        setUIntDigitalParam(pasynUser->reason, value, mask);
        if (result.processInterrupts) {
            callParamCallbacks();
        }
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
    if (result.status == asynSuccess) {
        *size = arrayRef.size();
        if (result.processInterrupts) {
            return doCallbacksArrayDispatch(pvInfo->index(), arrayRef);
        }
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
    if (result.status == asynSuccess && result.processInterrupts) {
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

asynStatus Driver::readOctetData(asynUser *pasynUser, char *value,
                                 size_t maxSize, size_t *nRead) {
    PVInfo *pvInfo = pvInfoFromUser(pasynUser);
    Octet arrayRef(value, maxSize);
    Handlers<Octet>::ReadHandler handler =
        getHandlerMap<Octet>().at(pvInfo->function()).readHandler;
    Handlers<Octet>::ReadResult result = handler(*pvInfo, arrayRef);
    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        *nRead = arrayRef.size();
        if (result.processInterrupts) {
            // The handler should have ensured termination, but we can't be
            // sure.
            arrayRef.terminate();
            setParamDispatch(pvInfo->index(), arrayRef);
            if (result.processInterrupts) {
                callParamCallbacks();
            }
        }
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
    if (result.status == asynSuccess && result.processInterrupts) {
        setParamDispatch(pvInfo->index(), arrayRef);
        if (result.processInterrupts) {
            callParamCallbacks();
        }
    }
    return result.status;
}

} // namespace Autoparam
