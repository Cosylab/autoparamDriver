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

The device is a controller connected to several peripherals. It has a generic
interface, meaning that the library through which we access it knows nothing
about these peripherals. It only deals with addresses and values stored at these
addresses. It can access the data either as a single 16-bit word (little endian)
or as an array of 8-bit values. It also provides access to hardware interrupts.

The interface for such a generic controller, defined in ``tutorialDevice.h``, is
simple::

  enum DeviceStatus {
      DeviceSuccess,
      DeviceError
  };

  typedef void (*InterruptCallback) (void *userData);

  DeviceStatus initDevice();
  DeviceStatus deinitDevice();

  DeviceStatus readWord(uint16_t address, uint16_t *value);
  DeviceStatus writeWord(uint16_t address, uint16_t value);

  DeviceStatus readBytes(uint16_t address, char *dest, uint16_t size);
  DeviceStatus writeBytes(uint16_t address, char const *data, uint16_t size);

  DeviceStatus enableInterruptCallback(uint8_t line, InterruptCallback callback, void *userData);
  DeviceStatus disableInterruptCallback(uint8_t line);
  DeviceStatus triggerSoftwareInterrupt(uint8_t line);

Implementing these functions is left as an exercise for the reader. Adding empty
stubs to ``tutorialDevice.cpp`` that simply return success should be enough for
the moment, allowing the code to compile and link. But it is more helpful if the
read functions return random data. It is also convenient if the
``triggerSoftwareInterrupt()`` function works and can be called from an IOC
shell command.

Before we start coding
----------------------

Before starting to code the driver, we need to think about how it will be used
to create an IOC. There are two aspects to this: the IOC shell script, and the
database. There's also a third point, the settings of the underlying
:cpp:class:`Autoparam::Driver`, which we will consider in the next section.

The IOC shell script is where the driver is instantiated. This is done by
creating a shell command; how to do this is documented in the `Application
Developer's Manual`_. Normally, the IOC shell command would take some parameters
and pass them to the driver's constructor. Thus, any settings that the user of
the driver (i.e. the IOC author) needs to be able to set, become parameters to
both the constructor and the user-accessible iocsh command. However, in our
case, we are dealing with a very simple device API and there is nothing the user
needs to configure. Still, one parameter is always required: the user-provided
name of the asyn port.

As for the database, because we don't know in advance which peripherals will be
connected to the controller, we need to provide a way for the records to access
all addresses. We need to come up with a syntax for the records' input and
output links. The following examples seem to make sense for our device at this
point::

  field(INP, "@asyn($(PORT) WORD 0x1234)")
  field(INP, "@asyn($(PORT) BYTES 0x1234 13)")
  field(INP, "@asyn($(PORT) INTR 5)")

Constructing the driver
-----------------------

Let us begin by defining our class in ``tutorialDriver.cpp``::

  #include <tutorialDevice.h>
  #include <autoparamDriver.h>

  class TutorialDriver : public Autoparam::Driver {
    public:
      TutorialDriver(char const *portName);
      ~TutorialDriver();
  };

Our device requires that the constructor calls ``initDevice()``. But that's not
all. It also needs to call the base class constructor. As it happens,
:cpp:func:`Autoparam::Driver::Driver()` supports a fair number of options. It's
time to take a look at :cpp:class:`Autoparam::DriverOpts` and pick what we need.

* Our device is simulated, all functions return instantly. Therefore, we do not
  need to declare our driver as blocking.
* Connection management in an :cpp:class:`asynPortDriver` is … not easy, and way
  beyond the scope of this tutorial. It's also not necessary in our case since
  the device is always connected. So it's best to keep autoconnect enabled; this
  way, the asyn port will always appear connected.
* Many EPICS drivers have a very cavalier attitude towards cleanup. But our
  simple device API offers us the option to do things properly. So let's enable
  autodestruct, which will delete our driver when the IOC shuts down.
