@echo off
echo Building ESPOverwatch.exe...
cl /EHsc /std:c++17 /FeESPOverwatch.exe main.cpp memory.cpp math.cpp overlay.cpp /link gdiplus.lib user32.lib kernel32.lib psapi.lib
if %errorlevel%==0 (
    echo Build successful. Run as Administrator.
) else (
    echo Build failed.
)
