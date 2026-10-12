@echo off
rem Extrait les en-tetes et bibliotheques du Windows SDK 7.1 depuis la copie de
rem l'ISO (deps\iso-sdk71), sans passer par l'installeur qui echoue sous Windows 11.
rem Resultat : deps\winsdk71\
cd /d "%~dp0.."
set SRC=%CD%\deps\iso-sdk71\Setup
if not exist "%SRC%\WinSDKBuild_amd64\WinSDKBuild_amd64.msi" (echo Copie de l'ISO introuvable dans deps\iso-sdk71. & pause & exit /b 1)
for %%M in ("%SRC%\WinSDKBuild_amd64\WinSDKBuild_amd64.msi" "%SRC%\WinSDK_amd64\WinSDK_amd64.msi") do (
    echo Extraction de %%~nxM ...
    msiexec /a "%%~fM" /qb TARGETDIR="%CD%\deps\winsdk71"
)
echo.
echo Termine. Contenu extrait dans %CD%\deps\winsdk71
pause
