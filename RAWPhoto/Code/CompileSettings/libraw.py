import os
import os.path
import SCons.Warnings

class LibRawNotFound(SCons.Warnings.Warning):
    pass
SCons.Warnings.enableWarningClass(LibRawNotFound)

def generate(env):
    path = env['OPTICKSDEPENDENCIESINCLUDE']
    if not path:
       SCons.Warnings.warn(PythonFound,"Could not detect LibRaw")
    else:
       if env["OS"] == "windows":
          lib=['raw']
       else:
          lib=['raw','gomp']
       env.AppendUnique(CXXFLAGS= ["-I%s/libraw" % (path,)],
                        LIBS=lib)

def exists(env):
    return env.Detect('libraw')
