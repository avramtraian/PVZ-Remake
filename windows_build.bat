:: Copyright (c) 2025 Traian Avram. All rights reserved.
:: This source file is part of the PvZ-Remake project and is distributed under the MIT license.

@ECHO OFF
PUSHD "%~dp0"
IF NOT EXIST "build/" ( MKDIR "build" )
PUSHD "build"
CLS

SET SourceFiles=../source/pvz.cpp ^
				../source/pvz_asset.cpp ^
				../source/pvz_memory.cpp ^
				../source/pvz_renderer.cpp ^
				../source/pvz_windows.cpp

::
:: Description of the common compiler flags used below:
::   /nologo                - Don't print information about the MSVC compiler when running this script.
::   /std:c++17 			- Use C++ 17 as the standard.
::   /GR-                   - Disables runtime type information (RTTI).
::   /D_HAS_EXCEPTIONS=0    - Disables exception handling.
::   /Zi                    - Generate debug information (.pdb files).
::   /Od                    - (Optional) Do not perform any optimizations.
::   /O2                    - (Optional) Perform full optimizations.
::
SET CommonCompilerDefines=/DPVZ_WINDOWS /DPVZ_INTERNAL
SET CommonCompilerFlags=/nologo /std:c++17 /GR- /D_HAS_EXCEPTIONS=0 /Zi /Ox
SET CommonLinkerFlags=/nologo

ECHO Compiling game source...
cl %CommonCompilerFlags% %CommonCompilerDefines% %SourceFiles% /link %CommonLinkerFlags% user32.lib gdi32.lib /OUT:PVZ-Remake.exe
ECHO Done.

ECHO.
ECHO Compiling tools source...
cl %CommonCompilerFlags% %CommonCompilerDefines% ../source/tools/pvzt_bap.cpp /link %CommonLinkerFlags% /OUT:bap.exe
IF %ERRORLEVEL%==0 (
	SET BAP_COMPILED_SUCCESSFULLY=1
) ELSE (
	SET BAP_COMPILED_SUCCESSFULLY=0
)
ECHO Done.

ECHO.
IF %BAP_COMPILED_SUCCESSFULLY%==1 (
	ECHO Building the asset pack...
	bap "../assets" "PVZ-Remake-Assets.data"
	ECHO Done.
) ELSE (
	ECHO The asset pack build tool was not successfully compiled. Skipping asset pack building!
)

POPD
POPD
