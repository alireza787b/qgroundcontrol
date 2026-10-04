FROM ubuntu:24.04@sha256:786a8b558f7be160c6c8c4a54f9a57274f3b4fb1491cf65146521ae77ff1dc54

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential=12.10ubuntu1 \
        ca-certificates=20260601~24.04.1 \
        ccache=4.9.1-1 \
        file=1:5.45-3build1 \
        git=1:2.43.0-1ubuntu7.3 \
        gstreamer1.0-gl=1.24.2-1ubuntu0.4 \
        gstreamer1.0-libav=1.24.1-1build1 \
        gstreamer1.0-plugins-bad=1.24.2-1ubuntu4 \
        gstreamer1.0-plugins-base=1.24.2-1ubuntu0.4 \
        gstreamer1.0-plugins-good=1.24.2-1ubuntu1.6 \
        gstreamer1.0-plugins-ugly=1.24.1-1build1 \
        gstreamer1.0-x=1.24.2-1ubuntu0.4 \
        libegl-dev=1.7.0-1build1 \
        libfontconfig1-dev=2.15.0-1.1ubuntu2 \
        libfreetype-dev=2.13.2+dfsg-1ubuntu0.1 \
        libgl-dev=1.7.0-1build1 \
        libgl1-mesa-dri=25.2.8-0ubuntu0.24.04.2 \
        libglx-mesa0=25.2.8-0ubuntu0.24.04.2 \
        libgstreamer-plugins-base1.0-dev=1.24.2-1ubuntu0.4 \
        libgstreamer1.0-dev=1.24.2-1ubuntu0.1 \
        libopengl-dev=1.7.0-1build1 \
        libpulse-dev=1:16.1+dfsg1-2ubuntu10.1 \
        libspeechd2=0.12.0~rc2-2build3 \
        libudev-dev=255.4-1ubuntu8.17 \
        libusb-1.0-0-dev=2:1.0.27-1 \
        libvulkan-dev=1.3.275.0-1build1 \
        libx11-xcb-dev=2:1.8.7-1build1 \
        libxcb-cursor-dev=0.1.4-1build1 \
        libxcb-icccm4-dev=0.4.1-1.1build3 \
        libxcb-image0-dev=0.4.0-2build1 \
        libxcb-keysyms1-dev=0.4.0-1build4 \
        libxcb-randr0-dev=1.15-1ubuntu2 \
        libxcb-render-util0-dev=0.3.9-1build4 \
        libxcb-shape0-dev=1.15-1ubuntu2 \
        libxcb-sync-dev=1.15-1ubuntu2 \
        libxcb-xfixes0-dev=1.15-1ubuntu2 \
        libxcb-xinerama0-dev=1.15-1ubuntu2 \
        libxkbcommon-dev=1.6.0-1build1 \
        libxkbcommon-x11-dev=1.6.0-1build1 \
        patchelf=0.18.0-1.1build1 \
        pkg-config=1.8.1-2build1 \
        python3=3.12.3-0ubuntu2.1 \
        xauth=1:1.1.2-1build1 \
        xvfb=2:21.1.12-1ubuntu1.6 \
    && rm -rf /var/lib/apt/lists/*
