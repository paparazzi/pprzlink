# Installation example
1. `mkdir build && cd build`
2. `export tinyxml2_DIR=/path/to/tinyxml2` (might not be needed depending on your install)
3. `cmake ..` (use `ccmake ..` to easily configure install path `CMAKE_INSTALL_PREFIX` or other parameters)
4. `make -j8`
5. `make install` (`sudo make install` is probably required if installing system wide)
