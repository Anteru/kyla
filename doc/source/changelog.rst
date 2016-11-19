Changelog
=========

kyla 1.1
--------

.. warning::

    This release contains a breaking change of the file format. Kyla 1.0 will
    fail to open a package created with Kyla 1.1 and vice versa.

* Change database format for improved forward compatibility.

  * Features like compression and encryption are not stored in the ``storage_mapping`` table directly any more, but separately.
  * A ``features`` table has been added which allows kyla to identify the required feature support for a given package.

* Zstd compression is available as an alternative to the default Brotli compression.
* The ``KYLA_MAKE_API_VERSION`` macro has been fixed.
* The repository builder can print out various statistics now, for instance, the final compression ratio.
