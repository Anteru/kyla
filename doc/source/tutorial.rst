Tutorial
========

In this tutorial, we'll go through the life cycle of a basic application - installation, updating, and configuration. To follow along, open the ``samples/glfw`` directory which contains the ``GLFW`` library -- and everything you need to get started with the first installer.

Throughout the tutorial, we'll be using the ``kcl`` command line binary which provides access to all of kyla's API, and provides the entry point to build repositories, too. Make sure you add it to your ``PATH`` variable so it can be found by calling ``.\kcl``.

.. note::

    Throughout this tutorial, various Uuids are used (like ``fc996a11-e205-4701-be7a-57694707edd1``). Those are dependent on the particular installation you get. Every time you see an Uuid here, make sure to adjust it for your particular use case.

Building
--------

The first step for any installation is to build a *repository*. A repository contains all file contents and describes how they should be deployed. Building a repository requires a repository description, which is an Xml file describing what should go into the repository.

In this tutorial, three repository descriptions are provided which we'll use throughout the sample. These are the three ``.xml`` files - one for each repository we're about to build. For building, just run the ``build.bat`` script. Afterwards, you should see three new folders - ``source-3.1`` and ``source-3.1.2`` which contain all of ``GFLW`` in a single repository, and ``source-3.1.2-filesets`` which contains ``GLFW`` 3.1.2 but using file sets. We'll come back to this in the *Configuring* part.

For more information about the repository descriptions, check the :ref:`repository-description`.

Installation
------------

You're ready to go - let's install. You can just call ``install.bat`` which performs the install, but let's take a look at the file contents to understand how this actually works! Let's take a peek at the file::

    kcl install source-3.1 deploy-3.1 b7705480-903e-455a-9512-483c50c4af36

This calls ``kcl``, requests an ``install`` and then it provides the source and target folders. But what's that Uuid at the end? At its core, kyla works with file sets and repositories. A single repository must contain at least one file set, and that is the basic unit used during installation. To find out which file sets a repository has, use::

    kcl query-filesets source-3.1

This will yield some output like::

    b7705480-903e-455a-9512-483c50c4af36 382 4669894

The first entry is the file set id, the second one is the number of files, and the last one is the total size of this file set in bytes. In this repository, there's only one file set, so by invoking install and passing on this single file set id, we've installed everything!

Updating
--------

For updating, we'll update from ``GLFW-3.1`` to ``GLFW-3.1.2``. This is covered by ``update.bat`` - in the meantime, let's take a look at what it does. First, it installs from ``source-3.1`` into ``deploy-3.x-update``. The next command is where the magic happens: It invokes ``kcl``, but it requests a ``configure`` to happen, and provides the file set id from the *new* source repository but passes in the path where the *old* repository was deployed to.

What happens here is that kyla *configures* the target (which is stored in ``deploy-3.x-update``) into the desired state. The desired state is that the file sets provided on the command line should be present after the configuration step finished. We're only asking for a file set from the *new* source repository, so this means kyla will remove all existing file sets first, and then deploy the new file sets. The final state is the same as a direct installation from ``source-3.1.2`` into the target directory. The main difference is that kyla only touches *changed* files, so the update requires much less I/O traffic than an uninstall followed by an installation.

Configuring
-----------

Now that we've seen how configure can change a repository, it's easy to understand how a repository with multiple file sets works. Each file set is treated independently, and by specifying which file sets should be present, we can add or remove features. For this, we need a repository with multiple file sets, and that is exactly what we'll find in ``source-3.1.2-filesets``. If you want to give it a quick try, run ``configure.bat`` which will install a file set first, and then change the installation to another file set.

That source repository contains three file sets, one for the binaries, one for the docs, and one for the examples. The first installation deployed the docs, the second one requests that the target only contains the examples, so the docs get removed and the examples get installed instead. If you change the second command to include the Uuid from the initial installation, the docs will be preserved and the examples will be added.

.. note::

    An update is just a configuration. kyla always identifies the minimal set of changes required to transform a repository, no matter what changes have been requested. This means that you can cross-install (i.e. change from one product to a completely unrelated one), downgrade, upgrade, add/remove features, all from the configuration command.

Uninstall
---------

kyla stores all its state in a database inside the target directory. A full uninstall is thus a simple directory removal.
