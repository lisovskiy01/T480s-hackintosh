@echo off
pushd %~dp0

set confgen=bin\confgen.exe
set target=..\EFI\CLOVER\config.plist
set example=..\EFI\CLOVER\config.example.plist
set force=false
set hasrun=false

if "%1"=="-f" set force=true
if "%1"=="--force" set force=true

if exist %target% (
    if "%force%"=="false" (
        %confgen% -t %example% -i %target% -o %target%
        set hasrun=true
    )
)
if "%hasrun%"=="false" (
    if "%force%"=="true" (
        echo Force overwriting config.plist.
    )
    %confgen% -t %example% -o %target%
)

pause
