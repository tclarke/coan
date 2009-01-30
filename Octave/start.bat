@echo off
set OPTICKS_HOME=c:\Opticks\TrunkOpticks\Code\Release
set OPTICKS_CODE_DIR=c:\Opticks\TrunkOpticks\Code
set OPTICKSDEPENDENCIES=c:\Opticks\TrunkOpticks\Dependencies
call "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\vcvarsall.bat" x86
"C:\Program Files (x86)\Common Files\Microsoft Shared\MSEnv\VSLauncher.exe" Octave.sln
rem "C:\Program Files\Microsoft Visual Studio 8\Common7\IDE\VCExpress.exe"
