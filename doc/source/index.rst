Welcome to kyla's documentation!
================================

Contents:

.. toctree::

    tutorial
    repository-definition
    repository-types

Introduction
------------

kyla is a file content management system. It handles file contents, similar to an installer, and can be used for deploying builds, patching and installation management. The main features are:

* kyla is fast: Files are only read sequentially, data is read or written only once if possible.
* kyla is reliable: It uses the `SQLite <https://sqlite.org>`_ storage engine for durability - one of the most robust databases in the world.
* kyla supports *feature-based* installation - install only parts of your application.
* kyla supports configure (i.e. adding/removing features), updating and downgrading. You can configure from one product to another and kyla will only update changed files - just like for a normal update.
* kyla supports various installation sources out of the box: Web deployment, installation packages, and loose files. kyla can also use an existing installation as a source.
* kyla provides repair and validation for installations and source packages alike.
* kyla works on file contents, not file names. No data is duplicated, and only content that changed is used. If you have a 10.000 file installation, and only one file changes between revisions, the update will only change this one file.

.. note::

    kyla is not a full-fledged installer taking care of registry keys, registering services, or similar. It is designed to deploy and manage applications in a single folder. If you need additional pre/post install hooks, you can easily build them on top of kyla.

Basic concepts
--------------

kyla has two main concepts:

* A **file repository**, which is the content source. kyla exclusively works on repositories. A file repository contains the contents of all files stored in it. Files are grouped in file sets. A repository can be either *packed* into a few package files, or be a *loose* repository with all file contents being stored separately in the file system.
* A **file set** is a set of files that is processed together. Each file in a file set must be unique, and all sets in a file repository must be disjoint. For example, each feature of an application would be represented as a file set.

Overview
--------

kyla consists of three separate parts:

* ``kcl``, the kyla command line binary. It includes the kyla compiler which takes a repository description and builds a *packed repository* from it, as well as basic routines to perform a deploy, repair and change of a kyla repository.
* ``pykyla``, the kyla utility library. A Python module which facilitates the repository description creation.
* ``libkyla``, the core library which contains all processing logic. ``libkyla`` provides a C API for clients for integration.
