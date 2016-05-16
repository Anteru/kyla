import argparse
import subprocess
import os
import hashlib
import json
import tempfile
from collections import OrderedDict
import glob

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

    def Configure(self, source, target, filesets=[]):
        return self._ExecuteAction ('configure', source, target, filesets) == 0

    def _ExecuteAction(self, action, source, target, filesets):
        args = [self._kcl, action, source, target] + filesets

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

class ExecuteConfigure:
    def Execute(self, env : TestEnvironment, args):
        source = os.path.join (env.testDirectory, args ['source'])
        target = os.path.join (env.testDirectory, args ['target'])
        filesets = args ['filesets']

        return env.kyla.Configure (source, target, filesets)

class CheckHash:
    def Execute (self, env : TestEnvironment, args):
        for k,v in args.items ():
            try:
                if hashlib.sha256 (open (os.path.join (env.testDirectory, k), 'rb').read()).digest().hex() != v:
                    return False
            except:
                return False
        return True

class CheckNotExistant:
    def Execute (self, env : TestEnvironment, args):
        for arg in args:
            if os.path.exists (os.path.join (env.testDirectory, arg)):
                return False
        return True

hooks = {
    "generate-repository" : SetupGenerateRepository,
    "install" : ExecuteInstall,
    "configure" : ExecuteConfigure,
    "check-hash" : CheckHash,
    "check-not-existant" : CheckNotExistant
}

class TestRunner:
    def __init__(self, kyla, testDescription):
        self.__kyla = kyla
        self.__test = json.load (open(testDescription, 'r'),
            object_pairs_hook=OrderedDict)

    def Execute(self):
        with tempfile.TemporaryDirectory () as tempDir:
            env = TestEnvironment (self.__kyla, tempDir)

            for phase in ['setup', 'execute', 'test']:
                for step in self.__test [phase]:
                    k = list (step.keys ()) [0]
                    v = step [k]
                    hook = hooks [k] ()
                    r = hook.Execute (env, v)

                    if r == False:
                        return False
        return True

if __name__ == '__main__':
    kyla = r"F:\Build\kyla\bin\Debug\kcl.exe"

    for test in glob.glob ('tests/*.json'):
        testName = os.path.splitext (os.path.basename (test))[0]
        tr = TestRunner (KylaRunner (kyla), test)
        r = tr.Execute ()
        if r:
            print (testName, 'PASS')
        else:
            print (testName, 'FAIL')
