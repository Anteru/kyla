#!/usr/bin/env python3
import sys
import os

sys.path.append ('../../scripts')
from kyla import FileRepositoryBuilder, PackageType

if __name__ == '__main__':
    for version in {'glfw-3.1', 'glfw-3.1.2'}:
        builder = FileRepositoryBuilder ()

        fileSet = builder.AddFileSet (version.upper ())
        fileSet.AddFilesFromDirectory (os.path.join (os.getcwd (), version))

        doc = builder.Finalize ()
        open ('{}.xml'.format (version), 'w').write (doc)

    # for 3.1.2, we also create a package with 3 filesets
    builder = FileRepositoryBuilder ()
    docs = builder.AddFileSet ('docs')
    docs.AddFilesFromDirectory (os.path.join (os.getcwd (), 'glfw-3.1.2', 'docs'), prefix='docs')

    examples = builder.AddFileSet ('examples')
    examples.AddFilesFromDirectory (os.path.join (os.getcwd (), 'glfw-3.1.2', 'examples'), prefix='examples')

    core = builder.AddFileSet ('core')
    for directory in {'CMake', 'deps', 'include', 'src', 'tests'}:
        core.AddFilesFromDirectory (os.path.join (os.getcwd (), 'glfw-3.1.2', directory), prefix=directory)
    core.AddFile ('cmake_uninstall.cmake.in')
    core.AddFile ('CMakeLists.txt')
    core.AddFile ('COPYING.txt')
    core.AddFile ('README.md')

    doc = builder.Finalize ()
    open ('glfw-3.1.2-filesets.xml', 'w').write (doc)
