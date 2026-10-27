@echo off
setlocal
cd /d "%~dp0"

if not exist ".venv\Scripts\python.exe" (
    echo The virtual environment is missing.
    echo Run install_and_run.bat first.
    pause
    exit /b 1
)

call ".venv\Scripts\activate.bat"
python -m streamlit run app.py
