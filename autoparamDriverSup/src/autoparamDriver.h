// SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
//
// SPDX-License-Identifier: MIT

#pragma once

#include "autoparamHandler.h"
#include <map>
#include <stdexcept>

namespace Autoparam {

class Driver;

/*! Options controlling the behavior of `Driver`.
 *
 * Certain behaviors of `Driver` and the underlying `asynPortDriver` can be
 * controlled through `DriverOpts`. The value passed to the `Driver`'s
 * constructor can be created and modified in place, like so:
 *
 *     Driver(portName,
 *            DriverOpts().setBlocking(true)
 *                        .setAutoInterrupts(false)
 *                        .setPriority(epicsThreadPriorityLow));
 */
class DriverOpts {
  public:
    //! A function that can be set to run after IOC init.
    typedef void (*InitHook)(Driver *);

    /*! Declare whether read and write handlers can block.
     *
     * If any read or write handler can block in any situation, the driver needs
     * to declare this. What "blocking" means is explained in [EPICS Application
     * Developer's Guide](
     * https://epics.anl.gov/base/R3-16/2-docs/AppDevGuide.pdf) in chapter
     * *Device Support*.
     *
     * In short, if read and write handlers return "immediately", the driver
     * does not need to declare itself as blocking. On the other hand, if
     * handlers are "slow" (e.g. because the device is network-connected), the
     * driver **must** declare itself as blocking. This causes the EPICS device
     * support layer to implement asynchronous processing, calling read and
     * write handlers from a separate thread.
     *
     * Default: non-blocking
     */
    DriverOpts &setBlocking(bool enable = true) {
        if (enable) {
            asynFlags |= ASYN_CANBLOCK;
        } else {
            asynFlags &= ~ASYN_CANBLOCK;
        }
        return *this;
    }

    /*! Enable/disable asyn autoconnect functionality.
     *
     * Please refer to [asyn
     * documentation](https://epics.anl.gov/modules/soft/asyn/R4-38/asynDriver.html)
     * for more information. In short, if you do not override
     * `asynPortDriver::connect()` or `asynPortDriver::disconnect()`, enabling
     * autoconnect simply means that the asyn port will always appear connected,
     * which may be all you need.
     *
     * **Important:** if overriding `asynPortDriver::connect()`, you need to
     * know what you are doing. Be aware that autoconnect tries to connect
     * *before your driver is completely initialized*. This means that your code
     * *will not be called* when autoconnect is enabled. If you need to override
     * `asynPortDriver::connect()`, you may be better off disabling the
     * autoconnect option here and instead use the `asynManager` and
     * `asynCommonSyncIO` interfaces to connect (and/or enable autoconnect) from
     * your driver's constructor, which is executed later when virtual functions
     * have already been set up. See also `setInitHook()` for a way to connect
     * to the device even later, after all the records are initialized.
     *
     * Default: enabled
     */
    DriverOpts &setAutoConnect(bool enable = true) {
        autoConnect = enable;
        return *this;
    }

    /*! Instruct the driver to clean up on IOC exit.
     *
     * If enabled, the `Driver` will register a hook that is run at IOC exit and
     * deletes the `Driver`, which ensures that the destructor is run. This is
     * convenient because the `Driver` can be allocated using `new` from an
     * iocshell command, then let be.
     *
     * Note: the exit hook will disable the asyn port before destroying the
     * driver. The reason is that records can still be processed after the
     * driver is destroyed. Disabling the port prevents the driver being called,
     * though asyn may print warnings.
     *
     * Default: disabled
     */
    DriverOpts &setAutoDestruct(bool enable = true) {
        autoDestruct = enable;
        return *this;
    }

