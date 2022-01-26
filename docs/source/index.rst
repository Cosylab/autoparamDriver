.. SPDX-FileCopyrightText: 2022 Cosylab d.d.
..
.. SPDX-License-Identifier: MIT

Welcome to autoparamDriver's documentation!
===========================================

``autoparamDriver`` is an EPICS module that facilitates writing a generic device
driver based on ``asyn`` that does not know ahead of time which (or how many)
parameters the device supports, delegating the job of defining them to the EPICS
database. If you've ever used EPICS modules like ``modbus`` or ``s7plc``, you
already know what this is about. If not, the :ref:`Introduction<introduction>`
gives an explanation, while the :doc:`tutorial` shows how to base your driver on
``autoparamDriver``.

``asyn`` documentation, while otherwise excellent, tries to be agnostic to the
control system and is thus a bit hard to follow for an EPICS developer. This
documentation takes a different approach and completely focuses on EPICS in the
hope that a developer will find it more approachable. That is not to say that it
is not necessary to be familiar with ``asyn``; in particular, the *Generic
Device Support for EPICS records* is a must-read. But ``autoparamDriver``
attempts to shield the developer from dealing with concepts like ``asynUser``
and various kinds of "reasons". To understand how ``autoparamDriver`` works from
``asyn`` point of view, refer to :doc:`design`.

.. toctree::
   tutorial
   design
   reference
   :maxdepth: 2
   :caption: Contents:

.. _introduction:

Introduction
============

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
