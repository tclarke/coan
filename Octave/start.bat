@echo off
set OPTICKS_HOME=c:\Program Files (x86)\Opticks\4.2.3
set OPTICKS_CODE_DIR=c:\Opticks\4.2.3-SDK
set OPTICKSDEPENDENCIES=c:\Opticks\4.2.3-SDK\Dependencies
call "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\vcvarsall.bat" x86
"C:\Program Files (x86)\Common Files\Microsoft Shared\MSEnv\VSLauncher.exe" Octave.sln
