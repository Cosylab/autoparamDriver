.. SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
..
.. SPDX-License-Identifier: MIT

autoparamDriver Release Notes
=============================

Development version
-------------------

No changes yet.

Version 2.1.0
-------------

* Added support for ``getParam()`` function to read parameter values.
* Improved port shutdown to use ``ASYN_DESTRUCTIBLE`` when available, with
  fallback to the existing approach for older asyn versions.
* The ``shutdowPortDriver()`` function is made available also on older asyn
  versions, keeping source compatibility.
* The ``Octet`` string wrapper is made safer to use by requiring the passed
  buffers to include null-termination of the string.

Version 2.0.0
-------------

* Added support for Windows and Visual Studio.
* Fixed issues with coexistence of multiple asynPortDrivers in the same IOC.

Version 1.0.0
-------------

This is the first public release.
