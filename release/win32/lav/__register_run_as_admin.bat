cd /d "%~dp0"

:: LAV filters
regsvr32.exe /s LAVAudio.ax
regsvr32.exe /s LAVVideo.ax
regsvr32.exe /s LAVSplitter.ax