* We will disable auto interrupts. The defaults match the default behavior of
  :cpp:class:`asynPortDriver`'s default read and write handlers, but such
  behavior is rarely needed with real hardware.
* We have no reason to change the thread priority and stack size. In fact, we
  don't declare the driver as blocking, so there's no thread in the first place.
* We don't need to install an init hook. Our driver does not need that
  additional stage of initialization.

After these considerations, the constructor looks like this::

  TutorialDriver::TutorialDriver(char const *portName)
      : Autoparam::Driver(
            portName,
            Autoparam::DriverOpts().setAutoDestruct().setAutoInterrupts(false)) {
      if (initDevice() == DeviceError) {
          asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "Error initializing device!");
      }
  }

Hopefully, the device API is implemented such that it keeps failing if not
properly initialized. If it is not, we need to track the initialization status
in the driver. In this tutorial, we won't bother.

To clean up after ourselves, we need a destructor::

  TutorialDriver::~TutorialDriver() {
      if (deinitDevice() == DeviceError) {
          asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "Error releasing device!");
      }
  }

To actually create an instance of the driver, we also need an iocsh command,
which requires a bit of boilerplate::

  static int const num_args = 1;
  static iocshArg const arg1 = {"port name", iocshArgString};
  static iocshArg const *const args[num_args] = {&arg1};
  static iocshFuncDef command = {"drvTutorialConfigure", num_args, args};

  static void call(iocshArgBuf const *args) { new TutorialDriver(args[0].sval); }

  extern "C" {

  static void tutorialDriverCommandRegistrar() { iocshRegister(&command, call); }

  epicsExportRegistrar(tutorialDriverCommandRegistrar);
  }

Don't forget to add the registrar to a ``dbd`` file, and to call the command
from the IOC shell. By the way, see how we allocated the driver with ``new``,
then threw away the pointer? The autodestruct option takes care of calling
``delete`` when the IOC exits.

The code won't build yet because our ``TutorialDriver`` is still an abstract
class: we have not yet implemented the functions that deal with parsing records'
input and output links. So let's get to it.

Parsing arguments and creating device variables
-----------------------------------------------

Because you have read (**TODO** insert ref), you already understand the concepts
of *device address* and *device variable*. Let's take a look at how to implement
them for our mock device.

Based on our considerations on what the ``INP`` field of a record might look
like, we see that our driver needs three distinct functions:

* ``WORD`` takes one argument, the variable address. The value there is an
  integer, so it makes sense to bind this function to the ``asynInt32``
  interface, represented by the ``epicsInt32`` type.
* ``BYTES`` takes two argument, an address and a length. The value is a byte
  array, so this function should be bound to the ``asynInt8Array``, represented
  by the ``Autoparam::Array<epicsInt8>`` type.
* ``INTR`` takes one argument, the interrupt line which identifies the source of
  interrupts. The API we are using can only notify us when an interrupt happens.
  As we are working on a generic driver, the best we can do is change some value
  that will cause a record to process. To do this, let's bind this function to
  the ``epicsInt32`` type to relay a counter to the EPICS layer.

We will see how to implement these device functions in the next section. Before
we can do that, we need some kind of handle that we can use to refer to data on
the device.

:cpp:class:`Autoparam::Driver` requires two steps to create a handle from an
``INP`` or an ``OUT`` field of a record. First, we need to derive a class from
:cpp:class:`Autoparam::DeviceAddress` and override
:cpp:func:`Autoparam::Driver::parseDeviceAddress()` to instantiate it.
Looking at the three functions we need to distinguish, the following should be
sufficient::

  using namespace Autoparam::Convenience;

  class TutorialAddress : public DeviceAddress {
    public:
      enum Type { Word, Bytes, Intr };

      Type type;
      epicsUInt16 address;
      epicsUInt16 size;

      bool operator==(DeviceAddress const& other) const {
          TutorialAddress const &o = static_cast<TutorialAddress const &>(other);
          if (type != o.type) return false;
          switch (type) {
              case Word:
              case Intr:
                  return address == o.address;
              case Bytes:
                  return address == o.address && size == o.size;
          }
      }
  };

