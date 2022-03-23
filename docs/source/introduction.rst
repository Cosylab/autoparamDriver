.. SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
..
.. SPDX-License-Identifier: MIT

Introduction
============

Imagine a number of different devices that one would like to integrate into
EPICS. They use a common communication protocol or software library, but are
otherwise substantially different. A concrete example would be a number of PLCs
that have different roles and attached peripherals. The process variables (PVs)
exposed by each device are different, and although the interface library or
protocol allows one to reach all PVs, implementing separate EPICS support for
each device is tedious.

It is convenient to have generic EPICS device support implementing the common
protocol (or using the common library), then binding records to the PVs
available on each device solely through EPICS database record definitions.

Consider a fictitious example::

  record(ai, "$(PREFIX):DEV1:Temperature") {
      field(DTYP, "myDevSup")
      field(INP, "@dev=1 type=float addr=0x5042")
  }

  record(longin, "$(PREFIX):DEV1:Status") {
      field(DTYP, "myDevSup")
      field(INP, "@dev=1 type=short addr=0x50a1")
  }

  record(ai, "$(PREFIX):DEV2:Temperature") {
      field(DTYP, "myDevSup")
      field(INP, "@dev=2 type=float addr=0xfb03")
  }

The ``myDevSup`` EPICS device support layer and the underlying driver need know
nothing about the process variables, they merely know how to parse the string
given in the records' INP/OUT links and shuffle the requested data to and from
the registers at the specified addresses. Depending on how communication to the
device is implemented, it is even possible that no connection to a device exists
until a record requesting it is initialized.


.. _autoparam-features:

.. rubric:: Autoparam features

``autoparamDriver`` makes implementing such a generic driver easier by

* handling the first stage of parsing the INP/OUT links;
* dynamic creation of handles for each device process variable requested by
  EPICS records during IOC initialization;
* providing facilities for forwarding hardware interrupts to ``I/O Intr`` records;
* being based on ``asynPortDriver`` with all the benefits this brings — most
  importantly, generic EPICS device support layer with a number of useful
  features;
* supplementing ``asynPortDriver`` with a more homogeneous C++ interface

  * by allowing registration of handler functions instead of requiring the
    driver to override read and write methods and dispatch "manually".
  * by providing a templated `setParam()` in lieu of separate
    `setIntegerParam()`, `setDoubleParam()` etc.


Concepts and terminology
------------------------

.. contents::
   :local:

.. rubric:: See also

* `General asynDriver documentation`_
* `asynPortDriver class reference`_

.. _General asynDriver documentation: https://epics.anl.gov/modules/soft/asyn/R4-38/asynDriver.html#genericEpicsSupport
.. _asynPortDriver class reference: https://epics.anl.gov/modules/soft/asyn/R4-38/asynDoxygenHTML/classasyn_port_driver.html


PVs, records and reasons
^^^^^^^^^^^^^^^^^^^^^^^^

What is a process variable?
```````````````````````````

In the EPICS world, the term "process variable" (PV) is often used
interchangeably with "record". This has to do with the fact that record fields
get exposed to the network as individual process variables. However, these are
**not** the PVs referred to here. Instead, the term PV refers to a piece of data
exposed by a device. This piece of data may *or may not* end up in one *or more*
records. Thus, a device PV may or may not be accessible as one or more Channel
Access or PV Access PVs.

This documentation only refers to PVs as presented by a device, *never* PVs from
EPICS network point of view.

How does a record refer to a device PV?
```````````````````````````````````````

As ``autoparamDriver`` is based on ``asyn``, it leverages its generic device
support and INST_IO link parsing. It relies on ``asyn``'s ``DrvUser`` interface
to obtain a so-called "reason string". A record thus looks like::

  record(ai, "$(PREFIX):DEV1:Temperature") {
      field(DTYP, "asynFloat64")
      field(INP, "@asyn($(PORT_NAME)) this is a reason string")
  }

Here, the macro ``$(PORT_NAME)`` refers to an instance of a driver derived from
:cpp:class:`Autoparam::Driver`. When the record is initialized, the driver will
be given the entire reason string. It is split on spaces and the first word is
called a **function** while the rest are called **arguments**. To be more
explicit:

* function: "this"
* argument 1: "is"
* argument 2: "a"
* argument 3: "reason"
* argument 4: "string"

The combination of a function and its arguments is called a **process variable
(PV)**. Any record referring to the same combination of function and arguments
will thus bind to the same PV. For example, records referring to
``"@asyn(PORT) FUNC a=1 b=2 c=3"`` and ``"@asyn(PORT) FUNC a=1  b=2    c=3"``
will bind to the same PV (spaces are not important) while a record referring to
``"@asyn(PORT) FUNC a=1 c=3 b=2"`` will bind to a *different* PV.

Each PV is backed by a **parameter**. This term refers to asyn-managed cache of
PV properties (c.f. :cpp:func:`asynPortDriver::createParam()`), such as
general status, alarm status, and (for scalars) value. While handlers (described
below) are used to update records on request from the EPICS database, parameters
are used to update records on request from the driver (e.g. in response to
hardware interrupts).

