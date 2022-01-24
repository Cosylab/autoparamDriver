// SPDX-FileCopyrightText: 2022 Cosylab d.d.
//
// SPDX-License-Identifier: MIT

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
    bool autoInterrupts;

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

    DriverOpts &setAutoInterrupts(bool enable) {
        autoInterrupts = enable;
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
          stackSize(0), autoDestruct(false), autoInterrupts(true) {}
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
                          typename Handlers<T>::WriteHandler writer,
                          InterruptRegistrar intrRegistrar);

    template <typename T>
    asynStatus doCallbacksArray(PVInfo const &pvInfo, Array<T> &value,
                                int alarmStatus = epicsAlarmNone,
                                int alarmSeverity = epicsSevNone);

    template <typename T>
    asynStatus setParam(PVInfo const &pvInfo, T value,
                        int alarmStatus = epicsAlarmNone,
                        int alarmSeverity = epicsSevNone);

    asynStatus setParam(PVInfo const &pvInfo, epicsUInt32 value,
                        epicsUInt32 mask, int alarmStatus = epicsAlarmNone,
                        int alarmSeverity = epicsSevNone);

    bool hasParam(int index);

    PVInfo *pvInfoFromUser(asynUser *pasynUser);

    std::vector<PVInfo *> getInterruptPVs();

  public:
    // Beyond this point, the methods are public because they are part of the
    // asyn interface, but derived classes shouldn't need to override them.

    asynStatus drvUserCreate(asynUser *pasynUser, const char *reason,
                             const char **, size_t *);

    asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);

    asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);

    asynStatus readInt64(asynUser *pasynUser, epicsInt64 *value);

    asynStatus writeInt64(asynUser *pasynUser, epicsInt64 value);

    asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);

    asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);

    asynStatus readInt8Array(asynUser *pasynUser, epicsInt8 *value,
                             size_t maxSize, size_t *size);

    asynStatus writeInt8Array(asynUser *pasynUser, epicsInt8 *value,
                              size_t size);

    asynStatus readInt16Array(asynUser *pasynUser, epicsInt16 *value,
                              size_t maxSize, size_t *size);

    asynStatus writeInt16Array(asynUser *pasynUser, epicsInt16 *value,
                               size_t size);

    asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                              size_t maxSize, size_t *size);

    asynStatus writeInt32Array(asynUser *pasynUser, epicsInt32 *value,
                               size_t size);

    asynStatus readInt64Array(asynUser *pasynUser, epicsInt64 *value,
                              size_t maxSize, size_t *size);

    asynStatus writeInt64Array(asynUser *pasynUser, epicsInt64 *value,
                               size_t size);

    asynStatus readFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
                                size_t maxSize, size_t *size);

    asynStatus writeFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
                                 size_t size);

    asynStatus readFloat64Array(asynUser *pasynUser, epicsFloat64 *value,
                                size_t maxSize, size_t *size);

    asynStatus writeFloat64Array(asynUser *pasynUser, epicsFloat64 *value,
                                 size_t size);

    asynStatus readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value,
                                 epicsUInt32 mask);

    asynStatus writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value,
                                  epicsUInt32 mask);

    asynStatus readOctet(asynUser *pasynUser, char *value, size_t nChars,
                         size_t *nActual, int *eomReason);

    asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t nChars,
                          size_t *nActual);

  private:
    void handleResultStatus(asynUser *pasynUser, ResultBase const &result);

    template <typename IntType>
    void getInterruptPVsForInterface(std::vector<PVInfo *> &dest,
                                     int canInterrupt, void *ifacePvt);

    template <typename T> bool hasReadHandler(int index);
    template <typename T> bool hasWriteHandler(int index);

    bool shouldProcessInterrupts(WriteResult const &result) const;
    bool shouldProcessInterrupts(ResultBase const &result) const;

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

    template <typename Iface, typename HType>
    void installAnInterruptRegistrar(void *interface);
    void installInterruptRegistrars();
    template <typename T>
    static asynStatus registerInterrupt(void *drvPvt, asynUser *pasynUser,
                                        void *callback, void *userPvt,
                                        void **registrarPvt);
    static asynStatus registerInterruptDigital(void *drvPvt,
                                               asynUser *pasynUser,
                                               void *callback, void *userPvt,
                                               epicsUInt32 mask,
                                               void **registrarPvt);
    template <typename T>
    static asynStatus cancelInterrupt(void *drvPvt, asynUser *pasynUser,
                                      void *registrarPvt);

    DriverOpts opts;

    typedef std::map<int, PVInfo *> ParamMap;
    ParamMap m_params;
    std::map<std::string, asynParamType> m_functionTypes;
    std::map<asynParamType, std::pair<void *, void *> > m_originalIntrRegister;

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

} // namespace Autoparam
