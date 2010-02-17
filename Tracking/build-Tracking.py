#!/usr/bin/env python
import sys
import subprocess
import os
import os.path
from os.path import join
import optparse
import traceback
import shutil
import zipfile
import re
import datetime
import string

aeb_platform_mappings = {'win32':'win32-x86-msvc8.1-release',
                         'win64':'win64-x86-msvc8.1-release',
                         'win32-debug':'win32-x86-msvc8.1-debug',
                         'win64-debug':'win64-x86-msvc8.1-debug'}

def execute_process(args, bufsize=0, executable=None, preexec_fn=None,
      close_fds=None, shell=False, cwd=None, env=None,
      universal_newlines=False, startupinfo=None, creationflags=0):
    if is_windows():
        stdin = subprocess.PIPE
        stdout = sys.stdout
        stderr = sys.stderr
    else:
        stdin = None
        stdout = None
        stderr = None

    process = subprocess.Popen(args, bufsize=bufsize, stdin=stdin,
          stdout=stdout, stderr=stderr, executable=executable,
          preexec_fn=preexec_fn, close_fds=close_fds, shell=shell,
          cwd=cwd, env=env, universal_newlines=universal_newlines,
          startupinfo=startupinfo, creationflags=creationflags)
    if is_windows():
        process.stdin.close()
    returncode = process.wait()
    return returncode

def is_windows():
    """Determine if this script is executing on the Windows operating system.
    @return: Return True if script is executed on Windows, False otherwise.
    @rtype: L{bool}

    """
    return sys.platform.startswith("win32")

class ScriptException(Exception):
    """Report error while running script"""

