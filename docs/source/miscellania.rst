.. SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
..
.. SPDX-License-Identifier: MIT

Topics of interest
==================

Strings vs. arrays
------------------

Strings in asyn have two faces. When reading and writing data, they behave like
arrays. For that reason, ``autoparamDriver`` represents them with
:cpp:class:`Autoparam::Octet`, which derives from
:cpp:class:`Autoparam::Array\<char>`. However, unlike arrays, they are
represented by ``asynPortDriver`` parameters. This means that they are
propagated to ``I/O Intr`` records using
:cpp:func:`Autoparam::Driver::setParam()` instead of
:cpp:func:`Autoparam::Driver::doCallbacksArray()`.

Digital IO and unsigned integers
--------------------------------

asyn only supports signed integers. It may be tempting to register handlers for
``epicsUInt32`` to handle unsigned integers, but that is not the way to go: that
type is mapped to the ``asynUInt32Digital`` interface, which serves a different
purpose — I/O on specific bits of an integer register. For example, a ``bi``
record like this::

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
device deals in 32-bit unsigned values where all 32-bits are used, you need to
use the ``epicsInt64`` type.

Arrays are a bit different. While e.g. the ``longin`` record is unsuitable for
unsigned 32-bit values larger than 2³¹-1, the ``waveform`` record supports
integers of all sizes, both signed and unsigned. Again, asyn only supports
signed types. But because no arithmetic is done, it is perfectly ok to push
unsigned data via signed integers of the same size. They will end up in the
``waveform`` record unchanged. Just be careful when converting endianness from
device order to host order.

Connection management
---------------------

By default, :cpp:class:`Autoparam::DriverOpts` enables the autoconnect
functionality. This is useful for the simplest case where your driver does no
connection management, or simply does its best to always stay connected. In this
case, the asyn port (which is the interface through which records and other
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