    /*! Enable/disable default `I/O Intr` behavior for write handlers.
     *
     * When enabled, successful writes will process `I/O Intr` records bound to
     * the parameter written to, unless overriden by
     * `ResultBase::processInterrupts`.
     *
     * Note that default write handlers (passed as `NULL` to
     * `Driver::registerHandlers()`) are not affected by this: the write handler
     * will always process interrupts.
     *
     * Default: enabled
     */
    DriverOpts &setAutoInterrupts(bool enable) {
        autoInterrupts = enable;
        return *this;
    }

    /*! Set the thread priority of read/write handlers in blocking mode.
     *
     * If `setBlocking()` was enabled, read and write handlers run in a separate
     * thread. This setting controls the priority of this thread.
     *
     * Default: `epicsThreadPriorityMedium`
     */
    DriverOpts &setPriority(int prio) {
        priority = prio;
        return *this;
    }

    /*! Set the thread stack size of read/write handlers in blocking mode.
     *
     * If `setBlocking()` was enabled, read and write handlers run in a separate
     * thread. This setting controls the priority of this thread.
     *
     * Default: `epicsThreadStackMedium`
     */
    DriverOpts &setStacksize(int size) {
        stackSize = size;
        return *this;
    }

    /*! Set a function to run after IOC initialization is done.
     *
     * If the driver needs to do something (like open communication to device)
     * *after* all the records (and consequently, `DeviceVariable`) are
     * constructed, registering a hook function here is the way to go.
     *
     * The hook is run after the IOC is built, but before any record processing
     * occurs. Specifically, it is hooked to
     * `initHookState::initHookAfterScanInit`
     *
     * Default: `NULL`
     */
    DriverOpts &setInitHook(InitHook hook = NULL) {
        initHook = hook;
        return *this;
    }

    // We have a fixed interface mask. Whether an interface is implemented or
    // not is decided implicitly by which handlers are registered. That's why we
    // enable all the relevant interfaces, and let the read and write functions
    // error out if there is no handler.
    static const int minimalInterfaceMask = asynCommonMask | asynDrvUserMask;
    static const int defaultMask =
        asynInt32Mask | asynInt64Mask | asynUInt32DigitalMask |
        asynFloat64Mask | asynOctetMask | asynInt8ArrayMask |
        asynInt16ArrayMask | asynInt32ArrayMask | asynInt64ArrayMask |
        asynFloat32ArrayMask | asynFloat64ArrayMask;

    DriverOpts()
        : interfaceMask(minimalInterfaceMask | defaultMask),
          interruptMask(defaultMask), asynFlags(0), autoConnect(1), priority(0),
          stackSize(0), autoDestruct(false), autoInterrupts(true),
          initHook(NULL) {}

  private:
    friend class Driver;

