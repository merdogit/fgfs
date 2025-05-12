
SET CMAKE_TOOLCHAIN="Visual Studio 16 2019"

SET THIRDPARTY_DIR=C:\FGFS\windows-3rd-party\msvc140

SET ROOT_DIR=%CD%
SET SOURCE=C:\FGFS\c-ares-1.34.2


cmake -B build-c-ares -S %SOURCE% -G %CMAKE_TOOLCHAIN% -A "x64" ^
    -D CMAKE_INSTALL_PREFIX:PATH=%ROOT_DIR%\install_64\c_ares 

cmake --build build-c-ares --config RelWithDebInfo
cmake --install build-c-ares --config RelWithDebInfo



xcopy /S /Y %ROOT_DIR%\install_64\c_ares\include\* %THIRDPARTY_DIR%\3rdParty.x64\include\
xcopy /S /Y %ROOT_DIR%\install_64\c_ares\bin\cares.* %THIRDPARTY_DIR%\3rdParty.x64\bin\
xcopy /S /Y %ROOT_DIR%\install_64\c_ares\lib\cares.* %THIRDPARTY_DIR%\3rdParty.x64\lib\
xcopy /S /Y %ROOT_DIR%\install_64\c_ares\lib\cmake\* %THIRDPARTY_DIR%\3rdParty.x64\lib\



cmake -B build-c-ares-32 -S %SOURCE% -G %CMAKE_TOOLCHAIN% -A "Win32" ^
    -D CMAKE_INSTALL_PREFIX:PATH=%ROOT_DIR%\install_32\c_ares 

cmake --build build-c-ares-32 --config RelWithDebInfo
cmake --install build-c-ares-32 --config RelWithDebInfo



xcopy /S /Y %ROOT_DIR%\install_32\c_ares\include\* %THIRDPARTY_DIR%\3rdParty\include\
xcopy /S /Y %ROOT_DIR%\install_32\c_ares\bin\cares.* %THIRDPARTY_DIR%\3rdParty\bin\
xcopy /S /Y %ROOT_DIR%\install_32\c_ares\lib\cares.* %THIRDPARTY_DIR%\3rdParty\lib\
xcopy /S /Y %ROOT_DIR%\install_32\c_ares\lib\cmake\* %THIRDPARTY_DIR%\3rdParty\lib\
