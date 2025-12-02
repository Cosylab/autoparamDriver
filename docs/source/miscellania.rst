.. SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
..
.. SPDX-License-Identifier: MIT

.. _miscellania:

Topics of interest
==================

Strings vs. arrays
------------------

Strings in ``asyn`` have two faces. When reading and writing data, they behave
like arrays. For that reason, ``autoparamDriver`` represents them with
:cpp:class:`Autoparam::Octet`, which derives from
:cpp:class:`Autoparam::Array\<char>`. However, unlike arrays, they are
represented by ``asynPortDriver`` parameters. This means that they are
propagated to ``I/O Intr`` records using
:cpp:func:`Autoparam::Driver::setParam()` instead of
:cpp:func:`Autoparam::Driver::doCallbacksArray()`.

Digital IO and unsigned integers
--------------------------------

``asyn`` only supports signed integers. It may be tempting to register handlers
for ``epicsUInt32`` to handle unsigned integers, but that is not the way to go:
that type is mapped to the ``asynUInt32Digital`` interface, which serves a
different purpose — I/O on specific bits of an integer register. For example, a
``bi`` record like this::

  record(bi, "$(PREFIX):bits_b3") {
      field(DESC, "A single bit")
      field(DTYP, "asynUInt32Digital")
      field(INP, "@asynMask($(PORT), 0, 0x8) DIGIO")
      field(ZNAM, "Down")
      field(ONAM, "Up")
  }

will pass the given mask, which only has bit 3 set, to the read handler. The
read handler can then either read the whole register from device and apply the
mask when returning the value, or, if the device supports it, fetch only the
requested bit. For writes, the handler also receives a mask, and it must only
modify the unmasked bits of the device register.

If you are doing "normal" integer I/O, you can only use signed integers. If the
device deals in 32-bit unsigned values where all 32 bits are used, you need to
use the ``epicsInt64`` type.

Arrays are a bit different. While e.g. the ``longin`` record is unsuitable for
unsigned 32-bit values larger than 2³¹-1, the ``waveform`` record supports
integers of all sizes, both signed and unsigned, by setting the FTVL field
appropriately. Again, ``asyn`` only supports signed types. But because no
arithmetic is done by ``asyn`` device support code, it is perfectly ok to push
unsigned data via signed integers of the same size. They will end up in the
``waveform`` record unchanged. Just be careful when converting endianness from
device order to host order, or if your driver code needs to do arithmetic.

Connection management
---------------------

By default, :cpp:class:`Autoparam::DriverOpts` enables the autoconnect
functionality. This is useful for the simplest case where your driver does no
connection management, or simply does its best to always stay connected. In this
case, the ``asyn`` port (which is the interface through which records and other
users talk to your driver) will always appear connected.

However, you may want to implement connection management to allow the port to
disconnect and connect to the device as needed. An example of where this is
useful is when the control system has states where some devices are completely
turned off. A state machine in the IOC needs to be able to connect and
disconnect in the appropriate state transitions.

To implement connection management, one needs to override the
``asynPortDriver::connect()`` and ``asynPortDriver::disconnect()`` functions,
then use :cpp:func:`Autoparam::DriverOpts::setAutoConnect()` to **disable
autoconnect**. It is important that autoconnect is disabled even if you do want
the driver to connect to the device immediately. The reason is that
``asynManager`` attempts to connect too early, before the driver is completely
constructed and the function overrides are in place.

With autoconnect disabled, there are several options on how to connect after the
driver is constructed:

* If you want to connect immediately, call either ``Driver::connect()`` or
  ``asynManager::autoConnect()`` at the end of the driver's constructor.

* If you want to connect automatically, but only after the records are
  initialized, do the above call at IOC init via
  :cpp:func:`Autoparam::DriverOpts::setInitHook()`.

* Do no connect automatically at all, but let the user initiate the connection
  as needed, via IOC shell command, a sequence program (using the ``asynCommon``
  or ``asynCommonSyncIO`` interfaces) or other means.

Background threads and cleanup
-------------------------------

Some devices require a background thread for efficient operation. This section
explains when background threads are appropriate, how to create them properly,
and how to ensure they are joined cleanly when the IOC exits.

.. rubric:: When background threads are needed

A background scanning thread is useful when:

* The device needs to be polled periodically to gather state.
* The device pushes data asynchronously and the driver needs to listen for it.
* Batch operations are more efficient than per-record access, so the driver
  fetches data in bulk and distributes it to multiple parameters.

.. rubric:: Creating joinable threads

Background threads must be joinable (not detached) to allow proper cleanup when
the IOC exits. The ``epicsThread`` C++ class creates joinable threads by
default, so you should use this class rather than the C API where creating a
joinable thread is more complicated. Using the ``std::thread`` class is
acceptable, however, EPICS API should be preferred because it allows you to
create named threads that can be manipulated using iocsh commands such as
``epicsThreadShow``.

* Store the ``epicsThread`` object as a member variable of your driver class.
* Start the thread by calling :cpp:func:`epicsThread::start()` at the end of
  the driver's constructor, after all other initialization is complete.

.. rubric:: Thread function principles

The thread function should:

* Run a loop that periodically checks a quit flag (a boolean member variable).
* Exit cleanly by returning from the function when the quit flag is set.
* Lock the driver before accessing any driver state or calling driver methods.

The :ref:`introduction <iointr>` explains how to use
:cpp:func:`Autoparam::Driver::getInterruptVariables()` to determine which device
variables the thread should scan.

.. rubric:: Thread safety and locking

Background threads must lock the driver before calling any driver methods or
accessing shared state. Use :cpp:func:`asynPortDriver::lock()` and
:cpp:func:`asynPortDriver::unlock()` for this purpose.

Always lock before calling:

* :cpp:func:`Autoparam::Driver::setParam()`
* :cpp:func:`asynPortDriver::callParamCallbacks()`
* :cpp:func:`Autoparam::Driver::doCallbacksArray()`

Also lock when checking the quit flag or accessing any other shared state. Note
that read and write handlers are already called with the driver locked, so you
don't need to lock again within handlers, but this is not the case for your
background thread. Keep the lock duration minimal to avoid blocking record
processing.

.. rubric:: Joining threads on IOC exit

The driver's destructor runs too late to safely stop background threads because
virtual functions are no longer available and the object is already partially
destroyed. To handle this, override :cpp:func:`Autoparam::Driver::shutdownPortDriver()`
in your derived class.

The cleanup sequence in ``shutdownPortDriver()`` should be:

1. Lock the driver with :cpp:func:`asynPortDriver::lock()`.
2. Set the quit flag to signal the thread to exit.
3. Unlock the driver with :cpp:func:`asynPortDriver::unlock()`.
4. Call :cpp:func:`epicsThread::exitWait()` to join the thread. This blocks
   until the thread returns from its function.
5. Call the base class :cpp:func:`Autoparam::Driver::shutdownPortDriver()`.

This method is called automatically when the IOC exits, provided you use
:cpp:func:`Autoparam::DriverOpts::setAutoDestruct()`.
