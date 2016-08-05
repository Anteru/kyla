Repository types
================

kyla supports several *repository types* with different capabilities. The built-in types are:

* ``Loose``: A loose repository consists of the content objects only. It supports repair, add/remove, and validation. All data is stored in a subfolder called ``.ky``. The content objects are available in ``.ky/objects``.
* ``Deployed``: A deployed repository is "unpacked", that is, the content objects are stored with their actual file name, and some content objects may be duplicated. A deployed repository supports repair, add/remove, and validation.
* ``Packed``: A packed repository consists of the database and one or more package files. Content objects are spread over package files. A packed repository supports only validation.

A ``Packed`` repository can be also be used for web installation. Putting all files onto a server which supports `HTTP range requests <https://tools.ietf.org/html/rfc7233>`_ makes the packed repository readable over the web.
