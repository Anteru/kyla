Welcome to Kyla's documentation!
===============================

Contents:

.. toctree::
    :maxdepth: 2

Goals
=====

* Installing software packages consisting of many files, including very large files (hundred of MiB-several GiB)
* Robust deployment (all files are validated)
* Compressed installation media
* Installation from (static) web space or local media
* Easy generation
* Fast installation (interleaved download/decompress/validation)
* Initializing a feature with 25.000 files in a 100.000 file installation should take <10 seconds.

Non-goals
=========

* Integration into OS systems
* Complex post-installation custom actions
* Minimal patch sizes

Terms
=====

Source package
  A compressed file containing content objects. Content objects may be split across multiple source packages.

Content object
  Opaque data associated with one or more files. This is the actual payload installed by the installer.

Installation source
  Either a URL or a directory, containing *source packages*.

Installation database
  A database describing the installation package (source packages, files, etc.)

Deployment database
  Stores all installed files and their content hashes. Created during installation.

Installation target
  A folder on the target machine. The target is assumed to be read-only after the installation.

Layout mode
  Similar to MSI, the layout mode will create a local installation package from a web installation source

The storage uses content objects, which are indexed using the SHA512 hash. A content object is tied to it's hash. A content object may be stored across multiple source packages.

Design
======

The basic design revolves around a hash-addressed content store. That is, all files are references into this content store. A file consists of exactly one hash, but the same hash can be referenced by multiple files. The installer ensures that the target directory contains the file layout described in the package definition and that each file points at the correct hash.

The installer fetches the data from the installation source, which consists of multiple source packages. Each source package can be processed independently. For each file selected for installation, all source packages are fetched, and each SHA-index data is extracted to a temporary location. Extraction and verification happen concurrently with downloading, if possible. Eventually, the data is moved to the target location (if multiple files reference the same SHA-index, it is copied or symlink'ed, depending on the OS.) Additionally, the installer creates a installation database which stores which SHA hashes have been installed to which file (this allows to patch and repair an installation later, assuming the original package definition is available.)

Installation
------------

The installation consists of the following steps:

#. User selects which features to install
#. The files associated with the features are identified
#. The content objects required are identified
#. The source packages which contain the required content objects are identified
#. All source packages are processed. A source package can be streamed, and each content object can be decompressed/validated/reassembled in parallel.
#. Each content object is checked whether it is in the list of requested content objects, if so, it is decompressed to a temporary location. Content objects have unique file names (their hash), so no collisions can occur here.
#. The file list is traversed and content objects are moved to the first location.
#. The installation database is created.

Source package
--------------------

A source package consists of two parts: The package index and the data. The package index is simply a list of (SHA512, offset) to each entry in the package. The data is a list of (Header, Compressed-Data) chunks.
