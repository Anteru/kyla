Comparison with other tools
===========================

NSIS
----

The `Nullsoft Scriptable Install System <http://nsis.sourceforge.net/Main_Page>`_ is a popular and fully fledged installation system. It works at a much higher level than kyla and supports the creation of shortcuts, has a scripting language, and creates self-extracting installations.

The main differences are:

* kyla has native support for updating and updates only the required files - NSIS does not have built-in support for this.
* NSIS is not designed for integration - it takes care of dialogs, for example - while kyla is
* NSIS does not support repairing or changing installations
* NSIS supports only Windows (the generated installers only support Windows) [#nsis_win]_

InnoSetup
---------

`InnoSetup <http://www.jrsoftware.org/isinfo.php>`_ is another popular Windows installation system. Similar to NSIS, it provides a high-level installation framework, and comes with it's own scripting engine and even an IDE.

The main differences are:

* kyla has native support for updating and updates only the required files - InnoSetup can update, but it is not aware which files have changed and which haven't.
* InnoSetup is not designed for integration - it takes care of dialogs, for example - while kyla is
* InnoSetup does not support repairing or changing installations
* InnoSetup supports only Windows

Windows Installer
-----------------

`Windows Installer <msdn.microsoft.com/en-us/library/cc185688%28VS.85%29.aspx>`_ is also a fully fledged runtime for installations which is bundled with Windows. It is comparable with kyla as it also uses a database, but there are again many differences:

* kyla can install directly from web, while the Windows Installer will download a package first before it starts installing [#wi_web]_
* Windows Installer supports only Windows
* Windows Installer does not support modern compression algorithms. Only cabinet files are supported.
* Windows Installer cannot split a file when creating an installation media - very large files will result in very large packages. [#wix_media]_

.. rubric:: Footnotes

.. [#nsis_win] From `<http://nsis.sourceforge.net/Features>`_: Generated installer will still run on Windows only
.. [#wi_web] From `<https://msdn.microsoft.com/en-us/library/windows/desktop/aa368328(v=vs.85).aspx>`_: If the installation database is at a URL, the installer downloads the database to a cache location before starting the installation.
.. [#wix_media] From `<http://www.joyofsetup.com/2011/06/21/wix-and-cabinetry/>`_: On the flip side, any individual files that are larger than the maximum size go into a single-file cabinet, so it’s possible that such cabinets will be larger than the maximum size.
