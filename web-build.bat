emcmake cmake -B ./web-build -DCMAKE_BUILD_TYPE=Release && cmake --build ./web-build && python3 -m http.server --directory ./web-build