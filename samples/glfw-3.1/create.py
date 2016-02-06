#!/usr/bin/env python3
import sys
import os

sys.path.append ('../../scripts')
from kyla import FileRepositoryBuilder

builder = FileRepositoryBuilder ()
fileSet = builder.AddFileSet ('GFLW')
fileSet.AddFilesFromDirectory (os.path.join (os.getcwd (), 'data'))

doc = builder.Finalize ()
print (doc)
