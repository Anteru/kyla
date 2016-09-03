kyla
====

kyla is an installation framework. Check out `the documentation <http://kyla.readthedocs.io/en/latest/>`_ as well as this `introductory blog post <https://anteru.net/blog/2016/introducing-kyla-part-1/>`_ for an overview.

License
-------

kyla is provided under the BSD license. See ``doc/source/liceses.rst`` for license information.

Building kyla
-------------

kyla requires Visual Studio 2015 Update 3 or GCC/Clang to build. The following external dependencies are also required:

* `Boost <http://www.boost.org/>`_, configured with ``program_options``, ``filesytem``.
* `OpenSSL <https://www.openssl.org/>`_
* `Qt 5.6 <https://www.qt.io/>`_ or later for the UI
* `Python 3.5 <https://python.org>`_ for build infrastructure and the Python bindings

The build system using `CMake <https://cmake.org/>`_.

Building the tutorial
^^^^^^^^^^^^^^^^^^^^^

The tutorial files described in the documentation can be invoked by calling ``samples/glfw/create.py``.