class Builder:
    def __init__(self, dependencies, opticks_code_dir, build_in_debug,
                 opticks_build_dir, verbosity):
        self.depend_path = dependencies
        self.opticks_code_dir = opticks_code_dir
        self.build_debug_mode = build_in_debug
        self.opticks_build_dir = opticks_build_dir
        self.verbosity = verbosity
        if self.build_debug_mode:
            self.mode = "debug"
        else:
            self.mode = "release"

    def update_version_number(self, scheme, new_version):
        if scheme is None or scheme == "none":
            return

        if self.verbosity > 1:
            print "Updating version number..."
        # Read the version directly from the file
        try:
            version_info = read_version_h()
            version_number = version_info["TRACKING_VERSION_NUMBER"]
            version_number = version_number.strip('"')
        except:
            raise ScriptException("Could not determine the "\
                "current version while attempting to update "\
                "the version")
        if self.verbosity >= 1:
            print "Original version # was", version_number

        if new_version is not None:
            version_number = new_version

        if scheme == "nightly" or scheme == "unofficial":
            #strip off any suffix from the version #
            version_parts = version_number.split(".")
            if len(version_parts) >= 2:
                #Check for Nightly.BuildRev where BuildRev is just a number
                #If so, strip off the BuildRev portion so the rest off the
                #suffix stripping will work.
                if version_parts[-2].find("Nightly") != -1:
                    version_parts = version_parts[0:-1] #Trim off the BuildRev part

            for count in range(0, len(version_parts) - 1):
                if not(version_parts[count].isdigit()):
                    raise ScriptException("The current version # "\
                        "is improperly formatted.")
            last_part = version_parts[-1]
            match_obj = re.match("^\d+(\D*)", last_part)
            if match_obj is None:
                raise ScriptException("The current version # is "\
                    "improperly formatted.")
            version_parts[-1] = last_part[:match_obj.start(1)]
            version_number = ".".join(version_parts)

            #append on the appropriate suffix to the version #
            if scheme == "unofficial":
                version_number = version_number + "Unofficial"
            elif scheme == "nightly":
                todays_date = datetime.date.today()
                today_str = todays_date.strftime("%Y%m%d")
                if len(today_str) != 8:
                    raise ScriptException("This platform does not properly "\
                        "pad month and days to 2 digits when using "\
                        "strftime.  Please update this script to address "\
                        "this problem")
                build_revision = "NoRev"
                if os.path.exists(".svn") or os.path.exists("_svn"):
                    process = subprocess.Popen(["svnversion", "-c", "-n", "."],
                        stdout=subprocess.PIPE, stdin=subprocess.PIPE)
                    stdout = process.communicate()[0]
                    if process.returncode != 0:
                        raise ScriptException("Problem running svnversion")
                    version_line_split = stdout.split(":")
                    if len(version_line_split) != 2:
                        raise ScriptException("Unexpected output from "\
                            "svnversion")
                    build_revision = version_line_split[1]
                    if build_revision.endswith("S"):
                        raise ScriptException("Switched working copy not "\
                            "currently supported")
                    if build_revision.endswith("M"):
                        build_revision = build_revision[:-1] + "*"

                if not(str(build_revision).isdigit()):
                    raise ScriptException("The Build Revision when using "\
                        "--update-version=nightly must indicate a "\
                        "subversion working copy that has not been modified.")
                version_number = version_number + \
                    "Nightly%s.%s" % (today_str, build_revision)
        elif new_version is None:
            print "You need to use --new-version to provide the version "\
                "# when using the production, rc, or milestone scheme"

        if self.verbosity >= 1:
            print "Setting version # to", version_number

        # Update TrackingVersion.h
        version_info["TRACKING_VERSION_NUMBER"] = '"' + version_number + '"'
        if scheme == "production":
            version_info["TRACKING_IS_PRODUCTION_RELEASE"] = "true"
            if self.verbosity >= 1:
                print "Making a production release"
        else:
            version_info["TRACKING_IS_PRODUCTION_RELEASE"] = "false"
            if self.verbosity >= 1:
                print "Making a not for production release"

        update_version_h(version_info)
        if self.verbosity > 1:
            print "Done updating version number"

    def build_executable(self, clean_build_first, build_opticks, concurrency):
        #No return code, throw exception or ScriptException
        if build_opticks == "none":
            return

        if self.verbosity > 1:
            print "Building Tracking plug-ins..."
        buildenv = os.environ
        buildenv["OPTICKSDEPENDENCIES"] = self.depend_path
        buildenv["OPTICKS_CODE_DIR"] = self.opticks_code_dir

        if self.verbosity >= 1:
            print_env(buildenv)

        if clean_build_first:
            if self.verbosity > 1:
                print "Cleaning compilation..."
            self.compile_code(buildenv, build_opticks, True, concurrency)
            if self.verbosity > 1:
                print "Done cleaning compilation"

        self.compile_code(buildenv, build_opticks, False, concurrency)
        if self.verbosity > 1:
            print "Done building Tracking plug-ins"

    def prep_to_run_helper(self, plugin_suffixes):
        if self.verbosity > 1:
            print "Copying Opticks plug-ins into Tracking workspace..."
        extension_plugin_path = join(self.get_binaries_dir(), "PlugIns")
        if not os.path.exists(extension_plugin_path):
            os.makedirs(extension_plugin_path)
        opticks_plugin_path = os.path.abspath(self.get_plugin_dir())
        copy_files_in_dir(opticks_plugin_path, extension_plugin_path, plugin_suffixes)
        if self.verbosity > 1:
            print "Done copying Opticks plug-ins"
            print "Copying dependency files"
        extension_bin_path = join(self.get_binaries_dir(), "Bin")
        copy_files_in_dir(join(self.depend_path, "opencv", "bin", self.platform), extension_bin_path, [".dll"])
        if self.verbosity > 1:
            print "Done copying dependency files"

        extension_bin_path = join(self.get_binaries_dir(), "Bin")
        if not os.path.exists(extension_bin_path):
            os.makedirs(extension_bin_path)
        extension_dep_file = join(extension_bin_path, "Tracking.dep")
        if not os.path.exists(extension_dep_file):
            if self.verbosity > 1:
                print "Creating Tracking.dep file..."
            extension_dep = open(extension_dep_file, "w")
            extension_dep.write("!depV1 { deployment: { "\
                "AppHomePath: $E(OPTICKS_HOME), "\
                "AdditionalDefaultPath: ../../../../Release/DefaultSettings, "\
                "UserConfigPath: ../../ApplicationUserSettings, "\
                "PlugInPath: ../PlugIns } } ")
            extension_dep.close()
            if self.verbosity > 1:
                print "Done creating Tracking.dep file"

        app_setting_dir = join("Code", "Build", "ApplicationUserSettings")
        if not os.path.exists(app_setting_dir):
            if self.verbosity > 1:
                print "Creating ApplicationUserSettings folder at %s..." % \
                    (app_setting_dir)
            os.makedirs(app_setting_dir)
            if self.verbosity > 1:
                print "Done creating ApplicationUserSettings folder"

