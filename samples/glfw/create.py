#!/usr/bin/env python3
import sys
import os

import urllib.request
import zipfile

def DownloadFile (url, filename):
    with urllib.request.urlopen (url) as dl, open (filename, 'wb') as output:
        cl = dl.getheader ('Content-Length')

        if cl:
            cl = int (cl)
            bytesRead = 0
            while bytesRead < cl:
                chunk = dl.read (65536)
                bytesRead += len (chunk)
                output.write (chunk)
                print ('Received: {}/{}'.format (bytesRead, cl))
            print ('File downloaded')
        else:
            output.write (dl.read ())

def DownloadAndExtract (url, filename, baseDirectory):
    filename = os.path.join (baseDirectory, filename)
    DownloadFile (url, filename)
    with zipfile.ZipFile (filename, 'r') as z:
        z.extractall (baseDirectory)
    os.unlink (filename)

def GenerateGLFWSamples (baseDirectory = os.getcwd ()):
    from kyla import FileRepositoryBuilder, PackageType
    DownloadAndExtract (
        'https://github.com/glfw/glfw/releases/download/3.1.2/glfw-3.1.2.zip',
        'glfw-3.1.2.zip', baseDirectory)
    DownloadAndExtract (
        'https://github.com/glfw/glfw/releases/download/3.1/glfw-3.1.zip',
        'glfw-3.1.zip', baseDirectory)

    rootFileSets = {}
    for version in {'glfw-3.1', 'glfw-3.1.2'}:
        builder = FileRepositoryBuilder (packageType=PackageType.Loose)

        fileSet = builder.AddFileSet (version.upper ())
        fileSet.AddFilesFromDirectory (os.path.join (baseDirectory, version))

        rootFileSets [version] = fileSet.GetId ()

        doc = builder.Finalize ()
        open (os.path.join (baseDirectory, '{}.xml'.format (version)), 'w').write (doc)
        print ('Generated {}.xml'.format (version))

    with open (os.path.join (baseDirectory, 'build.bat'), 'w') as outputFile:
        outputFile.write ('kcl build --source-directory=glfw-3.1 glfw-3.1.xml source-3.1\n')
        outputFile.write ('kcl build --source-directory=glfw-3.1.2 glfw-3.1.2.xml source-3.1.2\n')
        outputFile.write ('kcl build --source-directory=glfw-3.1.2 glfw-3.1.2-filesets.xml source-3.1.2-filesets\n')

    with open (os.path.join (baseDirectory, 'install.bat'), 'w') as outputFile:
        outputFile.write ('kcl install source-3.1 deploy-3.1 {}\n'.format (rootFileSets ['glfw-3.1']))

    with open (os.path.join (baseDirectory, 'update.bat'), 'w') as outputFile:
        outputFile.write ('kcl install source-3.1 deploy-3.x-update {}\n'.format (rootFileSets ['glfw-3.1']))
        outputFile.write ('kcl configure source-3.1.2 deploy-3.x-update {}\n'.format (rootFileSets ['glfw-3.1.2']))

    # for 3.1.2, we also create a package with 3 filesets
    builder = FileRepositoryBuilder (packageType=PackageType.Loose)
    docs = builder.AddFileSet ('docs')
    docs.AddFilesFromDirectory (
        os.path.join (baseDirectory, 'glfw-3.1.2', 'docs'), prefix='docs')

    examples = builder.AddFileSet ('examples')
    examples.AddFilesFromDirectory (
        os.path.join (baseDirectory, 'glfw-3.1.2', 'examples'), prefix='examples')

    core = builder.AddFileSet ('core')
    for directory in {'CMake', 'deps', 'include', 'src', 'tests'}:
        core.AddFilesFromDirectory (
            os.path.join (baseDirectory, 'glfw-3.1.2', directory), prefix=directory)
    core.AddFile ('cmake_uninstall.cmake.in')
    core.AddFile ('CMakeLists.txt')
    core.AddFile ('COPYING.txt')
    core.AddFile ('README.md')

    doc = builder.Finalize ()
    open (os.path.join (baseDirectory, 'glfw-3.1.2-filesets.xml'), 'w').write (doc)
    print ('Generated glfw-3.1.2-filesets.xml')

    # The sample batch file here installs docs first, then removes them
    # and installs the examples
    with open (os.path.join (baseDirectory, 'configure.bat'), 'w') as outputFile:
        outputFile.write ('kcl install source-3.1.2-filesets deploy-3.1.2-filesets {}\n'.format (docs.GetId ()))
        outputFile.write ('kcl configure source-3.1.2-filesets deploy-3.1.2-filesets {}\n'.format (examples.GetId ()))

if __name__=='__main__':
    sys.path.append (os.path.join (os.path.dirname (os.path.abspath (__file__)), '../../scripts'))

    GenerateGLFWSamples ()
