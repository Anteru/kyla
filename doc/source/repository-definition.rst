.. _repository-description:

Repository build description
============================

Installer repositories are compiled by using ``kcl build``. This requires an Xml file that describes how to build a file repository. Here's an example repository file:

.. code-block:: xml

    <?xml version="1.0" ?>
    <Repository>
      <Features>
        <Feature Id="7f9e0d24-1c82-49d3-86f9-47c0a7c984d5">
          <Reference Id="c9a41343-bc90-4871-84bd-27f04aac4794"/>
        </Feature>
      </Features> 
      <Files>
        <Group Id="c9a41343-bc90-4871-84bd-27f04aac4794">
          <File Source="A:\full\path\file.exe" Target="bin/file.exe"/>
        </Group>
        <Packages>
          <Package Name="bin">
            <Reference Id="c9a41343-bc90-4871-84bd-27f04aac4794"/>
          </Package>
        </Packages>
      </Files>
      <UI>
        <FeatureTree>
          <Node Description="Binary files." Name="Binaries">
            <Reference Id="7f9e0d24-1c82-49d3-86f9-47c0a7c984d5"/>
          </Node>
        </FeatureTree>
      </UI>
    </Repository>

Reference
---------

The repository stores various objects which can be referenced. In general, anything which has an ``Id`` attribute can be referenced, and all references are done by adding a ``Reference`` node. Objects can be grouped by using a ``Group`` node, this makes it possible to reference many objects using a single ``Reference``.

* ``Repository`` is the top level node and must be always present.
* ``Features`` contains the feature list. A feature must reference another object in the repository, and must not reference another feature.
* ``Files`` describes all file objects and the storage layout.
  
  If present, ``Packages`` is used to group files into packages. A ``Package`` must have a name and it must reference an object from the ``Files`` tree. Files which are not explicitly packaged are automatically placed into a ``main`` package.

  Files can be grouped together for easy referencing using a ``Group`` node.

  A ``File`` node can reference the full source path or a relative path. If a relative path is used, the source directory must be specified during the compilation. Relative paths are automatically used for the ``Target`` path as well if there's no ``Target`` specified.

* ``UI`` contains UI related data. This data is only present in source packages and never deployed, but it's used to create the installation UI.

  The ``FeatureTree`` defines the feature grouping. At least one ``Node`` must be present, and every node must reference at least one ``Feature``. Nodes can be arbitrarily nested.