.. SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
..
.. SPDX-License-Identifier: MIT

Welcome to autoparamDriver's documentation!
===========================================

``autoparamDriver`` is an EPICS module that facilitates writing a generic device
driver based on ``asyn`` that does not know ahead of time which (or how many)
parameters the device supports, delegating the job of defining them to the EPICS
database. If you've ever used EPICS modules like ``modbus`` or ``s7plc``, you
already know what this is about. If not, the :doc:`introduction` gives an
explanation, while the :doc:`tutorial` shows how to base your driver on
:cpp:class:`Autoparam::Driver`. Otherwise, jump directly to the list of
:ref:`autoparam-features`.

``asyn`` documentation, while otherwise excellent, tries to be agnostic with
respect to different users of ``asyn`` interfaces and is thus a bit hard to
follow for an EPICS developer. This documentation takes a different approach and
takes an EPICS-centric view in the hope that a developer will find it more
approachable. Moreover, ``autoparamDriver`` attempts to shield the developer
from dealing directly with heavily overloaded concepts like ``asynUser`` and
various kinds of "reasons".

That is not to say that it is not necessary to be familiar with ``asyn``; in
particular, the section *Generic Device Support for EPICS records* in the
`asynDriver documentation`_ is a must-read. ``autoparamDriver`` helps implement
all interfaces except ``Enum`` and ``GenericPointer``; for these two, it won't
help, but it won't hinder you either. Moreover, it relies on the ``DrvUser``
interface, which is how ``asyn`` passes additional information from an EPICS
record to the driver. Relying on this means, for example, that ``asynOctetRead``
EPICS device support will work, but ``asynOctetCmdResponse`` will not. This is
not considered a limitation: if you need a driver that doesn't rely on the
``DrvUser`` interface, ``autoparamDriver`` is not a good fit for your use case
anyway.

To further understand how ``autoparamDriver`` works from ``asyn`` point of view,
refer to :doc:`design`.

.. _asynDriver documentation: https://epics.anl.gov/modules/soft/asyn/R4-38/asynDriver.html#genericEpicsSupport

.. toctree::
   introduction
   tutorial
   design
   reference
   :maxdepth: 2
   :caption: Contents:

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
