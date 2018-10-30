Welcome to kyla's documentation!
================================

Contents:

.. toctree::
    :maxdepth: 1

    quickstart
    howto
    repository-definition
    repository-types
    comparison
    changelog
    build
    licenses

Introduction
------------

kyla is a library for "auto update" applications. It can be used as an installer, but the focus is quick installation and updates of large applications. The main features are:

* Speed: kyla uses fast compression algorithms, reads and writes files sequentially, and only fetches the data it absolutely needs.
* Scalability: Tens of thousands of files can be deployed easily. kyla also has first-class support for large binary files, splitting them up as necessary to provide streaming installations.
* Web-first: kyla can install directly from the internet. It fetches only the minimum amount of data required, and overlaps downloading and installing. This way there's only **one** progress bar, and no time and space is wasted on temporary files or waiting for large files to finish downloading.
* Feature-based installation: Deploy only subsets of your application, and support configure functionality.
* Upgrades, downgrades, configure: kyla can upgrade/downgrade your installation to another version. Only the data which **changed** will be transferred in this case. Upgrades, downgrades and configurations are handled through the same function.
* Library design: kyla is designed to be embedded into your frontend. It provides an easy-to-use C API and can be statically or dynamically linked.
* Reliability: It uses the `SQLite <https://sqlite.org>`_ storage engine for all metadata storage - one of the most robust databases in the world. Installations can be validated and repaired if they ever get corrupted.

.. note::

    kyla is not a full-fledged installer taking care of registry keys, registering services, or similar. It is designed to deploy and manage applications in a single folder. If you need additional pre/post install hooks, you can easily build them on top of kyla. For a comparison with existing tools, check out the :doc:`comparison`.

System requirements
-------------------

kyla has been tested on the following OS:

* Windows 10 x64
* Ubuntu 18.04

Other Windows/Linux variants should work, as kyla only relies on few cross-platform libraries, but they're not tested regularly.