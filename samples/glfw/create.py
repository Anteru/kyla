#!/usr/bin/env python3
import sys
import os

sys.path.append ('../../scripts')
from kyla import FileRepositoryBuilder, PackageType

if __name__ == '__main__':
    for version in {'glfw-3.1', 'glfw-3.1.2'}:
        builder = FileRepositoryBuilder ()

        builder.SetPackageType (PackageType.Loose)

        fileSet = builder.AddFileSet (version.upper ())
        fileSet.AddFilesFromDirectory (os.path.join (os.getcwd (), version))

        doc = builder.Finalize ()
        open ('{}.xml'.format (version), 'w').write (doc)