Notice that we imported the :cpp:namespace:`Autoparam::Convenience` namespace,
which provides several often-needed symbols, such as ``DeviceAddress`` or
``Array``.

We have to provide the equality operator because that is required by the
``DeviceAddress`` interface. It is used by the Autoparam machinery to identify
records that refer to the same underlying variable. We could also provide a
constructor, but because this is a simple class where everything is public, this
can be delegated to the factory function which we need to implement anyway::

  DeviceAddress *TutorialDriverparseDeviceAddress(std::string const &function,
                                                  std::string const &arguments) {
      TutorialAddress *addr = new TutorialAddress;
      std::istringstream is(arguments);

      if (function == "WORD") {
          addr->type = TutorialAddress::Word;
          is >> addr->address;
      } else if (function == "BYTES") {
          addr->type = TutorialAddress::Word;
          is >> addr->address;
          is >> addr->size;
      } else if (function == "INTR") {
          addr->type = TutorialAddress::Word;
          is >> addr->address;
      } else {
          delete addr;
          return NULL;
      }

      return addr;
  }

This function is called with the string given in an ``INP`` or ``OUT`` field.
Parsing the provided arguments is very simple in our case. Even so, this
function is *too* simple: there is no error handling! It is elided for clarity,
but this code is dealing with user-provided strings, and mistakes happen often,
so in a real driver, make sure you check all arguments for validity!

Next, we implement the device variable handle based on
:cpp:class:`Autoparam::DeviceVariable`::

  class TutorialVariable : public DeviceVariable {
    public:
      TutorialVariable(TutorialDriver *driver, DeviceVariable *baseVar)
          : DeviceVariable(baseVar), driver(driver) {}
      TutorialDriver *driver;
  };

A bit of "magic" happens here. The only thing we may do with the ``baseVar``
pointer is to pass it to the base class constructor, which takes ownership of
that data. This also includes the ``TutorialAddress`` that is created in the
previous step, and is now available as
:cpp:func:`Autoparam::DeviceVariable::address()`. Our simple device doesn't need
more than this in the handle: the address and size are all that's needed to use
the device API.

But one thing that is *very* convenient to add is a pointer to the driver
instance that this handle is related to. You will see why in a moment, as we get
around to implementing handlers for our device functions. But first, we must not
forget to implement the function that creates our variable handles::

  DeviceVariable *TutorialDriver::createDeviceVariable(DeviceVariable *baseVar) {
      return new TutorialVariable(this, baseVar);
  }

With the two factory functions implemented, our driver is not an abstract class
anymore, and the program compiles.

Implementing device functions
-----------------------------

To declare which functions our driver supports, we provide handlers and register
them. The handlers are static functions which we add to the driver. The
declaration of our class now looks like this::

  class TutorialDriver : public Autoparam::Driver {
    public:
      TutorialDriver(char const *portName);
      ~TutorialDriver();

    protected:
      static Result<epicsInt32> wordReader(DeviceVariable &variable);
      static WriteResult wordWriter(DeviceVariable &variable, epicsInt32 value);
      static ArrayReadResult bytesReader(DeviceVariable &variable, Array<epicsInt8> &value);
      static WriteResult bytesWriter(DeviceVariable &variable, Array<epicsInt8> const &value);
      static Result<epicsInt32> intrReader(DeviceVariable &variable);
      static WriteResult intrWriter(DeviceVariable &variable, epicsInt32 value);
      static asynStatus intrRegistrar(DeviceVariable &variable, bool cancel);
  };

and the constructor is extended with the following calls::

  registerHandlers<epicsInt32>("WORD", wordReader, wordWriter, NULL);
  registerHandlers<Array<epicsInt8>>("BYTES", bytesReader, bytesWriter, NULL);
  registerHandlers<epicsInt32>("INTR", intrReader, intrWriter, intrRegistrar);
