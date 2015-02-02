#!/usr/bin/env python3
import sys
import os

sys.path.append ('../../scripts')
from kyla import InstallationBuilder

builder = InstallationBuilder ('GFLW', '3.1')
glfwFeature = builder.AddFeature ('GFLW')
glfwFeature.AddFilesFromDirectory (os.path.join (os.getcwd (), 'data'))
builder.SetEmbeddedSourcePackages (True)

doc = builder.Finalize ()
print (doc)
