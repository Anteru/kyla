import argparse
import subprocess
import os
import hashlib
import json
import tempfile
from collections import OrderedDict

class KylaRunner:
    def __init__(self, kclBinaryPath):
        self._kcl = kclBinaryPath

    def BuildRepository(self, desc, targetDirectory, sourceDirectory=None):
        args = [self._kcl, 'build']

        if sourceDirectory:
            args.append ('--source-directory')
            args.append (sourceDirectory)

        args.append (desc)
        args.append (targetDirectory)

        return subprocess.check_call (args) == 0

    def Install(self, source, target, filesets=[]):
        return self._ExecuteAction ('install', source, target, filesets) == 0

    def _ExecuteAction(self, action, source, target, filesets):
        args = [self._kcl, action, source, target, ' '.join (filesets)]

        #TODO handle validate
        return subprocess.check_call (args)

class TestEnvironment:
    def __init__(self, kyla : KylaRunner, testDirectory):
        self.testDirectory = testDirectory
        self.kyla = kyla
        self.workingDirectory = os.path.abspath ('.')

class SetupGenerateRepository:
    def Execute(self, env : TestEnvironment, args):
        target = os.path.join (env.testDirectory, args ['target'])
        source = os.path.join (env.workingDirectory, 'tests', args ['source'])
        sourceDirectory = args.get ('source-directory', None)

        if sourceDirectory:
            sourceDirectory = os.path.join (env.workingDirectory, 'tests', sourceDirectory)

        return env.kyla.BuildRepository (source,
            target, sourceDirectory = sourceDirectory)

class ExecuteInstall:
    def Execute(self, env : TestEnvironment, args):
        source = os.path.join (env.testDirectory, args ['source'])
        target = os.path.join (env.testDirectory, args ['target'])
        filesets = args ['filesets']

        return env.kyla.Install (source, target, filesets)

class CheckHash:
    def Execute (self, env : TestEnvironment, args):
        for k,v in args.items ():
            if hashlib.sha256 (open (os.path.join (env.testDirectory, k), 'rb').read()).digest().hex() != v:
                return False
        return True

hooks = {
    "generate-repository" : SetupGenerateRepository,
    "install" : ExecuteInstall,
    "check-hash" : CheckHash
}

class TestRunner:
    def __init__(self, kyla, testDescription):
        self.__kyla = kyla
        self.__test = json.load (open(testDescription, 'r'),
            object_pairs_hook=OrderedDict)

    def Execute(self):
        print ('Executing test: ' + self.__test ['info']['description'])
        with tempfile.TemporaryDirectory () as tempDir:
            print ('Working directory: "{}"'.format (tempDir))
            env = TestEnvironment (self.__kyla, tempDir)

            for phase in ['setup', 'execute', 'test']:
                print ('Executing phase: ' + phase)
                for step in self.__test [phase]:
                    k = list (step.keys ()) [0]
                    v = step [k]
                    hook = hooks [k] ()
                    r = hook.Execute (env, v)
                    print ('Step: {}, pass: {}'.format (k, r))

if __name__ == '__main__':
    kyla = r"F:\Build\kyla\bin\Debug\kcl.exe"

    tr = TestRunner (KylaRunner (kyla), 'tests/basic_install.json')
    tr.Execute ()
