import sys
import os
import shutil
sys.path.append ('../scripts')

from kyla import FileRepositoryBuilder

from glfw.create import GenerateGLFWSamples

if __name__ == '__main__':
    if False:
        shutil.rmtree ('../package', ignore_errors=True)
        os.makedirs('../package/samples/glfw')
        os.makedirs('../package/bin')
        os.makedirs('../package/include')

        shutil.copytree ('../doc/build/html', '../package/docs')
        shutil.copyfile ('../src/kyla/inc/Kyla.h', '../package/include/Kyla.h')

        GenerateGLFWSamples ('../package/samples/glfw')

    sdk = FileRepositoryBuilder ()

    docs = sdk.AddFileSet ('docs')
    docs.AddFilesFromDirectory ('../package/docs', prefix='docs')
    binaries = sdk.AddFileSet ('binaries')
    binaries.AddFilesFromDirectory ('../package/bin', prefix='bin')
    include_files = sdk.AddFileSet ('include_files')
    include_files.AddFilesFromDirectory ('../package/include', prefix='include')
    samples = sdk.AddFileSet ('samples')
    samples.AddFilesFromDirectory ('../package/samples', prefix='samples')
    doc = sdk.Finalize ()
    open ('sdk.xml', 'w').write (doc)
