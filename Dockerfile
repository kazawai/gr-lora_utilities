from archlinux:latest

workdir /lib

# Update to latest arch
run pacman -Syu --noconfirm

# Install required dependencies
run pacman -S git cppunit fftw boost boost-libs gnuradio gnuradio-osmosdr libvolk log4cpp base-devel cmake wxgtk3 gnuradio-companion pybind11 gtk4 gtk3  --noconfirm

workdir /liquid

# Manual liquid-dsp install
run git clone https://github.com/jgaeddert/liquid-dsp.git . && \
    sh ./bootstrap.sh && \
    sh ./configure --prefix=/usr && \
    make && \
    make install

# Install gr-lora
workdir /src

arg CACHEBUST
run git clone https://github.com/kazawai/gr-lora_utilities.git . && \
    mkdir build && \
    cd build && \
    cmake ../ -DCMAKE_INSTALL_PREFIX=/usr && \
    make && \
    make install && \
    ldconfig

workdir /src/apps

expose 40868
