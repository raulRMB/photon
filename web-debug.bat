emcmake cmake -B ./web-build -DCMAKE_BUILD_TYPE=Debug && cmake --build ./web-build && python3 -m http.server 8080 --directory ./web-build