class WindowsBuilder(Builder):
    def __init__(self, dependencies, opticks_code_dir, build_in_debug,
                 opticks_build_dir, visualstudio, verbosity):
        Builder.__init__(self, dependencies, opticks_code_dir, build_in_debug,
            opticks_build_dir, verbosity)
        self.vs_path = visualstudio

    def compile_code(self, env, target, clean, concurrency):
        solution_file = os.path.abspath("Code/Tracking.sln")
        self.build_in_visual_studio(solution_file, target,
            self.build_debug_mode, self.is_64_bit, concurrency,
            self.vs_path, env, clean)

    def get_plugin_dir(self):
        return os.path.abspath(join(self.opticks_build_dir,
            "Binaries-%s-%s" % (self.platform, self.mode), "PlugIns"))

    def get_binaries_dir(self):
        if self.platform == "Win32":
            arch = "32"
        elif self.platform == "x64":
            arch = "64"
        build_dir = os.path.join(os.path.abspath("Code"), "Build")
        return os.path.abspath(join(build_dir,
            "Binaries-%s-%s" % (self.platform, self.mode)))

    def prep_to_run(self):
        self.prep_to_run_helper([".dll", ".exe"])

    def build_in_visual_studio(self, solutionfile, debug,
                               build_64_bit, concurrency, vspath,
                               environ, clean):
        if debug:
            mode = "Debug"
        else:
            mode = "Release"
        if build_64_bit:
            mode += "|x64"
        else:
            mode += "|Win32"

        msdev_exec = join(vspath, "vc", "vcpackages", "vcbuild.exe")
        arguments = [msdev_exec, solutionfile]
        if clean:
            arguments.append("/clean")
        arguments.append("/error:[ERROR]")
        arguments.append("/warning:[WARNING]")
        arguments.append("/M%s" % (concurrency))
        arguments.append(mode)
        ret_code = execute_process(arguments,
                                 env=environ)
        if ret_code != 0:
            raise ScriptException("Visual Studio did not compile project")

class Windows32bitBuilder(WindowsBuilder):
    platform = "Win32"
    def __init__(self, dependencies, opticks_code_dir, build_in_debug,
                 opticks_build_dir, visualstudio, verbosity):
        WindowsBuilder.__init__(self, dependencies, opticks_code_dir, build_in_debug,
            opticks_build_dir, visualstudio, verbosity)
        self.is_64_bit = False

class Windows64bitBuilder(WindowsBuilder):
    platform = "x64"
    def __init__(self, dependencies, opticks_code_dir, build_in_debug,
                 opticks_build_dir, visualstudio, verbosity):
        WindowsBuilder.__init__(self, dependencies, opticks_code_dir, build_in_debug,
            opticks_build_dir, visualstudio, verbosity)
        self.is_64_bit = True

class SolarisBuilder(Builder):
    platform = "solaris-sparc"
    def __init__(self, dependencies, opticks_code_dir, build_in_debug,
                 opticks_build_dir, verbosity):
        Builder.__init__(self, dependencies, opticks_code_dir, build_in_debug,
            opticks_build_dir, verbosity)

    def compile_code(self, env, target, clean, concurrency):
        #Build extension plugins
        self.run_scons(os.path.abspath("."), self.build_debug_mode,
            concurrency, env, clean, [])

    def run_scons(self, path, debug, concurrency, environ,
                  clean, extra_args=None):
        scons_exec = "scons"
        arguments = [scons_exec, "--directory=Code", "--no-cache"]
        arguments.append("-j%s" % (concurrency))
        if clean:
            arguments.append("-c")
        if not debug:
            arguments.append("RELEASE=yes")
        if extra_args:
            arguments.extend(extra_args)
        ret_code = execute_process(arguments,
                                 env=environ,
                                 cwd=path)
        if ret_code != 0:
            raise ScriptException("Scons did not compile project")

    def get_plugin_dir(self):
        return os.path.abspath(join(self.opticks_build_dir,
            "Binaries-solaris-sparc-%s" % (self.mode), "PlugIns"))

    def get_binaries_dir(self):
        return os.path.abspath(join("Code", "Build",
            "Binaries-solaris-sparc-%s" % (self.mode)))

    def prep_to_run(self):
        self.prep_to_run_helper([".so"])

def read_version_h():
    version_path = join("Code", "Include", "TrackingVersion.h")
    version_file = open(version_path, "rt")
    version_info = version_file.readlines()
    version_file.close()
    rdata = {}
    for vline in version_info:
        fields = vline.strip().split()
        if len(fields) >=3 and fields[0] == "#define":
            rdata[fields[1]] = " ".join(fields[2:])
    return rdata