    int interfaceMask;
    int interruptMask;
    int asynFlags;
    int autoConnect;
    int priority;
    int stackSize;
    bool autoDestruct;
    bool autoInterrupts;
    InitHook initHook;
};

/*! An `asynPortDriver` that dynamically creates parameters referenced by
 * records.
 *
 * Normally, an `asynPortDriver` instantiates a predefined set of parameters,
 * each associated with a string that can subsequently be used to reference a
 * parameter from records in the EPICS database.
 *
 * `Autoparam::Driver` works differently. No parameters exist when the `Driver`
 * is constructed; instead, instances of `DeviceVariable` are created as EPICS
 * database records are initialized. The string a record uses to refer to a
 * parameter is split into a "function" and its "arguments" which, together,
 * define a "parameter". This is handled by the `DeviceAddress` and
 * `DeviceVariable` classes, and needs to be implemented by the class deriving
 * from `Driver`.
 *
 * Drivers based on `Autoparam::Driver` do not need to override the read and
 * write methods. Instead, they register read and write handlers for "functions"
 * used by records. `Autoparam::Driver` will then call these handlers when
 * records are processed.
 *
 * To facilitate updating `I/O Intr` records, two mechanisms are provided:
 *
 * - When a parameter is written to (or read from), the value can optionally be
 *   propagated to `I/O Intr` records bound to the same parameter. See
 *   `DriverOpts::setAutoInterrupts()` and `ResultBase::processInterrupts`.
 *
 * - The driver can process `I/O Intr` records at any time (e.g. from a
 *   background thread or in response to hardware interrupts) by
 *   - (scalars) setting the value using `Driver::setParam()`, then calling
 *     `asynPortDriver::callParamCallbacks()`;
 *   - (arrays) calling `Driver::doCallbacksArray()`.
 *
 * To create a new driver based on `Autoparam::Driver`:
 *   1. Create a derived class of `DeviceAddress`.
 *   2. Create a derived class of `DeviceVariable`.
 *   3. Create a derived class of `Driver`.
 *     - Implement the `parseDeviceAddress()` method to instantiate the derived
 *       `DeviceAddress`.
 *     - Implement the `createDeviceVariable()` method to instantiate the
 *       derived `DeviceVariable`.
 *     - Define static functions that will act as read and write handlers (see
 *       `Autoparam::Handlers` for signatures) and register them as handlers in
 *       the driver's constructor (c.f. `Driver::registerHandlers()`).
 *   4. Create one or more iocshell commands to instatiate and configure the
 *      driver.
 *
 * Apart from read and write functions, methods of `asynPortDriver` such as
 * `asynPortDriver::connect()` can be overriden as needed. To facilitate that,
 * `Driver::deviceVariableFromUser()` is provided to obtain `DeviceVariable`
 * from the `asynUser` pointer that `asynPortDriver` methods are provided.
 */
class Driver : public asynPortDriver {
  public:
    /*! Constructs the `Driver` with the given options.
     *
     * \param portName The user-provided name of the port used to refer to this
     *                 driver instance.
     * \param params   Options controlling the behavior of `Driver`.
     */
    explicit Driver(const char *portName, DriverOpts const &params);

    virtual ~Driver();

  protected:
    /*! Parse the given `function` and `arguments`.
     *
     * `DeviceAddress` is meant to be subclassed. As records are initialized,
     * `Driver` needs some information on the device variable referred to by
     * `function` and `arguments`, thus it calls this method.
     *
     * May return NULL on error.
     */
    virtual DeviceAddress *parseDeviceAddress(std::string const &function,
                                              std::string const &arguments) = 0;

    /*! Convert the given `DeviceVariable` into an instance of a derived class.
     *
     * `DeviceVariable` is meant to be subclassed. As records are initialized,
     * `Driver` creates instances of the `DeviceVariable` base class, then
     * passes them to this method to convert them to whichever subclass the
     * implementation decides to return.
     *
     * The `baseVar` pointer is intended to be passed to the constructor of
     * `DeviceVariable` which will take ownership of it.
     *
     * May return NULL on error.
     */
    virtual DeviceVariable *createDeviceVariable(DeviceVariable *baseVar) = 0;

    /*! Register handlers for the combination of `function` and type `T`.
     *
     * Note that the driver is implicitly locked when when handlers are called.
     *
     * \tparam T A type corresponding to one of asyn interfaces/parameter types.
     *         See `Autoparam::AsynType`. It determines which EPICS device
     *         support provided by asyn (determined by record's DTYP field) the
     *         `function` can be used with.
     *
     * \param function The name of the "function" (in the sense of "device
     *        function", see `DeviceVariable`).
     *
     * \param reader Handler function that is called when an input record
     *        referencing `function` with `DTYP` corresponding to `T` is
     *        processed;
     *
     * \param writer Handler function that is called when an output record
     *        referencing `function` with `DTYP` corresponding to `T` is
     *        processed;
     *
     * \param intrRegistrar A function that is called when a record referencing
     *        `function` switches to or from `I/O Intr`.
     */
    template <typename T>
    void registerHandlers(std::string const &function,
                          typename Handlers<T>::ReadHandler reader,
                          typename Handlers<T>::WriteHandler writer,
                          InterruptRegistrar intrRegistrar);

