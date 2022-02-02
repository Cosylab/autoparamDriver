.. SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
..
.. SPDX-License-Identifier: MIT

API Reference
=============

.. Note: this reference is not automatically generated to allow manual selection
   of directive options for each documented object. In particular, listing
   undocumented menbers makes sense for some objects but not others. It also
   allows to have a different order of objects in documentation vs. code.

   When updating code, ensure that the index below is updated if needed.

All symbols of ``autoparamDriver`` are in the :cpp:any:`Autoparam` namespace. A
limited selection of symbols that are most needed when writing a driver based on
:cpp:class:`Autoparam::Driver` are put into the
:cpp:any:`Autoparam::Convenience` namespace which is meant to be imported with
a ``using`` directive.

.. contents::

.. rubric:: See also

* `General asynDriver documentation`_
* `asynPortDriver class reference`_

.. _General asynDriver documentation: https://epics.anl.gov/modules/soft/asyn/R4-38/asynDriver.html#genericEpicsSupport
.. _asynPortDriver class reference: https://epics.anl.gov/modules/soft/asyn/R4-38/asynDoxygenHTML/classasyn_port_driver.html

The driver
----------

.. doxygenclass:: Autoparam::Driver
.. doxygenclass:: Autoparam::DriverOpts

PV-specific information
-----------------------

.. doxygenclass:: Autoparam::PVInfo
   :undoc-members:

References to array data
------------------------

.. doxygenclass:: Autoparam::Array
   :undoc-members:

.. doxygenclass:: Autoparam::Octet
   :undoc-members:

Returning results from handlers
-------------------------------

.. doxygenstruct:: Autoparam::ProcessInterrupts

.. doxygenstruct:: Autoparam::ResultBase
.. doxygenstruct:: Autoparam::WriteResult
.. doxygenstruct:: Autoparam::ArrayResult
.. doxygenstruct:: Autoparam::Result
.. doxygenstruct:: Autoparam::Result< Octet >

Signatures of handler functions
-------------------------------

.. doxygentypedef:: Autoparam::InterruptRegistrar

.. doxygenstruct:: Autoparam::Handlers
.. doxygenstruct:: Autoparam::Handlers< T, false >
.. doxygenstruct:: Autoparam::Handlers< Array< T >, true >
.. doxygenstruct:: Autoparam::Handlers< epicsUInt32, false >
.. doxygenstruct:: Autoparam::Handlers< Octet, false >

Miscellania
-----------

.. doxygenfunction:: Autoparam::getAsynTypeName

.. doxygenstruct:: Autoparam::AsynType
   :undoc-members:

.. doxygennamespace:: Autoparam::Convenience
   :undoc-members:
