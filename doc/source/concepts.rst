Basic concepts
==============

Kyla is built around two main concepts:

* A **repository**, which stores the file contents kyla is managing. kyla exclusively works on repositories. Inside a repository, the individual files are grouped into file sets. There are different repository types: For instance, the file contants can be packed together, or the repository can be the installed application itself.
* A **file set** is a set of files that are processed together. Each file in a file set must be unique, and all sets in a file repository must be disjoint. For example, each feature of an application would be represented as a file set.

.. note::

    The repository only stores contents - for kyla, the link of a file content to an actual file name happens through a file set. A repository can store all file contents in a single file. In this case, the file sets are invisible to the user when inspecting the on-disk storage.

Kyla treats the source and target of every deployment as a repository. There are only three operations that can be performed on a repository: *configure*, *validate* and *repair*. *Configure* configures the *target* repository such that it contains the specified file sets. An installation is a special case of a *configure* operation into an *empty* repository. *Validate* validates the contents of the repository; a *repair* is a validate followed by a repair step which fetches the missing contents.

.. note::

    Some repository types don't support all operations. See :doc:`repository-types` for details.

Under the hood, kyla works using file contents instead of files. For example, if a file set consists of five identical files, kyla will only request the *content* of one of those files from the source repository, and then copy it inside the target and duplicate there. This reduces the amount of data that must be read from the source repository, which is important for web based installations.