    /*! Propagate the array data to `I/O Intr` records bound to `var`.
     *
     * Unless this function is called from a read or write handler, the driver
     * needs to be locked. See `asynPortDriver::lock()`.
     *
     * Status and alarms of the records are set according to the same principles
     * as on completion of a write handler. See `Autoparam::ResultBase`.
     *
     * **Note:** strings are not arrays, even though `Autoparam::Octet` derives
     * from `Autoparam::Array`. Use `setParam()` and `callParamCallbacks()`
     * instead of `doCallbacksArray()`.
     */
    template <typename T>
    asynStatus doCallbacksArray(DeviceVariable const &var, Array<T> &value,
                                asynStatus status = asynSuccess,
                                int alarmStatus = epicsAlarmNone,
                                int alarmSeverity = epicsSevNone);

    /*! Set the value of the parameter represented by `var`.
     *
     * Unless this function is called from a read or write handler, the driver
     * needs to be locked. See `asynPortDriver::lock()`.
     *
     * Status and alarms of the records are set according to the same principles
     * as on completion of a write handler. See `Autoparam::ResultBase`.
     *
     * Unlike `doCallbacksArray()`, no `I/O Intr` records are processed. Use
     * `asynPortDriver::callParamCallbacks()` after setting the value. This
     * allows more than one parameter to have its value set before doing record
     * processing.
     */
    template <typename T>
    asynStatus setParam(DeviceVariable const &var, T value,
                        asynStatus status = asynSuccess,
                        int alarmStatus = epicsAlarmNone,
                        int alarmSeverity = epicsSevNone);

    /*! Set the value of the parameter represented by `var`.
     *
     * This is an overload for digital IO, where `mask` specifies which bits of
     * `value` are of interest. While the default overload works with
     * `epicsUInt32`, it uses the mask value `0xFFFFFFFF`.
     */
    asynStatus setParam(DeviceVariable const &var, epicsUInt32 value,
                        epicsUInt32 mask, asynStatus status = asynSuccess,
                        int alarmStatus = epicsAlarmNone,
                        int alarmSeverity = epicsSevNone);

    /*! Obtain a list of all device variables.
     *
     * This function is threadsafe, locking the driver is not necessary.
     */
    std::vector<DeviceVariable *> getAllVariables() const;

    /*! Obtain a list of device variables bound by `I/O Intr` records.
     *
     * The list of `DeviceVariable` pointers returned by this method is useful
     * if you need to implement periodic polling for data and would like to know
     * which data to poll. It is meant to be used together with
     * `doCallbacksArray()`, `setParam()` and
     * `asynPortDriver::callParamCallbacks()`.
     *
     * This function is threadsafe, locking the driver is not necessary.
     */
    std::vector<DeviceVariable *> getInterruptVariables();

    /*! Obtain a `DeviceVariable` given an `asynUser`.
     *
     * This facilitates overriding `asynPortDriver` methods if need be. Be
     * aware, though, that the `asynUser` structure is used in asyn to represent
     * any number of different things and the one you have may not correspond to
     * any `DeviceVariable`. The argument to `asynPortDriver::connect()` is an
     * example. Use of this method is subject to "know what you are doing"
     * constraints.
     */
    DeviceVariable *deviceVariableFromUser(asynUser *pasynUser);

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
    static void destroyDriver(void *driver);

    bool hasParam(int index);

    void handleResultStatus(asynUser *pasynUser, ResultBase const &result);

    template <typename IntType>
    void getInterruptVarsForInterface(std::vector<DeviceVariable *> &dest,
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

    typedef std::map<int, DeviceVariable *> ParamMap;
    ParamMap m_params;
    std::map<std::string, asynParamType> m_functionTypes;

    // Type erasure for function pointers.
    typedef void (*VoidFuncPtr)(void);
    std::map<asynParamType, std::pair<VoidFuncPtr, VoidFuncPtr> >
        m_originalIntrRegister;
    std::map<DeviceVariable *, int> m_interruptRefcount;

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
