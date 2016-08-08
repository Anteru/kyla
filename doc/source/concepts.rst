Basic concepts
==============

Kyla is built around two main concepts:

* A **repository**, which is the content source. kyla exclusively works on repositories. A file repository contains the contents of all files stored in it. Files are grouped in file sets. A repository can be either *packed* into a few package files, or be a *loose* repository with all file contents being stored separately in the file system.
* A **file set** is a set of files that are processed together. Each file in a file set must be unique, and all sets in a file repository must be disjoint. For example, each feature of an application would be represented as a file set. A file set is the basic unit used by kyla.

.. note::

    The repository only stores contents - for kyla, the link of a file content to an actual file name happens through a file set. A repository can store all file contents in a single file. In this case, the file sets are invisible to the user when inspecting the on-disk storage.

Kyla treats the source and target of every deployment as a repository. There is only one operation that can be performed, called *configure*, which configures the *target* repository such that it contains the specified file sets. An installation is a special case of a *configure* operation into an *empty* repository.

Under the hood, kyla works using file contents instead of files. For example, if a file set consists of five identical files, kyla will only request the *content* of one of those files from the source repository, and then copy it inside the target. This reduces the amount of data that must be read from the source repository, which is important for web based installations.
