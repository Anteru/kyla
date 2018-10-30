Building kyla
=============

Requirements
------------

* A modern C++ compiler: On Windows, Visual Studio 2017 or later, on Linux, GCC 6 or later
* CMake 3.8 or later
* Boost 1.63 or later. The ``filesystem``, ``program_options``, and ``system`` components must be available.
* OpenSSL 1.1 or later
* Python 3.5 or later
* Qt 5.6 or later. Qt 5.9 is recommended. kyla can be optionally built without the UI, in which case Qt is not required.

.. note::

    To build kyla without the UI, disable the ``KYLA_BUILD_UI`` option.

Build
-----

Run CMake to configure the project. Build the ``kcl`` target to get the kyla runtime.

.. note::

    The ``docs`` target currently supports Windows only due to the virtual environment setup.

Tests
^^^^^

kyla comes with a set of sanity tests which ensure basic functionality. To run the tests, build the ``RUN_TESTS`` target, or use ``CTest``.