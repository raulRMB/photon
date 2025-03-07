@echo off
set FILE=C:/dev/photon/web-build/photon.html
set DATA=C:/dev/photon/web-build/photon.data

if exist "%FILE%" (
  rm %FILE%
  echo %FILE% deleted.
)

if exist "%DATA%" (
  rm %DATA%
  echo %DATA% deleted.
)

emcmake cmake -B ./web-build -DCMAKE_BUILD_TYPE=Release && cmake --build ./web-build && python3 -m http.server 8080 --directory ./web-build
