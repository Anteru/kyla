Welcome to kyla's documentation!
================================

Contents:

.. toctree::
    :maxdepth: 1

    repository-definition
    repository-types
    comparison
    changelog
    licenses

Introduction
------------

kyla is an installation system designed to deploy large amounts data. The main features are:

* Speed: kyla uses fast compression algorithms, reads and writes files sequentially, and only fetches the data it absolutely needs.
* Scalability: Tens of thousands of files can be deployed easily. kyla also has first-class support for large binary files, splitting them up as necessary to provide streaming installations.
* Web-first: kyla can install directly from the internet, fetches only the minimum amount of data required, and installs during download to maximize bandwidth usage and minimize installation time.
* Feature-based installation: Deploy only subsets of your application, and support configure functionality.
* Upgrades, downgrades, configure: kyla can upgrade/downgrade your installation to another version and will only fetch changed contents. Upgrades, downgrades and configurations are handled through the same function.
* Library design: kyla is designed to be embedded into your frontend. It provides an easy-to-use C API and can be statically or dynamically linked.
* Reliability: It uses the `SQLite <https://sqlite.org>`_ storage engine for all metadata storage - one of the most robust databases in the world. Installations can be validated and repaired if they ever get corrupted.

.. note::

    kyla is not a full-fledged installer taking care of registry keys, registering services, or similar. It is designed to deploy and manage applications in a single folder. If you need additional pre/post install hooks, you can easily build them on top of kyla. For a comparison with existing tools, check out the :doc:`comparison`.

Overview
--------

kyla consists of three separate parts:

* ``kcl``, the kyla command line binary. It includes a command line front-end to the API, as well as access to the kyla build API.
* ``kui``, the kyla UI. This is a small UI which can perform an installation, up/downgrades, and configure operations.
* ``pykyla``, the kyla utility library. A Python module which facilitates the repository description creation.
* ``libkyla``, the core library which contains all processing logic. ``libkyla`` provides a C API for clients for integration.

Quick start
-----------

The fastest -- and also the recommended way -- to get started with kyla is to use the Python bindings to create a build definition. A simple installer could look like this:

.. code:: Python

    import kyla

    rb = kyla.RepositoryBuilder ()

    mainFeature = rb.AddFeature ()

    binaryFiles = rb.AddFileGroup ()
    binaryFiles.AddDirectory ('path/to/bin', outputDirectory = 'bin')

    # We want to store the binary files in their own package
    binPackage = rb.AddFilePackage ('bin')

    # Link the binary package to the binary files
    binPackage.AddReference (binaryFiles)

    # Our main feature consists of only the binary files -- link them together
    mainFeature.AddReference (binaryFiles)

    # For the UI, we want a single node feature tree
    featureTreeMainNode = rb.AddFeatureTreeNode ('Binaries', 
        'These are the binaries')

    # link the feature tree node to the feature. If the user selects the node,
    # all linked features will be installed
    featureTreeMainNode.AddReference (mainFeature)

    open ('desc.xml', 'w').write (rb.Finalize())

The Python bindings hide the complexity of referencing nodes and handle all ids internally. The generated Xml file can be then processed using ``kcl`` to create a build package using the following command line: ``kcl build desc.xml target-folder``.

Once created, there are two ways to install. The command line can be used as following:

.. code:: bash

    $ kcl query-repository features path/to/repository
    c353d049-710e-4027-a707-18e11bbcab22

    $ kcl install path/to/repository path/to/target c353d049-710e-4027-a707-18e11bbcab22

Note that the feature id will vary, this is just an example. The alternative is to build the user interface and create an ``info.json`` file next to the binary with the following contents:

.. code:: json

    {
        "applicationName": "Name to show in the UI",
        "repository": "path/to/repository/relative/to/UI"
    }