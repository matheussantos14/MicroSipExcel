@echo off
rem Compila SQLiteCpp + sqlite3 como bibliotecas estaticas Win32 (/MT), para o MicroSIP | Excel.
setlocal
set DEPS=%~dp0
set VSDIR=C:\Program Files\Microsoft Visual Studio\18\Community
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul || exit /b 1

set OUT=%DEPS%lib\Win32\Release
set OBJ=%DEPS%obj\sqlitecpp
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%" mkdir "%OBJ%"

set CFLAGS=/nologo /c /O2 /MT /EHsc /W3 /DNDEBUG /DWIN32 /D_WINDOWS /DSQLITE_ENABLE_COLUMN_METADATA /DSQLITE_THREADSAFE=1
set INC=/I"%DEPS%SQLiteCpp\include" /I"%DEPS%SQLiteCpp\sqlite3"

cl %CFLAGS% %INC% /Fo"%OBJ%\sqlite3.obj" "%DEPS%SQLiteCpp\sqlite3\sqlite3.c" || exit /b 1
lib /nologo /OUT:"%OUT%\sqlite3.lib" "%OBJ%\sqlite3.obj" || exit /b 1

cl %CFLAGS% /std:c++17 %INC% /Fo"%OBJ%\\" "%DEPS%SQLiteCpp\src\*.cpp" || exit /b 1
lib /nologo /OUT:"%OUT%\SQLiteCpp.lib" "%OBJ%\Backup.obj" "%OBJ%\Column.obj" "%OBJ%\Database.obj" "%OBJ%\Exception.obj" "%OBJ%\Savepoint.obj" "%OBJ%\Statement.obj" "%OBJ%\Transaction.obj" || exit /b 1

echo OK
endlocal
