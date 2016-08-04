import sys
import os
import shutil
sys.path.append (os.path.join (os.path.dirname (os.path.abspath (__file__)), '../scripts'))

from kyla import FileRepositoryBuilder

from glfw.create import GenerateGLFWSamples
import subprocess

if __name__ == '__main__':
    srcDir = sys.argv [1]
    binaryDir = sys.argv [2]

    print ('Preparing directories', flush=True)
    shutil.rmtree ('sdk-package', ignore_errors=True)
    shutil.rmtree ('sdk', ignore_errors=True)
    os.makedirs('sdk-package/samples/glfw')
    os.makedirs('sdk-package/bin')
    os.makedirs ('sdk-package/bin/platforms')
    os.makedirs('sdk-package/include')

    print ('Copying docs', flush=True)
    shutil.copytree (os.path.join (srcDir, 'doc/build/html'), 'sdk-package/docs')
    shutil.copyfile (os.path.join (srcDir, 'src/kyla/inc/Kyla.h'), 'sdk-package/include/Kyla.h')

    print ('Generating samples', flush=True)
    GenerateGLFWSamples ('sdk-package/samples/glfw')

    print ('Generating SDK installer', flush=True)
    sdk = FileRepositoryBuilder ()

    docs = sdk.AddFileSet ('docs')
    docs.AddFilesFromDirectory ('sdk-package/docs', prefix='docs')
    binaries = sdk.AddFileSet ('binaries')

    print ('Copying binaries', flush=True)
    binaryFiles = [
     'kcl.exe', 'kui.exe', 'kyla.dll',
     'Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Widgets.dll', 'platforms/qwindows.dll'
    ]

    for binary in binaryFiles:
     shutil.copyfile (os.path.join (binaryDir, binary), 'sdk-package/bin/' + binary)

    binaries.AddFilesFromDirectory ('sdk-package/bin', prefix='bin')
    include_files = sdk.AddFileSet ('include_files')
    include_files.AddFilesFromDirectory ('sdk-package/include', prefix='include')
    samples = sdk.AddFileSet ('samples')
    samples.AddFilesFromDirectory ('sdk-package/samples', prefix='samples')
    doc = sdk.Finalize ()
    open ('sdk.xml', 'w').write (doc)

    print ('Packaging installer', flush=True)
    subprocess.check_call ([os.path.join (binaryDir, 'kcl.exe'), 'build',
        '--source-directory=sdk-package', 'sdk.xml', 'sdk'])

    print ('Cleaning up', flush=True)    
    shutil.rmtree ('sdk-package', ignore_errors=True)
