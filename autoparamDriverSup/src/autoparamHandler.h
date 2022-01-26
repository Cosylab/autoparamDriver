// SPDX-FileCopyrightText: 2022 Cosylab d.d.
//
// SPDX-License-Identifier: MIT

#pragma once

// Standard includes
#include <string>
#include <vector>
#include <stdexcept>

// EPICS includes
#include <asynPortDriver.h>
#include <alarm.h>
#include <epicsTypes.h>

namespace Autoparam {

/*! represents a process variable and is a handle for asyn parameters.
 * A placeholder for a longer description.
 */
class PVInfo {
  public:
    typedef std::vector<std::string> ArgumentList;

    //! Brief description goes here
    PVInfo(PVInfo const &other);

    PVInfo &operator=(PVInfo const &other);

    virtual ~PVInfo();

    std::string const &function() const { return m_function; }

    ArgumentList const &arguments() const { return m_arguments; }

    std::string normalized() const;

    int asynIndex() const { return m_asynParamIndex; }

    asynParamType asynType() const { return m_asynParamType; }

  private:
    friend class Driver;

    explicit PVInfo(char const *asynReason);

    void setAsynIndex(int index, asynParamType type) {
        m_asynParamIndex = index;
        m_asynParamType = type;
    }

    int m_asynParamIndex;
    asynParamType m_asynParamType;
    std::string m_function;
    ArgumentList m_arguments;
};

template <typename T> class Array {
  public:
    Array(T *value, size_t size)
        : m_data(value), m_size(size), m_maxSize(size) {
        if (value == NULL && size != 0) {
            throw std::logic_error("Cannot create Autoparam::Array from NULL");
        }
    }

    T *data() const { return m_data; }

    size_t size() const { return m_size; }

    size_t maxSize() const { return m_maxSize; }

    void setSize(size_t size) { m_size = std::min(m_maxSize, size); }

    void fillFrom(T const *data, size_t size) {
        m_size = std::min(m_maxSize, size);
        std::copy(data, data + m_size, m_data);
    }

    void fillFrom(std::vector<T> const &vector) {
        return fillFrom(vector.data(), vector.size());
    }

    size_t writeTo(T *data, size_t maxSize) const {
        size_t size = std::min(maxSize, m_size);
        std::copy(m_data, m_data + size, data);
        return size;
    }

  protected:
    T *m_data;
    size_t m_size;
    size_t m_maxSize;
};

template <typename T> class IsArray {
  private:
    template <typename C> static int check(Array<C> *);

    template <typename C> static char check(C *);

  public:
    static const bool value = (sizeof(check(static_cast<T *>(NULL))) > 1);
};

// Same as array of char, but ends with a null terminator.
class Octet : public Array<char> {
  public:
    Octet(char *value, size_t size) : Array<char>(value, size) {}

    void terminate() {
        if (m_size > 0) {
            m_data[m_size] = 0;
        }
    }

    void fillFrom(char const *data, size_t size) {
        Array<char>::fillFrom(data, size);
        terminate();
    }

    void fillFrom(std::string const &string) {
        fillFrom(string.data(), string.size());
    }

    size_t writeTo(char *data, size_t maxSize) const {
        size_t size = Array<char>::writeTo(data, maxSize);
        data[size] = 0;
        return size;
    }
};

struct ProcessInterrupts {
    enum ValueType {
        OFF,
        ON,
        DEFAULT,
    } value;

    ProcessInterrupts() : value(DEFAULT) {}

    ProcessInterrupts &operator=(bool v) {
        value = v ? ON : OFF;
        return *this;
    }

    bool operator==(ValueType v) const { return value == v; }
};

struct ResultBase {
    asynStatus status;
    epicsAlarmCondition alarmStatus;
    epicsAlarmSeverity alarmSeverity;
    ProcessInterrupts processInterrupts;

