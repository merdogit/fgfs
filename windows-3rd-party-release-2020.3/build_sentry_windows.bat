
SET PATH=%PATH%;C:\sentry


SET CMAKE_TOOLCHAIN="Visual Studio 16 2019"


SET SOURCE=C:\FGFS\sentry-native
SET THIRDPARTY_DIR=C:\FGFS\windows-3rd-party\msvc140

rd /S /Q install_64\sentry
rd /S /Q install_32\sentry

cmake %SOURCE% -G %CMAKE_TOOLCHAIN% -A x64 -B build-sentry ^ 
                 -D CMAKE_INSTALL_PREFIX:PATH=%ROOT_DIR%\install_64\sentry 

cmake --build build-sentry --config RelWithDebInfo --target INSTALL

cmake %SOURCE% -G %CMAKE_TOOLCHAIN% -A Win32 -B build-sentry-32 ^
                 -D CMAKE_INSTALL_PREFIX:PATH=%ROOT_DIR%\install_32\sentry 

cmake --build build-sentry-32 --config RelWithDebInfo --target INSTALL

cd %ROOT_DIR%

xcopy /S /Y %ROOT_DIR%\install_64\sentry\* %THIRDPARTY_DIR%\3rdParty.x64\
xcopy /S /Y %ROOT_DIR%\install_32\sentry\* %THIRDPARTY_DIR%\3rdParty\

SET SENTRY_ORG=flightgear
SET SENTRY_PROJECT=flightgear
REM SET SENTRY_AUTH_TOKEN=

sentry-cli upload-dif %ROOT_DIR%\install_64\sentry\bin\sentry.pdb
sentry-cli upload-dif %ROOT_DIR%\install_32\sentry\bin\sentry.pdb
