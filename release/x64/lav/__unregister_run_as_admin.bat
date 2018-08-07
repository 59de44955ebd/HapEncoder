cd /d "%~dp0"

:: LAV filters
regsvr32.exe /s /u LAVAudio.ax
regsvr32.exe /s /u LAVVideo.ax
regsvr32.exe /s /u LAVSplitter.ax
