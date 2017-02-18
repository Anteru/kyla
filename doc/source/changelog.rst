Changelog
=========

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

  .. note:: The encryption only encrypts the file contents, so file names, sizes, and even the hashes of the content will remain visibile. In particular, the database itself is not encrypted.
