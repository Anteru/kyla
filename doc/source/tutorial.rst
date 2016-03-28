Tutorial
========

Let's get started with building a basic file repository. We'll use the ``pykyla`` builder for this. The tutorial assumes that you have Python 3 installed and in your path.

Getting test data
-----------------

We'll start by grabbing some test data. In ``samples/glfw``, you can find a ``fetch.py`` script which will download two versions of the GLFW library.

To create the build scripts, run ``create.py`` which will result in two Xml files -- ``glfw-3.1.xml`` and ``glfw-3.1.2.xml``. Those build files contain everything necessary to create a repository from the extracted archives.

Building
--------

The next step is to build a repository. This is done using the ``kcl`` command line binary, which is expected to be found in the path. We'll build a repository for ``glfw-3.1.2`` first, using the ``build.bat`` batch file. All it does is call the ``kcl`` binary and pass the build file generated previously. The script will actually build twice into two separate output folders, one is ``source``, and one is ``target`` - more on that in a moment.

Validating
----------

Any kyla repository can self-validate for consistency. Let's try on the repository we just built - use::

    kcl validate source

This will validate the ``source`` repository. You should see::

    OK 362 CORRUPTED/MISSING 0

Open the ``target/.ky/objects`` folder and let's damage some data. Open ``99a170b76f0c25536a080e78cb5d7ebd9e5cf6befcdb76645ba953c43d9f03ef`` (this is a text file) and do some random changes to this file. Next, delete ``45857f4db2fd924afc4859e640d9c8bd6e6128d3a03caf1843f240b036f48f06``. Let's verify again::

    kcl validate target

The output should be::

    OK 360 CORRUPTED/MISSING 2

Let's fine out what is damaged or missing, by using the verbose output::

    kcl validate -v target

You'll notice the output starts with::

    MISSING   target\.ky\objects\45857f4db2fd924afc4859e640d9c8bd6e6128d3a03caf1843f240b036f48f06
    CORRUPTED target\.ky\objects\99a170b76f0c25536a080e78cb5d7ebd9e5cf6befcdb76645ba953c43d9f03ef

Repairing
---------

Let's repair the repository. As we have a pristine copy (``source``) we can simply repair the target repository using the source repository. Notice that the repositories don't even have to be related at all - they just need to contain the same contents. Let's give it a try::

    kcl repair source target

If we validate now, we'll get::

    OK 362 CORRUPTED/MISSING 0

That was easy.

Installing
----------

Let's try to actually install - or deploy - the repository. Any deploy requires to specify which file sets are about to be deployed. We can find out which file sets the repository contains by calling::

    kcl query-filesets source

This will give us::

    bd4f8902-087f-401b-819c-f978c6e14d6b 377 4688725

.. note:: The uuid will be different if you run this!

The output is file set uuid, followed by the number of files in the file set and the total size in bytes. Now we can create an installation. The ``kcl`` allows us to select the file sets on the command line::

    kcl install source deploy bd4f8902-087f-401b-819c-f978c6e14d6b

An installation is also a file repository, so we can use the usual options on it. Let's start by validating the installation::

    kcl validate deploy

This yields::

    OK 377 CORRUPTED/MISSING 0

Let's try the previous example of damaging the installation. Delete the ``CMakeLists.txt`` file and change a byte in ``COPYING``, and check again::

    OK 375 CORRUPTED/MISSING 2

We can repair the repository using our source repository as before::

    kcl repair source deploy

In fact, we could have also repaired the source repository using the deploy repository, as all repository types are equivalent. Wait a moment, does this mean we can install from the just installed repository? Yes, this is indeed possible::

    kcl install deploy deploy2 bd4f8902-087f-401b-819c-f978c6e14d6b
