Changelog
=========

kyla 3.0
----------

.. warning::

    This release contains a breaking change of the file format. Kyla 3.0 will
    fail to open a package created with earlier versions.

* The feature tree is now part of the features themselves, and no longer external. This means features can be nested now, and a deployed repository will now properly retain the feature relationships.
* Run-time variables in the build are now provided through *variables* instead of special write-enabled properties.
* The log callback function signature has changed.

kyla 2.0.3
----------

* Progress update has been rewritten and is now much more accurate. Deleting files reports progress now so removing features shows progress, and retrieving files reports progress based on data written, including file duplication.
* Fix memory leak if a repository could not be opened.
* UI has been simplified and uses the "fusion" style for cross-platform portability.
* Updated SQLite from 3.19.2 to 3.21.0.

kyla 2.0.2
----------

* Installation preparation time has been drastically cut down if many (>10000) files are to be installed. Previously, a lot of time was spent identifying unique paths to create all directories, which has been reduced by more than an order of magnitude.
* Progress reporting is now solely based on fetch progress. This optimizes for the default case where progress is dominated by fetching changed contents. In that case, the progress will be 100% accurate and reflect exactly what is written on disk. In cases where an installation mostly consists of duplication, the progress will not be updated while files are duplicated. Renames are treated as new content fetching.

kyla 2.0.1
----------

* The packed repository deployment backend is now multithreaded. It will overlap reading data, decompressing, and writing data. For network sources, this means that downloads will happen concurrently with everything else, and installations should finish as soon as the data download has finished.
* On Windows, the sample UI now shows the progress in the task bar.
* Updated SQLite from 3.16.1 to 3.19.2.

kyla 2.0
--------

.. warning::

    This release contains a breaking change of the file format. Kyla 1.0 will
    fail to open a package created with Kyla 2.0 and vice versa.

* Changed database format for improved forward compatibility.
* kyla uses now a feature-based definition instead of the previous file-set centric approach. This changes both the database structure, as well as the repository definition structure.
* The Python generator has been rewritten.
* The ``kui`` sample UI has been rewritten, and kyla learned how to provide UI related information.
* Zstd compression is available as an alternative to the default Brotli compression.
* The ``KYLA_MAKE_API_VERSION`` macro has been fixed.
* The repository builder can print out various statistics now, for instance, the final compression ratio.
* kyla supports file content encryption.

  .. note:: The encryption only encrypts the file contents, so file names, sizes, and even the hashes of the content will remain visible. In particular, the database itself is not encrypted.
