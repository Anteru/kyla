Welcome to kyla's documentation!
================================

Contents:

.. toctree::

    tutorial
    concepts
    repository-definition
    repository-types
    comparison
    licenses

Introduction
------------

kyla is a file content management system. It handles file contents, similar to an installer, and can be used for deploying builds, patching and installation management. The main features are:

* kyla is fast: Files are only read sequentially, data is read or written only once if possible.
* kyla is reliable: It uses the `SQLite <https://sqlite.org>`_ storage engine for durability - one of the most robust databases in the world.
* kyla supports *feature-based* installations: Only parts of the application can be deployed.
* kyla supports configure -- i.e. adding/removing features, as well as updating from one version to another. In fact, you can configure from one product to another and kyla will only update changed files.
* kyla supports various installation sources out of the box: Web deployment, installation packages, and loose files. kyla can also use an existing installation as a source.
* kyla provides repair and validation for installations and source packages alike.
* kyla works on file contents, not paths. No data is duplicated, and only content that changed is used. If you have a 10.000 file installation, and only one file changes between revisions, the update will only change this one file.

.. note::

    kyla is not a full-fledged installer taking care of registry keys, registering services, or similar. It is designed to deploy and manage applications in a single folder. If you need additional pre/post install hooks, you can easily build them on top of kyla. For a comparison with existing tools, check out the :doc:`comparison`.

Overview
--------

kyla consists of three separate parts:

* ``kcl``, the kyla command line binary. It includes a command line front-end to the API, as well as access to the kyla build API.
* ``kui``, the kyla UI. This is a small UI which can perform an installation, up/downgrades, and configure operations.
* ``pykyla``, the kyla utility library. A Python module which facilitates the repository description creation.
* ``libkyla``, the core library which contains all processing logic. ``libkyla`` provides a C API for clients for integration.

For a quick start, check out the :doc:`tutorial`. For a deeper introduction, head over to the :doc:`concepts`.
