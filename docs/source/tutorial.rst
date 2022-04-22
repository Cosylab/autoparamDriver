.. SPDX-FileCopyrightText: 2022 Cosylab d.d. https://www.cosylab.com
..
.. SPDX-License-Identifier: MIT

Tutorial
========

**IN PROGRESS**

To learn how to use ``autoparamDriver``, we will imagine a simple device and
create a small library to talk to it; the library will, naturally, generate mock
data. We will then write a driver to integrate it into EPICS. While all of this
could be presented as a finished example application, it is more instructive to
show each step separately and explain the reasoning behind it.

Creating an app
---------------

First, assuming that ``autoparamDriver`` is already installed in your EPICS
environment, let's create the EPICS application structure for our tutorial::

  $ mkdir autoparamTutorial
  $ cd autoparamTutorial
  $ makeBaseApp.pl -t ioc autoparamTutorial
  $ makeBaseApp.pl -i -t ioc autoparamTutorial

If you don't know what the above commands do, please check out the `Application
Developer's Manual`_.

.. _Application Developer's Manual: https://docs.epics-controls.org/en/latest/appdevguide/AppDevGuide.html

Modify the ``configuration/RELEASE`` file, setting the ``EPICS_BASE``, ``ASYN``
and ``AUTOPARAM`` variables to the locations where the respective modules are
installed.

Modify ``autoparamTutorialApp/src/Makefile``, adding::

  autoparamTutorialDBD += asyn.dbd
  autoparamTutorial_LIBS += autoparamDriver
  autoparamTutorial_LIBS += asyn
  autoparamTutorial_SRCS += tutorialDriver.cpp
  autoparamTutorial_SRCS += tutorialDevice.cpp

Modify ``autoparamTutorialApp/Db/Makefile``, adding::

  DB += tutorial.db

Create the source and database files, keeping them empty for now::

  touch autoparamTutorialApp/Db/tutorial.db autoparamTutorialApp/src/tutorial{Driver.cpp,Device.cpp,Device.h}

Run ``make``, then ``make distclean`` to ensure that the empty tutorial app
builds and that your EPICS environment is set up correctly. If these steps fail,
please explain how you previously managed to install ``autoparamDriver`` 😉

You are now all set up to start implementing the driver. From now on, all C++
driver code goes into ``tutorialDriver.cpp``, device simulation code goes into
``tutorialDevice.cpp`` and all database records go into ``tutorial.db``. Don't
forget to load the database in ``st.cmd``!

A mock device to work with
--------------------------
