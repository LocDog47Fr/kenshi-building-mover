@echo off
rem Telecharge KenshiLib + Boost 1.60 dans deps\ (depot KenshiLib_Examples_deps)
rem et lance son Setup.bat.
rem Necessite Git for Windows (avec Git LFS, inclus par defaut).
cd /d "%~dp0"
where git >nul 2>nul || (echo Git introuvable. Installe Git for Windows puis relance. & pause & exit /b 1)
git lfs install
IF NOT EXIST deps\.git (
    git clone https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps.git deps || (pause & exit /b 1)
)
cd deps
git lfs pull
call Setup.bat
echo.
echo Termine. Ferme puis rouvre Visual Studio pour qu'il voie les nouvelles variables.
pause
