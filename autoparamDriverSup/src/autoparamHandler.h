// SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
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

/*! Represents parsed device address information.
 *
 * The derived driver needs to subclass this and return it from the overridden
 * `Autoparam::Driver::parseDeviceAddress()`. It is intended for the derived
 * driver to store parsed function arguments here, e.g. numeric addresses and
 * offsets. It is used by the base `Driver` to identify which records refer to
 * the same device variable.
 *
 * Unlike `DeviceVariable`, this class should not take any device resources, or,
 * if unavoidable (e.g. because it needs to access the device for name
 * resolution), must release resources when destroyed; because several records
 * can refer to the same underlying variable, many instances of `DeviceAddress`
 * can be created per `DeviceVariable`, then destroyed even before the IOC is
 * fully initialized.
 *
 * A `DeviceAddress` must be equality-comparable to other addresses. Two
 * addresses shall compare equal when they refer to the same device variable.
 */
class DeviceAddress {
  public:
    virtual ~DeviceAddress() {}

    //! Compare to another address. Must be overridden.
    virtual bool operator==(DeviceAddress const &other) const = 0;
};

/*! Represents a device variable and is a handle for asyn parameters.
 *
 * This class is used as a handle referring to a device device variable, e.g. in
 * read and write handlers or `Driver::setParam()`.
 *
 * `DeviceVariable` is meant to be subclassed for use by the derived driver.
 * This greatly increases its utility as it can hold any information related to
 * a device variable that the derived driver might require. This is done via
 * `Driver::createDeviceVariable()`.
 *
 * `DeviceVariable` instances are created only once per device variable, but are
 * shared between records referring to the same device variable. They are
 * destroyed when the driver is destroyed.
 */
class DeviceVariable {
  public:
    /*! Construct `DeviceVariable` from another; the other one is invalidated.
     *
     * Being the only public constructor, this is the only way the driver
     * deriving from `Autoparam::Driver` can construct a `DeviceVariable`. The
     * usage pattern is the following:
     *
     * - The driver overrides `Autoparam::Driver::parseDeviceAddress()`. That
     *   method interprets the provided `function` and `arguments`, returning an
     *   instance of `DeviceAddress`.
     *
     * - The `Autoparam::Driver` base class creates an instance of
     *   `DeviceVariable` which contains the provided `DeviceAddress` instance
     *   and some internal data.
     *
     * - The driver overrides `Autoparam::Driver::createDeviceVariable()`. In
     *   that method, it uses the provided instance of the `DeviceVariable` base
     *   class and the previously created `DeviceAddress` (now available as
     *   `DeviceVariable::address()`) to instantiate a subclass of
     *   `DeviceVariable` that contains everything that the driver needs to
     *   access the underlying device variable.
     */
    explicit DeviceVariable(DeviceVariable *other);

    virtual ~DeviceVariable();

    //! Returns the "function" given in the record.
    std::string const &function() const { return m_function; }

    /*! Returns the "function+arguments" string representation.
     *
     * The resulting string is used for display only, e.g. in error messages.
     */
    std::string const &asString() const { return m_reasonString; };

    /*! Returns the index of the underlying asyn parameter.
     *
     * This allows advanced users to call methods of `asynPortDriver` if the
     * need arises.
     */
    int asynIndex() const { return m_asynParamIndex; }

    /*! Returns the type of the underlying asyn parameter.
     *
     * Apart from complementing `asynIndex()`, it allows the driver (or the
     * constructor of the derived class of `DeviceVariable`) to act differently
     * based on the type. While the derived driver can also determine this
     * information from `function()` (it knows which type each function handler
     * is registered for), using `asynType()` is faster and more convenient.
     */
    asynParamType asynType() const { return m_asynParamType; }

    /*! Returns the pre-parsed representation of the device address.
     *
     * This is the same instance of `DeviceAddress` that has been previously
     * created by `Driver::parseDeviceAddress()`.
     */
    DeviceAddress const &address() const { return *m_address; }

  private:
    friend class Driver;

    // Only the `Driver` has access to the reason string, so this constructor is
    // private. It also doesn't completely initialize `DeviceVariable`. That job
    // is up to `Driver::drvUserCreate()`. DeviceVariable takes ownership of the
    // provided DeviceAddress object.
    DeviceVariable(char const *reason, std::string const &function,
                   DeviceAddress *addr);

