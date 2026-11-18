@echo off
setlocal
cd /d "%~dp0\.."
if not exist build\media mkdir build\media
ffmpeg -hide_banner -loglevel error -f lavfi -i testsrc2=size=160x90:rate=8 -t 1 -c:v libvpx-vp9 -deadline realtime -cpu-used 8 -y build\media\vp9.webm
if errorlevel 1 exit /b 1
ffmpeg -hide_banner -loglevel error -f lavfi -i testsrc2=size=160x90:rate=8 -t 1 -c:v libaom-av1 -cpu-used 8 -crf 45 -y build\media\av1.webm
if errorlevel 1 exit /b 1
ffmpeg -hide_banner -loglevel error -f lavfi -i testsrc2=size=160x90:rate=8 -f lavfi -i sine=frequency=440:sample_rate=44100 -t 1 -c:v libx264 -preset ultrafast -c:a aac -y build\media\h264-aac.mp4
if errorlevel 1 exit /b 1
ffmpeg -hide_banner -loglevel error -f lavfi -i sine=frequency=440:sample_rate=44100 -t 1 -c:a aac -y build\media\aac.m4a
if errorlevel 1 exit /b 1
ffmpeg -hide_banner -loglevel error -f lavfi -i sine=frequency=440:sample_rate=44100 -t 1 -c:a pcm_s16le -y build\media\pcm.wav
exit /b %errorlevel%