def update_version_h(fields_to_replace):
    version_path = join("Code", "Include", "TrackingVersion.h")
    version_file = open(version_path, "rt")
    version_info = version_file.readlines()
    version_file.close()
    version_file = open(version_path, "wt")
    for vline in version_info:
        fields = vline.strip().split()
        if len(fields) >= 3 and fields[0] == '#define' and \
                fields[1] in fields_to_replace:
            version_file.write('#define %s %s\n' % (fields[1],
                fields_to_replace[fields[1]]))
        else:
            version_file.write(vline)
    version_file.close()

def build_installer(aeb_platforms=[], aeb_output=None,
                    depend_path=None, sdk_version=None, verbosity=None,
                    solaris_dir=None):
    raise ScriptException("Installer not supported.")

def print_env(environ):
    print "Environment is currently set to"
    for key in environ.iterkeys():
        print key, "=", environ[key]

def copy_files_in_dir(src_dir, dst_dir, suffixes_to_match=[]):
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)
    for entry in os.listdir(src_dir):
        src_path = join(src_dir, entry)
        if os.path.isfile(src_path):
            matches = False
            if suffixes_to_match is None or len(suffixes_to_match) == 0:
                matches = True
            else:
                for suffix in suffixes_to_match:
                    if entry.endswith(suffix):
                        matches = True
            if matches:
                copy_file(src_dir, dst_dir, entry)
        elif os.path.isdir(src_path):
            dst_path = join(dst_dir, entry)
            copy_files_in_dir(src_path, dst_path, suffixes_to_match)

def copy_file(src_dir, dst_dir, filename):
    dst_file = join(dst_dir, filename)
    if os.path.exists(dst_file):
        os.remove(dst_file)
    shutil.copy2(join(src_dir, filename),
                 dst_file)

def copy_files_in_dir_to_zip(src_dir, dst_dir, zfile, suffixes_to_match=None, entries_to_skip=None):
    for entry in os.listdir(src_dir):
        if entries_to_skip is not None and entry in entries_to_skip:
            continue
        src_path = join(src_dir, entry)
        if os.path.isfile(src_path):
            matches = False
            if suffixes_to_match is None or len(suffixes_to_match) == 0:
                matches = True
            else:
                for suffix in suffixes_to_match:
                    if entry.endswith(suffix):
                        matches = True
            if matches:
                copy_file_to_zip(src_dir, dst_dir, entry, zfile)
        elif os.path.isdir(src_path):
            dst_path = join(dst_dir, entry)
            copy_files_in_dir_to_zip(src_path, dst_path, zfile, suffixes_to_match, entries_to_skip)

def copy_file_to_zip(src_dir, dst_dir, filename, zfile):
    dst_file = join(dst_dir, filename)
    zfile.writestr(dst_file, open(join(src_dir, filename), "rb").read())

