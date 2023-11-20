# docker build --network=host -t moonlight-n3ds .
FROM ubuntu:22.04

# Use bash instead of sh
SHELL ["/bin/bash", "-c"]

# Make sure Docker does not freeze while setting up the timezone
ENV TZ=Asia/Dubai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Install common build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    curl \
    gdb \
    lsb-release \
    libreadline-dev \
    software-properties-common \
    wget \
    unzip \
    libc6 \
    git \
    autoconf \
    libtool

# Install moonlight dependencies
RUN apt-get install -y \
    libssl-dev \
    libopus-dev \
    libasound2-dev \
    libudev-dev \
    libavahi-client-dev \
    libcurl4-openssl-dev \
    libevdev-dev \
    libexpat1-dev \
    libpulse-dev \
    uuid-dev \
    cmake \
    gcc \
    g++

# Install devkitPro for 3DS
RUN wget https://apt.devkitpro.org/install-devkitpro-pacman && \
    chmod +x ./install-devkitpro-pacman && \
    echo "y" | ./install-devkitpro-pacman
RUN dkp-pacman -S 3ds-dev --noconfirm && \
    dkp-pacman -Syu 3ds-curl --noconfirm && \
    dkp-pacman -Syu 3ds-libarchive \
                    3ds-jansson \
                    3ds-libjpeg-turbo \
                    3ds-libpng \
                    3ds-tinyxml2 \
                    3ds-freetype \
                    3ds-curl \
                    3ds-libopus \
                    --noconfirm

# Install bannertool
RUN wget https://github.com/Steveice10/bannertool/releases/download/1.2.0/bannertool.zip && \
    unzip bannertool.zip -d /bannertool && \
    cp /bannertool/linux-x86_64/bannertool /usr/local/bin && \
    chmod +x /usr/local/bin/bannertool && \
    rm -r /bannertool

# Install MakeROM for CIA packaging
RUN wget https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.18.4/makerom-v0.18.4-ubuntu_x86_64.zip && \
    unzip makerom-v0.18.4-ubuntu_x86_64.zip -d /usr/local/bin && \
    chmod +x /usr/local/bin/makerom

# Install custom third party libraries
COPY . /moonlight-N3DS

RUN source /etc/profile.d/devkit-env.sh && /moonlight-N3DS/3ds/build-expat.sh
RUN source /etc/profile.d/devkit-env.sh && /moonlight-N3DS/3ds/build-openssl.sh
RUN source /etc/profile.d/devkit-env.sh && /moonlight-N3DS/3ds/build-sdl.sh
RUN source /etc/profile.d/devkit-env.sh && /moonlight-N3DS/3ds/build-ffmpeg.sh
RUN rm -rf /moonlight-N3DS

CMD ["/bin/bash"]