    std::string m_reasonString;
    std::string m_function;
    asynParamType m_asynParamType;
    int m_asynParamIndex;
    DeviceAddress *m_address;
};

/*! A non-owning reference to a data buffer.
 *
 * `Array` is used to pass around a reference to a contiguous buffer of type
 * `T`. For example, read and write handlers called by `Driver` receive an
 * `Array` as an argument, pointing to the data of a waveform record.
 *
 * An `Array` contains a data pointer, the current size of the buffer and the
 * maximum size of the buffer. It also provides convenience function to copy
 * data to and from other buffers.
 */
template <typename T> class Array {
  public:
    //! Construct an `Array` reference to `value`, setting its size to
    //! `maxSize`.
    Array(T *value, size_t maxSize)
        : m_data(value), m_size(maxSize), m_maxSize(maxSize) {
        if (value == NULL && maxSize != 0) {
            throw std::logic_error("Cannot create Autoparam::Array from NULL");
        }
    }

    T *data() const { return m_data; }

    size_t size() const { return m_size; }

    size_t maxSize() const { return m_maxSize; }

    void setSize(size_t size) { m_size = std::min(m_maxSize, size); }

    //! Set the size and copy data from the provided buffer.
    void fillFrom(T const *data, size_t size) {
        m_size = std::min(m_maxSize, size);
        std::copy(data, data + m_size, m_data);
    }

    //! Set the size and copy data from the provided vector.
    void fillFrom(std::vector<T> const &vector) {
        return fillFrom(vector.data(), vector.size());
    }

    //! Copy data to the provided buffer, up to `maxSize`.
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

// A SFINAE trick to determine if `T` is an `Array`, in which case,
// `IsArray::value` is `true`.
template <typename T> class IsArray {
  private:
    template <typename C> static int check(Array<C> *);

    template <typename C> static char check(C *);

  public:
    static const bool value = (sizeof(check(static_cast<T *>(NULL))) > 1);
};

/*! A specialization of `Array` to deal with string data.
 *
 * This class is called `Octet` instead of `String` to match the asyn
 * nomenclature. It is an `Array` of `char` and provides convenience function to
 * ensure null-termination required by C strings.
 */
class Octet : public Array<char> {
  public:
    //! Construct an Octet reference to `value`, setting it's size to `maxSize`.
    Octet(char *value, size_t maxSize) : Array<char>(value, maxSize) {}

    //! Terminate the string at its current size.
    void terminate() {
        if (m_size > 0) {
            m_data[m_size] = 0;
        }
    }

    //! Set the size, copy data from the provided buffer and null-terminate.
    void fillFrom(char const *data, size_t size) {
        Array<char>::fillFrom(data, size);
        terminate();
    }

    //! Set the size, copy data from the provided string and null-terminate.
    void fillFrom(std::string const &string) {
        fillFrom(string.data(), string.size());
    }

    //! Copy data to the provided buffer, up to `maxSize`, with
    //! null-termination.
    size_t writeTo(char *data, size_t maxSize) const {
        size_t size = Array<char>::writeTo(data, maxSize);
        data[size] = 0;
        return size;
    }
};

/*! A tri-state determining whether `I/O Intr` records should be processed.
 *
 * Used in `ResultBase` to determine whether interrupts should be processed.
 * When left alone, it specifies the default behavior. When a `bool` is assigned
 * to it, it overrides the default.
 *
 * \sa `ResultBase::processInterrupts`
 */
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

/*! The result returned from a read or write handler.
 *
 * `ResultBase` tells the `Driver` calling a read or write handler whether the
 * call was successful and how to proceed. Based on this, the `Driver` will set
 * the appropriate alarm status on the EPICS record that caused the call.
 *
 * The default-constructed result represents a successful handling; thus, in the
 * happy case, the handler need not change anything. If something went wrong,
 * the handler as a fair bit of freedom in deciding what will happen to the
 * record that caused processing.
 */
struct ResultBase {
    /*! The overall status of read/write handling.
     *
     * If `status` is set to `asynSuccess` (the default) upon returning from a
     * handler, interrupts may be processed (see
     * `ResultBase::processInterrupts`).
     *
     * If `status` is set to anything else except `asynSuccess`, interrupts will
     * not be processed.
     *
     * For read handlers, the value read will be passed to the record regardless
     * of `status`.
     *
     * Unless `ResultBase::alarmStatus` and `ResultBase::alarmSeverity` are also
     * set, the record's alarm and severity are determined according to the
     * value of `status` and the type of record. For example, on `asynError`, an
     * input record will be put into `READ_ALARM` and an output record will be
     * put into `WRITE_ALARM`.
     */
    asynStatus status;

    //! Overrides the record's alarm status.
    epicsAlarmCondition alarmStatus;
    //! Overrides the record's severity status.
    epicsAlarmSeverity alarmSeverity;

    /*! Determines whether interrupts should be processed on success.
     *
     * When a read or write handler finishes with `asynSuccess`, it may be
     * appropriate to process `I/O Intr` records that are bound to the same
     * parameter. The decision can be done globally via
     * `Autoparam::DriverOpts::setAutoInterrupts`, but can always be overriden
     * by a handler by setting `ResultBase::processInterrupts`.
     *
     * The default setting follows the default behavior of `asynPortDriver`:
     *   - do not process interrupts upon returning from a read handler;
     *   - process interrupts upon returning from a write handle, propagating
     *     the value just written.
     *
     * To override the defaults, simply set `processInterrupts` to `true` or
     * `false`.
     */
    ProcessInterrupts processInterrupts;

    ResultBase()
        : status(asynSuccess), alarmStatus(epicsAlarmNone),
          alarmSeverity(epicsSevNone), processInterrupts() {}
};

//! %Result returned from a write handler, status only.
struct WriteResult : ResultBase {};

//! %Result returned from an array read handler, status only.
struct ArrayResult : ResultBase {};

//! %Result returned from a scalar read handler, status and value.
template <typename T> struct Result : ResultBase {
    //! The value returned by the read handler.
    T value;

    Result() : ResultBase(), value() {}
};

/*! %Result returned from `Octet` read handler, status only.
 *
 * Octets behave like arrays in this respect.
 */
template <> struct Result<Octet> : ResultBase {};

//! Return string representation of the given asyn parameter type.
char const *getAsynTypeName(asynParamType type);

/*! Maps type `T` to the corresponding `asynParamType` value.
 *
 * This mapping allows using EPICS types everywhere as template arguments and
 * types of function arguments. It is defined as follows:
 *
 *   - `epicsInt32` → `asynParamInt32`
 *   - `epicsInt64` → `asynParamInt64`
 *   - `epicsFloat64` → `asynParamFloat64`
 *   - `epicsUint32` → `asynParamUint32Digital`
 *   - `Octet` → `asynParamOctet`
 *   - `Array<epicsInt8>` → `asynParamInt8Array`
 *   - `Array<epicsInt16>` → `asynParamInt16Array`
 *   - `Array<epicsInt32>` → `asynParamInt32Array`
 *   - `Array<epicsInt64>` → `asynParamInt64Array`
 *   - `Array<epicsFloat32>` → `asynParamFloat32Array`
 *   - `Array<epicsFloat64>` → `asynParamFloat64Array`
 */
#ifdef DOXYGEN_RUNNING
template <typename T> struct AsynType { static const asynParamType value; };
#else
template <typename T> struct AsynType;
#endif

/*! Called when a device variable switches to or from `I/O Intr` scanning.
 *
 * The registrar function is called both when a variable switches to `I/O Intr`
 * and when it switches away; the `cancel` argument reflects that, being `false`
 * in the former case and `true` in the latter. The purpose of the registrar
 * function is to set up or tear down a subscription for events (or interrupts)
 * relevant to the given `var`.
 *
 * To be more precise: a device variable can be referred to by several EPICS
 * records, any number of which can be set to `I/O Intr` scanning. This function
 * is called with `cancel = false` when the number of `I/O Intr` records
 * increases to 1, and with `cancel = true` when the number decreases to 0.
 */
typedef asynStatus (*InterruptRegistrar)(DeviceVariable &var, bool cancel);

/*! Handler signatures for type `T`.
 *
 * Specializations of this struct describe the signatures of the read handler,
 * write handler, and interrupt registrar for the given type `T`. Functions with
 * these signatures are passed as arguments to Driver::registerHandlers().
 *
 * This struct is defined separately for scalars, arrays, strings (`Octet`s) and
 * digital IO. See the specializations for concrete signatures.
 */
// The value member is only true when T = Array. This does not include Octet.
template <typename T, bool array = IsArray<T>::value> struct Handlers;

//! Signatures of handlers for scalar types `T`.
template <typename T> struct Handlers<T, false> {
    typedef Autoparam::WriteResult WriteResult;
    //! %Result type for scalar reads.
    typedef Result<T> ReadResult;
    //! Writes `value` to the device.
    typedef WriteResult (*WriteHandler)(DeviceVariable &var, T value);
    //! Reads a value from the device, returning it inside `ReadResult`.
    typedef ReadResult (*ReadHandler)(DeviceVariable &var);

    static const asynParamType type = AsynType<T>::value;
    WriteHandler writeHandler;
    ReadHandler readHandler;
    InterruptRegistrar intrRegistrar;

    Handlers() : writeHandler(NULL), readHandler(NULL), intrRegistrar(NULL) {}
};

//! Signatures of handlers for array types `Array<T>`.
template <typename T> struct Handlers<Array<T>, true> {
    typedef Autoparam::WriteResult WriteResult;
    //! %Result type for array reads.
    typedef ArrayResult ReadResult;
    //! Writes `value` to the device.
    typedef WriteResult (*WriteHandler)(DeviceVariable &var,
                                        Array<T> const &value);
    /*! Reads an array from the device, storing data in `value`.
     *
     * Unlike a scalar read handler, the value is *not* returned as a
     * `ReadResult`, but written directly to the given buffer, up to the amount
     * returned by `value.maxSize()`.
     */
    typedef ReadResult (*ReadHandler)(DeviceVariable &var, Array<T> &value);

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

/*! Signatures of handlers for `epicsUInt32`.
 *
 * `epicsUInt32` (a.k.a. `asynParamUInt32Digital`) is used for digital (i.e.
 * bit-level) IO. As such, its handlers are passed an additional parameter
 * `mask`. This mask tells the handler which bits the caller is interested in.
 * It's up to the handler to properly mask the value.
 */
template <> struct Handlers<epicsUInt32, false> {
    typedef Autoparam::WriteResult WriteResult;
    //! %Result type for scalar reads.
    typedef Result<epicsUInt32> ReadResult;
    //! Writes `value` to the device, honoring the given `mask`.
    typedef WriteResult (*WriteHandler)(DeviceVariable &var, epicsUInt32 value,
                                        epicsUInt32 mask);
    //! Reads a value from the device, honoring `mask`, returning it inside
    //! `ReadResult`.
    typedef ReadResult (*ReadHandler)(DeviceVariable &var, epicsUInt32 mask);

    static const asynParamType type = AsynType<epicsUInt32>::value;
    WriteHandler writeHandler;
    ReadHandler readHandler;
    InterruptRegistrar intrRegistrar;

    Handlers() : writeHandler(NULL), readHandler(NULL), intrRegistrar(NULL) {}
};

/*! Signatures of handlers for `Octet`.
 *
 * For the purpose of read and write handlers, `Octet` behaves as an array.
 */
template <> struct Handlers<Octet, false> {
    typedef Autoparam::WriteResult WriteResult;
    //! %Result type for `Octet` reads (essentially `ArrayResult`).
    typedef Result<Octet> ReadResult;
    //! Writes `value` to the device.
    typedef WriteResult (*WriteHandler)(DeviceVariable &var,
                                        Octet const &value);
    //! Reads a string from the device, storing data in `value`.
    typedef ReadResult (*ReadHandler)(DeviceVariable &var, Octet &value);

    static const asynParamType type = AsynType<Octet>::value;

    WriteHandler writeHandler;
    ReadHandler readHandler;
    InterruptRegistrar intrRegistrar;

    Handlers() : writeHandler(NULL), readHandler(NULL), intrRegistrar(NULL) {}
};

/*! Symbols that are often needed when implementing drivers.
 *
 * This namespace is meant to be used as
 *
 *     using namespace Autoparam::Convenience;
 *
 * This makes the symbols declared herein easily accessible. Apart from the
 * typdefs shown below, this namespace exposes also:
 *   - `Autoparam::DeviceAddress`
 *   - `Autoparam::DeviceVariable`
 *   - `Autoparam::Array`
 *   - `Autoparam::Octet`
 *   - `Autoparam::Result`
 */
namespace Convenience {
// using directives are not picked up by doxygen
using Autoparam::Array;
using Autoparam::DeviceAddress;
using Autoparam::DeviceVariable;
using Autoparam::Octet;
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
