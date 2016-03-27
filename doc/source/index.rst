Welcome to kyla's documentation!
================================

Contents:

.. toctree::

    tutorial
    repository-types

Introduction
------------

kyla is a file content management system. It handles file contents, similar to an installer, and can be used for deploying builds, patching and installation management. However, kyla is not a full installer. It does not handle shortcuts, registry entries or other setup.

Basic concepts
--------------

kyla has two main concepts:

* A **file repository**, which is the content source. kyla exclusively works on repositories. A file repository contains the contents of all files stored in it. Files are grouped in file sets. A repository can be *packed* into a few package files, or *loose* with all files being stored separately in the file system.
* A **file set** is a set of files that is processed together. Each file in a file set must be unique, and all sets in a file repository must be disjoint. For example, each feature of an application would be represented as a file set.

Overview
--------

kyla consists of three separate parts:

* ``kcl``, the **k**yla **c**ommand **l**ine binary. It includes the kyla compiler which takes a repository description and builds a *packed repository* from it, as well as basic routines to perform a deploy, repair and change of a kyla repository.
* ``pykyla``, the kyla utility library. A Python module which facilitates the repository description creation.
* ``libkyla``, the core library which contains all processing logic. ``libkyla`` provides a C API for clients for integration.