How does the driver refer to a device PV?
`````````````````````````````````````````

As the IOC is initialized, the driver will automatically identify the requested
PVs and instantiate parameters. Instances of :cpp:class:`Autoparam::PVInfo`
serve as handles:

* when a record is processed, the driver is given a ``PVInfo`` identifying which
  PV the record is interested in;
* when the driver wants to update ``I/O Intr`` records asynchronously, it uses
  ``PVInfo`` to specify which parameters to update.

The :cpp:class:`Autoparam::PVInfo` class as used by the
:cpp:class:`Autoparam::Driver` base class does not do much: apart from being
used as a handle, it provides access to the function and its arguments as
strings, and that's it. However, ``PVInfo`` is polymorphic and it is expected
that the driver deriving from :cpp:class:`Autoparam::Driver` will deal with
subclasses of ``PVInfo``; see :cpp:func:`Autoparam::Driver::createPVInfo()`. The
subclass (or subclasses, there can be several) can contain anything the driver
needs to work with the PV, like device-specific argument parsing, hardware
interrupt subscription, etc.

Record processing
^^^^^^^^^^^^^^^^^

How does the driver react to record processing?
```````````````````````````````````````````````

A driver subclassing :cpp:class:`Autoparam::Driver` registers **handlers** for
functions by calling :cpp:func:`Autoparam::Driver::registerHandlers()` in its
constructor. The ``registerHandlers()`` method associates the combination of a
function name and a value type (see :cpp:class:`Autoparam::AsynType`) with a
read handler, a write handler and an interrupt registrar. The signatures depend
on the value type; they are grouped and documented in
:cpp:class:`Autoparam::Handlers` structures.

Handlers take a reference to :cpp:class:`Autoparam::PVInfo` as the first
argument. The task of a read handler is to read the value of the requested PV
from the device and return it (for scalars) or write it to the provided buffer
(for arrays/waveforms). The task of the write handler is to write the value
given as its second argument to the requested PV on the device.

Both read and write handlers can be ``NULL``. In this case, a default handler is
used. For scalars, the default read handler simply returns the value stored in
the parameter associated with the PV while the write handler stores the value
provided by the record in that same parameter. For arrays, both handlers return
an error since array parameters cannot store values themselves.

How does the driver process ``I/O Intr`` records?
`````````````````````````````````````````````````

There are three mechanisms that can be used to push values into ``I/O Intr``
records that are appropriate for different situations:

* during or after running write or read handlers,
* in response to hardware interrupts,
* or at any other time, in particular from a background scanning thread.

Which mechanism is appropriate depends on the device; they may also be combined.


.. rubric:: During or after running write or read handlers

By default, should the write handler for some PV complete successfully, the
driver will automatically update the cached parameter value and process the
callbacks registered by ``I/O Intr`` records that are bound to the same PV to
update them with *the written value*. This follows the behavior of default (i.e.
``NULL``) handlers and is appropriate when a PV is not really backed by
hardware, but is a "soft" PV in the driver.

It may also be appropriate when the PV is a "write-only" PV and does not allow
the driver to read back the value. In that case, the last written value is the
only data available, and updating the parameter after a write allows one to have
a ``NULL`` read handler that simply returns the last written data.

While the default (i.e. ``NULL``) write handler *always* behaves like this, this
automatic processing of interrupts can be overridden for normal handlers either

* globally by :cpp:func:`Autoparam::DriverOpts::setAutoInterrupts()`
* or on a per-write (or read) basis by setting
  :cpp:member:`Autoparam::ResultBase::processInterrupts`.

The latter also allows *reads* to update ``I/O Intr`` records bound to the same
PV. This is an edge use case and is thus not done by default, but the mechanism
is there and can be used explicitly.

A more common use case is a "write-read" operation which writes to the device
and obtains a readback of the value in the same transaction. The default
behavior of write handlers is not appropriate: while it does update the value of
``I/O Intr`` records, it uses the *value that was written*. To instead use the
value that was read back, the write handler should

* disable automatic processing of interrupts,
* then call :cpp:func:`Autoparam::Driver::setParam()`,
  :cpp:func:`asynPortDriver::callParamCallbacks()` or
  :cpp:func:`Autoparam::Driver::doArrayCallbacks()` itself.


.. rubric:: From a background scanning thread

The approach used for write-read operations is generally applicable and can be
used anywhere. In particular, some devices can only operate efficiently if data
is requested periodically in large batches, and the driver needs to do this kind
of update in a background thread. When data arrives, the background thread can
update many scalar parameters by calling
:cpp:func:`Autoparam::Driver::setParam()`, then call
:cpp:func:`asynPortDriver::callParamCallbacks()` once. For arrays,
:cpp:func:`Autoparam::Driver::doCallbacksArray()` does both operations at the
same time.

Note that handlers are called with the driver locked. When using the above
functions (or any other driver function, for that matter) from a different
context (such as a background thread), ensure that the driver is locked (see
:cpp:func:`asynPortDriver::lock()` and :cpp:func:`asynPortDriver::unlock()`).

To make it easier for the background thread to know which PVs are of interest,
:cpp:func:`Autoparam::Driver::getInterruptPVs()` returns a list of ``PVInfo``
that one or more records have subscribed to. Be aware that the list can change
at any time, both during database initialization and during runtime due to
``SCAN`` field changes.


.. rubric:: In response to hardware interrupts

Setting a parameter and calling the callbacks can be done in response to
hardware interrupts as well, in the same way as from a background thread.
However, hardware interrupts may need to be enabled, or, for network-connected
devices, an event subscription needs to be set up. This could, in principle, be
done by obtaining the list of required PVs using the
:cpp:func:`Autoparam::Driver::getInterruptPVs()` method. However, as this list
can change at any time, something would need to check the list periodically and
enable or disable the appropriate interrupts.

A more appropriate approach is to register a function that is called whenever a
record's ``SCAN`` field changes to or from ``I/O Intr``. Such an
:cpp:type:`Autoparam::InterruptRegistrar` can be registered together with read
and write handlers.