def main(args):
    #chdir to the directory where the script resides
    os.chdir(os.path.abspath(os.path.dirname(sys.argv[0])))

    options = optparse.OptionParser()
    options.add_option("-d", "--dependencies",
        dest="dependencies", action="store", type="string")
    options.add_option("--opticks-code-dir",
        dest="opticks_code_dir", action="store", type="string")
    if is_windows():
        vs_path = "C:\\Program Files (x86)\\Microsoft Visual Studio 8"
        if not os.path.exists(vs_path):
            vs_path = "C:\\Program Files\\Microsoft Visual Studio 8"
        options.add_option("--visualstudio", dest="visualstudio",
            action="store", type="string")
        options.add_option("--arch", dest="arch", action="store",
            type="choice", choices=["32","64"])
        options.add_option("--solaris-dir", dest="solaris_dir",
            action="store", type="string",
            help="This option is required when using the --build-installer "\
                 "flag to build the Solaris platforms into an AEB when "\
                 "running this script on the Windows platform. This "\
                 "option should point to a directory that contains a "\
                 "checked out and compiled copy of this extension on "\
                 "Solaris.")
        options.set_defaults(visualstudio=vs_path, arch="64",
            solaris_dir=None)
    options.add_option("-m", "--mode", dest="mode",
        action="store", type="choice", choices=["debug", "release"])
    options.add_option("--clean", dest="clean", action="store_true")
    options.add_option("--build-extension", dest="build_extension", action="store_true")
    options.add_option("--prep", dest="prep", action="store_true")
    options.add_option("--build-installer", dest="build_installer", action="store")
    options.add_option("--aeb-output", dest="aeb_output", action="store")
    options.add_option("--sdk-version", dest="sdk_version", action="store")
    options.add_option("--concurrency", dest="concurrency", action="store")
    options.add_option("-q", "--quiet", help="Print fewer messages",
        action="store_const", dest="verbosity", const=0)
    options.add_option("-v", "--verbose", help="Print more messages",
        action="store_const", dest="verbosity", const=2)
    options.add_option("--update-version", dest="update_version_scheme",
        action="store", type="choice",
        choices=["milestone", "nightly", "none", "production",
                 "rc", "unofficial"],
        help="Use milestone, nightly, production, rc, unofficial or "\
             "none.  When using milestone, production, or rc you will "\
             "need to use --new-version to provide the complete "\
             "version #. "\
             "Using production will mark the extension "\
             "as production, all others will mark the extension "\
             "as not for production.  The unofficial and nightly "\
             "will mutate the existing version #, so --new-version is "\
             "not required.")
    options.add_option("--new-version", dest="new_version",
        action="store", type="string")
    options.set_defaults(mode="release", clean=False,
        build_extension=False,
        sdk_verson=None,
        prep=False, concurrency=1, verbosity=1,
        update_version_scheme="none")
    options = options.parse_args(args[1:])[0]
    if not(is_windows()):
        options.solaris_dir = None

    builder = None
    try:
        opticks_depends = os.environ.get("OPTICKSDEPENDENCIES", None)
        if options.dependencies:
            #allow the -d command-line option to override
            #environment variable
            opticks_depends = options.dependencies
        if not opticks_depends:
            #didn't use -d command-line option, nor an environment variable
            #so consider that an error
            raise ScriptException("Dependencies argument must be provided")
        if not os.path.exists(opticks_depends):
            raise ScriptException("Dependencies path is invalid")

        opticks_code_dir = os.environ.get("OPTICKS_CODE_DIR", None)
        if options.opticks_code_dir:
            opticks_code_dir = options.opticks_code_dir
        if not opticks_code_dir:
            raise ScriptException("Opticks code dir argument must be provided")
        if not os.path.exists(opticks_code_dir):
            raise ScriptException("Opticks code dir is invalid")

        opticks_build_dir = join(opticks_code_dir, "Build")
        if not opticks_build_dir:
            raise ScriptException("Opticks build directory argument "\
                "must be provided")
        if not os.path.exists(opticks_build_dir):
            raise ScriptException("Opticks build directory path is invalid")

        if options.build_installer:
            if options.verbosity > 1:
                print "Building AEB installation extension..."
            aeb_output = None
            if options.aeb_output:
               aeb_output = options.aeb_output
            aeb_platforms = []
            if options.build_installer == "all":
                 aeb_platforms = aeb_platform_mappings.values()
            else:
                plats = options.build_installer.split(',')
                for plat in plats:
                    plat = plat.strip()
                    if plat in aeb_platform_mappings:
                        aeb_platforms.append(aeb_platform_mappings[plat])
                    else:
                        aeb_platforms.append(plat)
            build_installer(aeb_platforms,
                aeb_output, opticks_depends, options.sdk_version,
                options.verbosity, options.solaris_dir)
            if options.verbosity > 1:
                print "Done building installer"
            return 0

        if options.mode == "debug":
            build_in_debug = True
        else:
            build_in_debug = False

        if not is_windows():
            builder = SolarisBuilder(opticks_depends, opticks_code_dir, build_in_debug,
                opticks_build_dir, options.verbosity)
        else:
            if not os.path.exists(options.visualstudio):
                raise ScriptException("Visual Studio path is invalid")

            if options.arch == "32":
                builder = Windows32bitBuilder(opticks_depends, opticks_code_dir,
                    build_in_debug, opticks_build_dir,
                    options.visualstudio, options.verbosity)
            if options.arch == "64":
                builder = Windows64bitBuilder(opticks_depends, opticks_code_dir,
                    build_in_debug, opticks_build_dir,
                    options.visualstudio, options.verbosity)

        builder.update_version_number(options.update_version_scheme,
            options.new_version)
                
        if options.build_extension:
           builder.build_executable(options.clean,
               options.concurrency)

        if options.prep:
            if options.verbosity > 1:
                print "Prepping to run..."
            builder.prep_to_run()
            if options.verbosity > 1:
                print "Done prepping to run"

    except Exception, e:
        print "--------------------------"
        traceback.print_exc()
        print "--------------------------"
        return 2000

    return 0

if __name__ == "__main__":
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
    retcode = main(sys.argv)
    print "Return code is", retcode
    sys.exit(retcode)
