@echo off

rem @if "%1c"=="c" goto usage

@goto install



:usage

@echo.

@echo USAGE: Install [current-working-directory]

@echo The current-working-directory is the full path containing all the

@echo IMB driver installation files, including this command file.

@echo.

@echo Example:

@echo.

@echo         Install C:\IMBDRV\Test

@echo.

@goto error



:install

@set INSTALL_DIR=%1

@echo.

DeviceSetup.exe -v install flashud.inf *INT0800

DeviceSetup.exe -v install IBSMUtil.inf *IntelUtilDeV

@echo.

@echo Installing IMB driver...

@echo.

devicesetup updateonly imbdrv.inf *IMBDRV >NUL 1>&2



if errorlevel 0 echo "IMBDriver upgraded successfully"  && goto exit

DeviceSetup.exe detectmsipmi

if errorlevel 0 echo "MSIPMI Driver detected. Skipping IMBDRV installation" && goto exit


devicesetup.exe -v install imbdrv.inf *IMBDRV



:done

@echo Done!

@echo.

@echo Installation Successful

@echo.

@goto exit



:error

@echo.

@echo @@@@ Installation Failed @@@@

@echo.



:exit



