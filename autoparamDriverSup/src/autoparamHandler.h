#pragma once

// Standard includes
#include <string>
#include <vector>

// EPICS includes
#include <asynPortDriver.h>
#include <alarm.h>
#include <epicsTypes.h>

namespace Autoparam {

class PVInfo {
  public:
    typedef std::vector<std::string> ArgumentList;

    explicit PVInfo(char const *asynReason);

    PVInfo(PVInfo const &other);

    PVInfo &operator=(PVInfo const &other);

    virtual ~PVInfo();

    std::string const &function() const { return m_function; }

    ArgumentList const &arguments() const { return m_arguments; }

    std::string normalized() const;

  private:
    friend class Driver;

    void setIndex(int index) { m_asynParamIndex = index; }

    int index() const { return m_asynParamIndex; }

    int m_asynParamIndex;
    std::string m_function;
    ArgumentList m_arguments;
};

template <typename T> class Array {
  public:
    Array() : m_data(NULL), m_size(0) {}
    Array(T *value, size_t size) : m_data(value), m_size(size) {}
    Array(std::vector<T> &vector)
        : m_data(vector.data()), m_size(vector.size) {}

    T *data() const { return m_data; }

    size_t size() const { return m_size; }

  private:
    T *m_data;
    size_t m_size;
};

template <typename T> class IsArray {
  private:
    template <typename C> static int check(Array<C> *);

    template <typename C> static char check(C *);

  public:
    static const bool value = (sizeof(check(static_cast<T *>(NULL))) > 1);
};

struct ResultBase {
    asynStatus status;
    epicsAlarmCondition alarmStatus;
    epicsAlarmSeverity alarmSeverity;

    ResultBase()
        : status(asynSuccess), alarmStatus(epicsAlarmNone),
          alarmSeverity(epicsSevNone) {}
};

template <typename T = void> struct Result : ResultBase {
    T value;

    Result() : ResultBase(), value() {}
};

template <> struct Result<void> : ResultBase {};

template <> struct Result<epicsUInt32> : ResultBase {
    epicsUInt32 value;
    epicsUInt32 mask;

    Result() : value(), mask() {}
};

template <typename T> struct AsynType;
template <typename T, bool array = IsArray<T>::value> struct Handlers;

template <typename T> struct Handlers<T, false> {
    typedef Result<void> WriteResult;
    typedef Result<T> ReadResult;
    typedef WriteResult (*WriteHandler)(PVInfo &, T);
    typedef ReadResult (*ReadHandler)(PVInfo &);

    static const asynParamType type = AsynType<T>::value;
    WriteHandler writeHandler;
    ReadHandler readHandler;

    Handlers() : writeHandler(NULL), readHandler(NULL) {}
};

template <typename T> struct Handlers<Array<T>, true> {
    typedef Result<void> WriteResult;
    typedef Result<Array<T> > ReadResult;
    typedef WriteResult (*WriteHandler)(PVInfo &, Array<T>);
    typedef ReadResult (*ReadHandler)(PVInfo &, size_t);

    static const asynParamType type = AsynType<Array<T> >::value;

    WriteHandler writeHandler;
    ReadHandler readHandler;

    Handlers() : writeHandler(NULL), readHandler(NULL) {}
};

// TODO
struct Octet {};

typedef Handlers<epicsInt32> Int32Handlers;
template <> struct AsynType<epicsInt32> {
    static const asynParamType value = asynParamInt32;
    static char const *name;
};
char const *AsynType<epicsInt32>::name = "Int32";

typedef Handlers<epicsInt64> Int64Handlers;
template <> struct AsynType<epicsInt64> {
    static const asynParamType value = asynParamInt64;
    static char const *name;
};
char const *AsynType<epicsInt64>::name = "Int64";

typedef Handlers<epicsUInt32> UInt32DigitalHandlers;
template <> struct AsynType<epicsUInt32> {
    static const asynParamType value = asynParamUInt32Digital;
    static char const *name;
};
char const *AsynType<epicsUInt32>::name = "UInt32Digital";

typedef Handlers<epicsFloat64> Float64Handlers;
template <> struct AsynType<epicsFloat64> {
    static const asynParamType value = asynParamFloat64;
    static char const *name;
};
char const *AsynType<epicsFloat64>::name = "Float64";

typedef Handlers<Octet> OctetHandlers;
template <> struct AsynType<Octet> {
    static const asynParamType value = asynParamOctet;
    static char const *name;
};
char const *AsynType<Octet>::name = "Octet";

typedef Handlers<Array<epicsInt8> > Int8ArrayHandlers;
template <> struct AsynType<Array<epicsInt8> > {
    static const asynParamType value = asynParamInt8Array;
    static char const *name;
};
char const *AsynType<Array<epicsInt8> >::name = "Int8Array";

typedef Handlers<Array<epicsInt16> > Int16ArrayHandlers;
template <> struct AsynType<Array<epicsInt16> > {
    static const asynParamType value = asynParamInt16Array;
    static char const *name;
};
char const *AsynType<Array<epicsInt16> >::name = "Int16Array";

typedef Handlers<Array<epicsInt32> > Int32ArrayHandlers;
template <> struct AsynType<Array<epicsInt32> > {
    static const asynParamType value = asynParamInt32Array;
    static char const *name;
};
char const *AsynType<Array<epicsInt32> >::name = "Int32Array";

typedef Handlers<Array<epicsInt64> > Int64ArrayHandlers;
template <> struct AsynType<Array<epicsInt64> > {
    static const asynParamType value = asynParamInt64Array;
    static char const *name;
};
char const *AsynType<Array<epicsInt64> >::name = "Int64Array";

typedef Handlers<Array<epicsFloat32> > Float32ArrayHandlers;
template <> struct AsynType<Array<epicsFloat32> > {
    static const asynParamType value = asynParamFloat32Array;
    static char const *name;
};
char const *AsynType<Array<epicsFloat32> >::name = "Float32Array";

typedef Handlers<Array<epicsFloat64> > Float64ArrayHandlers;
template <> struct AsynType<Array<epicsFloat64> > {
    static const asynParamType value = asynParamFloat64Array;
    static char const *name;
};
char const *AsynType<Array<epicsFloat64> >::name = "Float64Array";

namespace Convenience {
using Autoparam::Array;
using Autoparam::PVInfo;
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
} // namespace Convenience

} // namespace Autoparam
