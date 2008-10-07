@echo off
set OPTICKS_HOME=C:\Program Files\Opticks\4.2.2
set OPTICKS_CODE_DIR=c:\Opticks\4.2.2-SDK
set OPTICKSDEPENDENCIES=c:\Opticks\TrunkOpticks\Dependencies
call "C:\Program Files (x86)\Microsoft Visual Studio 8\VC\vcvarsall.bat" x86
"C:\Program Files (x86)\Common Files\Microsoft Shared\MSEnv\VSLauncher.exe" LIDAR.sln