    ResultBase()
        : status(asynSuccess), alarmStatus(epicsAlarmNone),
          alarmSeverity(epicsSevNone), processInterrupts() {}
};

struct WriteResult : ResultBase {};

struct ArrayResult : ResultBase {};

template <typename T> struct Result : ResultBase {
    T value;

    Result() : ResultBase(), value() {}
};

template <> struct Result<Octet> : ResultBase {};

char const *getAsynTypeName(asynParamType type);

template <typename T> struct AsynType;

typedef asynStatus (*InterruptRegistrar)(PVInfo &, bool);

// The value member is only true when T = Array. This does not include Octet.
template <typename T, bool array = IsArray<T>::value> struct Handlers;

template <typename T> struct Handlers<T, false> {
    typedef Autoparam::WriteResult WriteResult;
    typedef Result<T> ReadResult;
    typedef WriteResult (*WriteHandler)(PVInfo &, T);
    typedef ReadResult (*ReadHandler)(PVInfo &);

    static const asynParamType type = AsynType<T>::value;
    WriteHandler writeHandler;
    ReadHandler readHandler;
    InterruptRegistrar intrRegistrar;

    Handlers() : writeHandler(NULL), readHandler(NULL), intrRegistrar(NULL) {}
};

template <typename T> struct Handlers<Array<T>, true> {
    typedef Autoparam::WriteResult WriteResult;
    typedef ArrayResult ReadResult;
    typedef WriteResult (*WriteHandler)(PVInfo &, Array<T> const &);
    typedef ReadResult (*ReadHandler)(PVInfo &, Array<T> &);

    static const asynParamType type = AsynType<Array<T> >::value;

    WriteHandler writeHandler;
    ReadHandler readHandler;
    InterruptRegistrar intrRegistrar;

    Handlers() : writeHandler(NULL), readHandler(NULL), intrRegistrar(NULL) {}
};

typedef Handlers<epicsInt32> Int32Handlers;
template <> struct AsynType<epicsInt32> {
    static const asynParamType value = asynParamInt32;
};

typedef Handlers<epicsInt64> Int64Handlers;
template <> struct AsynType<epicsInt64> {
    static const asynParamType value = asynParamInt64;
};

typedef Handlers<epicsUInt32> UInt32DigitalHandlers;
template <> struct AsynType<epicsUInt32> {
    static const asynParamType value = asynParamUInt32Digital;
};

typedef Handlers<epicsFloat64> Float64Handlers;
template <> struct AsynType<epicsFloat64> {
    static const asynParamType value = asynParamFloat64;
};
typedef Handlers<Octet> OctetHandlers;
template <> struct AsynType<Octet> {
    static const asynParamType value = asynParamOctet;
};
typedef Handlers<Array<epicsInt8> > Int8ArrayHandlers;
template <> struct AsynType<Array<epicsInt8> > {
    static const asynParamType value = asynParamInt8Array;
};

typedef Handlers<Array<epicsInt16> > Int16ArrayHandlers;
template <> struct AsynType<Array<epicsInt16> > {
    static const asynParamType value = asynParamInt16Array;
};

typedef Handlers<Array<epicsInt32> > Int32ArrayHandlers;
template <> struct AsynType<Array<epicsInt32> > {
    static const asynParamType value = asynParamInt32Array;
};

typedef Handlers<Array<epicsInt64> > Int64ArrayHandlers;
template <> struct AsynType<Array<epicsInt64> > {
    static const asynParamType value = asynParamInt64Array;
};

typedef Handlers<Array<epicsFloat32> > Float32ArrayHandlers;
template <> struct AsynType<Array<epicsFloat32> > {
    static const asynParamType value = asynParamFloat32Array;
};

typedef Handlers<Array<epicsFloat64> > Float64ArrayHandlers;
template <> struct AsynType<Array<epicsFloat64> > {
    static const asynParamType value = asynParamFloat64Array;
};

template <> struct Handlers<epicsUInt32, false> {
    typedef Autoparam::WriteResult WriteResult;
    typedef Result<epicsUInt32> ReadResult;
    typedef WriteResult (*WriteHandler)(PVInfo &, epicsUInt32, epicsUInt32);
    typedef ReadResult (*ReadHandler)(PVInfo &, epicsUInt32);

    static const asynParamType type = AsynType<epicsUInt32>::value;
    WriteHandler writeHandler;
    ReadHandler readHandler;
    InterruptRegistrar intrRegistrar;

    Handlers() : writeHandler(NULL), readHandler(NULL), intrRegistrar(NULL) {}
};

template <> struct Handlers<Octet, false> {
    typedef Autoparam::WriteResult WriteResult;
    typedef Result<Octet> ReadResult;
    typedef WriteResult (*WriteHandler)(PVInfo &, Octet const &);
    typedef ReadResult (*ReadHandler)(PVInfo &, Octet &);

    static const asynParamType type = AsynType<Octet>::value;

    WriteHandler writeHandler;
    ReadHandler readHandler;
    InterruptRegistrar intrRegistrar;

    Handlers() : writeHandler(NULL), readHandler(NULL), intrRegistrar(NULL) {}
};

namespace Convenience {
using Autoparam::Array;
using Autoparam::Octet;
using Autoparam::PVInfo;
using Autoparam::Result;
typedef Autoparam::WriteResult WriteResult;
typedef Autoparam::ArrayResult ArrayReadResult;
typedef Result<epicsInt32> Int32ReadResult;
typedef Result<epicsInt64> Int64ReadResult;
typedef Result<epicsUInt32> UInt32ReadResult;
typedef Result<epicsFloat64> Float64ReadResult;
typedef Result<Octet> OctetReadResult;
} // namespace Convenience

} // namespace Autoparam
