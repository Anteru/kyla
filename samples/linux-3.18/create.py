#!/usr/bin/env python3
import sys
import os

sys.path.append ('../../scripts')
from kyla import InstallationBuilder

builder = InstallationBuilder ('Linux', '3.18')

srcData = {
	'Linux_3_18' : 'linux-3.18',
	'Linux_3_18_1' : 'linux-3.18.1',
	'Linux_3_18_2' : 'linux-3.18.2',
	'Linux_3_18_3' : 'linux-3.18.3'
}

for k,v in srcData.items ():
	feature = builder.AddFeature (k)
	feature.SetSourcePackage ('SourcePackage_{}'.format (k))
	feature.AddFilesFromDirectory (os.path.join (os.getcwd (), 'data', v))
	builder.AddSourcePackage ('SourcePackage_{}'.format (k))

doc = builder.Finalize ()
print (doc)
