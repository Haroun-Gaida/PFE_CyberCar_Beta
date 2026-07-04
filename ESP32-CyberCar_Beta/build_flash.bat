@echo off
setlocal

set "IDF_PATH=C:\Users\harou\.platformio\packages\framework-espidf"
set "IDF_TOOLS_PATH=C:\Users\harou\.espressif"
set "IDF_PYTHON_ENV_PATH=C:\Users\harou\.espressif\python_env\idf5.3_py3.14_env"
set "PYTHON=%IDF_PYTHON_ENV_PATH%\Scripts\python.exe"
set "IDF_PY=%IDF_PATH%\tools\idf.py"

set "PATH=C:\Users\harou\.platformio\packages\tool-cmake\bin;C:\Users\harou\.platformio\packages\tool-ninja;C:\Users\harou\.platformio\packages\toolchain-xtensa-esp-elf\bin;C:\Users\harou\.espressif\tools\xtensa-esp-elf\esp-13.2.0_20240530\xtensa-esp-elf\bin;%IDF_PYTHON_ENV_PATH%\Scripts;C:\Users\harou\.platformio\packages\tool-esptoolpy;%PATH%"

echo.
echo ============================================
echo  ESP32-Deauther Build + Flash
echo  IDF: %IDF_PATH%
echo  PORT: %PORT%
echo ============================================
echo.

if "%1"=="flash" goto flash
if "%1"=="spiffs" goto spiffs
if "%1"=="all" goto all

:build
echo [1/3] Building firmware...
"%PYTHON%" "%IDF_PY%" -DIDF_TARGET=esp32 build
if errorlevel 1 goto error
echo.
echo Build complete!
goto done

:flash
echo [1/3] Building firmware...
"%PYTHON%" "%IDF_PY%" -DIDF_TARGET=esp32 build
if errorlevel 1 goto error
echo.
echo [2/3] Flashing firmware to COM9...
"%PYTHON%" "%IDF_PY%" -DIDF_TARGET=esp32 -p COM9 -b 460800 flash
if errorlevel 1 goto error
echo.
echo Flash complete!
goto done

:spiffs
echo [SPIFFS] Note: SPIFFS data is included in the main flash step automatically.
echo No separate SPIFFS flash needed with this IDF version.
goto done

:all
echo [1/3] Building firmware...
"%PYTHON%" "%IDF_PY%" -DIDF_TARGET=esp32 build
if errorlevel 1 goto error
echo.
echo [2/3] Flashing all partitions to COM9 (includes storage/SPIFFS)...
"%PYTHON%" "%IDF_PY%" -DIDF_TARGET=esp32 -p COM9 -b 460800 flash
if errorlevel 1 goto error
echo.
echo [3/3] Done! All partitions flashed successfully.
goto done

:error
echo.
echo *** BUILD/FLASH FAILED ***
exit /b 1

:done
endlocal
