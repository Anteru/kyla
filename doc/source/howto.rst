How do I ...?
=============

How to update an installation
-----------------------------

Updating is performed through a *configure*. Let's assume you have two version of your product, v1 and v2. The customer has installed v1 of the product. An upgrade can be performed by a configure using the installer of v2 directly into the current installation directory. The only difficulty arises if the installation has multiple features, in which case you need to map the currently installed features to the features in v2. As there's no general solution for this (one feature could be split, for instance), kyla does not handle this directly. Instead, you need to query the currently installed features in the target directory, and map them manually to the new features in the v2 repository.

Even though for kyla, v1 and v2 are unrelated repositories, kyla will only transfer new file contents from v2. There is no need for a special patch installer.

How to add/remove features
--------------------------

Adding/removing features is done through *configure*. Note that the current target directory cannot be used as both a source and destination of a configure operation, even though removing features does not strictly require the source to be present.

How to do a web installation
----------------------------

Create a packed installation repository, and host it on an HTTP server with range requests enabled. Use the full URL including ``http://`` or ``https://`` when opening the installation source repository, and kyla will automatically download the repository description.