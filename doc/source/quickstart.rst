Quick start
===========

The fastest -- and also the recommended way -- to get started with kyla is to use the Python bindings to create a build definition. A simple installer could look like this:

.. code:: Python

    import kyla

    rb = kyla.RepositoryBuilder ()

    mainFeature = rb.AddFeature ('Main app', 'The main application')

    binaryFiles = rb.AddFileGroup ()
    binaryFiles.AddDirectory ('path/to/bin', outputDirectory = 'bin')

    # We want to store the binary files in their own package
    binPackage = rb.AddFilePackage ('bin')

    # Link the binary package to the binary files
    binPackage.AddReference (binaryFiles)

    # Our main feature consists of only the binary files -- link them together
    mainFeature.AddReference (binaryFiles)

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