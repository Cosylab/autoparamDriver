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
* providing facilities for forwarding hardware interrupts to ``IO Intr`` records;
* being based on ``asynPortDriver`` with all the benefits this brings — most
  importantly, generic EPICS device support layer with a number of useful
  features;
* supplementing ``asynPortDriver`` with a more homogeneous C++ interface

  * by allowing registration of handler functions instead of requiring the
    driver to override read and write methods and dispatch "manually".
  * by providing a templated `setParam()` in lieu of separate
    `setIntegerParam()`, `setDoubleParam()` etc.

Concepts
--------
