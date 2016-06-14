Tutorial
========

Let's get started with building a basic file repository. We'll use the ``pykyla`` builder for this. The tutorial assumes that you have Python 3 installed and in your path.

Getting test data
-----------------

We'll start by grabbing some test data. In ``samples/glfw``, you can find a ``fetch.py`` script which will download two versions of the GLFW library.

To create the build scripts, run ``create.py`` which will result in two Xml files -- ``glfw-3.1.2.xml`` and ``glfw-3.1.2-filesets.xml``. Those build files contain everything necessary to create a repository from the extracted archives.

Building
--------

The next step is to build a repository. This is done using the ``kcl`` command line binary, which is expected to be found in the path. We'll build a repository for `glfw-3.1.2`` first, using the ``build.bat`` batch file. All it does is call the ``kcl`` binary and pass the build file generated previously. The script will actually build twice into two separate output folders, one is ``source``, and one is ``target`` - more on that in a moment.

Validating
----------

Any kyla repository can self-validate for consistency. Let's try on the repository we just built - use::

    $ .\kcl validate source

This will validate the ``source`` repository. You should see::

    OK 362 CORRUPTED/MISSING 0

Open the ``target/.ky/objects`` folder and let's damage some data. Open ``99a170b76f0c25536a080e78cb5d7ebd9e5cf6befcdb76645ba953c43d9f03ef`` (this is a text file) and do some random changes to this file. Next, delete ``45857f4db2fd924afc4859e640d9c8bd6e6128d3a03caf1843f240b036f48f06``. Let's verify again::

    $ .\kcl validate target

The output should be::

    OK 360 CORRUPTED/MISSING 2

Let's fine out what is damaged or missing, by using the verbose output::

    $ .\kcl validate -v target

You'll notice the output starts with::

    MISSING   target\.ky\objects\45857f4db2fd924afc4859e640d9c8bd6e6128d3a03caf1843f240b036f48f06
    CORRUPTED target\.ky\objects\99a170b76f0c25536a080e78cb5d7ebd9e5cf6befcdb76645ba953c43d9f03ef

Repairing
---------

Let's repair the repository. As we have a pristine copy (``source``) we can simply repair the target repository using the source repository. Notice that the repositories don't even have to be related at all - they just need to contain the same contents. Let's give it a try::

    $ .\kcl repair source target

If we validate now, we'll get::

    OK 362 CORRUPTED/MISSING 0

That was easy.

Installing
----------

Let's try to actually install - or deploy - the repository. Any deploy requires to specify which file sets are about to be deployed. We can find out which file sets the repository contains by calling::

    $ .\kcl query-filesets source

This will give us::

    bd4f8902-087f-401b-819c-f978c6e14d6b 377 4688725

.. note:: The uuid will be different if you run this!

The output consists of the file set Uuid, followed by the number of files in the file set and the total size in bytes. The file set id is needed so we can tell kyla what to install. On the command line, we can start an installation by using the ``install`` command, followed by the source and target repository, and the ids of the file sets we want to deploy::

    $ .\kcl install source deploy bd4f8902-087f-401b-819c-f978c6e14d6b

An installation is also a file repository, so we can use the usual actions on it. Let's start by validating the installation::

    $ .\kcl validate deploy

This yields::

    OK 377 CORRUPTED/MISSING 0

Let's try the previous example of damaging the installation. Delete the ``CMakeLists.txt`` file and change a byte in ``COPYING``, and check again::

    OK 375 CORRUPTED/MISSING 2

We can repair the repository using our source repository as before::

    $ .\kcl repair source deploy

In fact, we could have also repaired the source repository using the deploy repository, as all repository types are equivalent. Wait a moment, does this mean we can install from the just installed repository? Yes, this is indeed possible::

    $ .\kcl install deploy deploy2 bd4f8902-087f-401b-819c-f978c6e14d6b

This will install from our freshly deployed repository into a new deployed repository.

Configuring
-----------

Configuring a repository means adding or removing file sets from it. We'll create three filesets for GLFW, a general one, one for the ``docs/`` folder, and one for the ``examples/`` folder. For this sample, you need to build the ``glfw-3.1.2-filesets.xml`` repository. Let's query it::

    $ .\kcl query-filesets -n source-fs

This yields::

    82511c20-841a-49c5-9388-41ca8a068f93 docs 268 2594580
    aa1bc840-5432-45cc-8880-ab4f8fc3ce87 core 101 1982061
    b5badd20-d6cf-4420-aadc-0f6b62fa9e02 examples 8 112084

We can now install only one feature::

    $ .\kcl install source-fs deploy-fs 82511c20-841a-49c5-9388-41ca8a068f93

Let's add the examples now, and remove the docs::

    $ .\kcl configure source-fs deploy-fs b5badd20-d6cf-4420-aadc-0f6b62fa9e02

Updating
--------

For updating, we'll update from ``GLFW-3.1`` to ``GLFW-3.1.2``. Let's install the old one as usual, by querying the filesets in ``source-old`` and issuing a deploy into ``deploy``::

    $ .\kcl query-filesets source-old
    0d773cdb-998a-4323-a083-6dd68d950dbd 382 4669894
    $ .\kcl install source-old deploy 0d773cdb-998a-4323-a083-6dd68d950dbd

Now we update - simply by using configure into the new, desired state::

    $ .\kcl query-filesets source
    bd4f8902-087f-401b-819c-f978c6e14d6b 377 4688725
    $ .\kcl configure source deploy bd4f8902-087f-401b-819c-f978c6e14d6b

We can validate that everything is in order - open the ``CMakeLists.txt`` and you'll see it's set for GLFW 3.1.2.
