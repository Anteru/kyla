#!/usr/bin/env python3
import sys
import os

sys.path.append ('../../scripts')
from kyla import FileRepositoryBuilder, PackageType

builder = FileRepositoryBuilder ()

builder.SetPackageType (PackageType.Loose)

fileSet = builder.AddFileSet ('GFLW')
fileSet.AddFilesFromDirectory (os.path.join (os.getcwd (), 'glfw-3.1.2'))

doc = builder.Finalize ()
print (doc)
