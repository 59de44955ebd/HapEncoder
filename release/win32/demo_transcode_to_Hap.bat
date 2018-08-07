::::::::::::::::::::::::::::::::::::::::
:: HapEncoder Demo - Transcode MP4 to Hap
::::::::::::::::::::::::::::::::::::::::
@echo off
cd "%~dp0"

:: CLSID constants
set CLSID_LAVSplitterSource={B98D13E7-55DB-4385-A33D-09FD1BA26338}
set CLSID_LAVVideoDecoder={EE30215D-164F-4A92-A4EB-9D4C13390F9F}
set CLSID_HapEncoder={8A2D8B52-1689-4FA5-9498-335109624DE7}
set CLSID_AVIMux={E2510970-F137-11CE-8B67-00AA00A3F1A6}
set CLSID_FileWriter={8596E5F0-0DA5-11D0-BD21-00A0C911CE86}

echo.
echo ============================================================
echo Transcoding video to AVI with Hap codec - please be patient...
echo ============================================================
echo.

bin\dscmd^
 -graph ^
%CLSID_LAVSplitterSource%;file=lav\LAVSplitter.ax;src=..\assets\bbb_360p_10sec.mp4,^
%CLSID_LAVVideoDecoder%;file=lav\LAVVideo.ax,^
%CLSID_HapEncoder%;file=HapEncoder.ax;dialog,^
%CLSID_AVIMux%,^
%CLSID_FileWriter%;dest=Hap.avi!^
0:1,1:2,2:3,3:4^
 -size 480,120^
 -progress^
 -winCaption "Transcoding to ""Hap.avi""..."

echo.
echo.

if "%ERRORLEVEL%"=="0" (
	echo ============================================================
	echo The video was successfully transcoded to file "Hap.avi"
	echo ============================================================
) else (
	============================================================
	echo ERROR: Transcoding failed.
	============================================================
)

echo.
pause