# docker build --network=host -t moonlight-n3ds .
#FROM ubuntu:22.04
FROM devkitpro/devkitarm:latest

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
    libtool \
    python3 \
    python3-pip \
    ffmpeg

# Install the luma dump parser for inspecting crashes
RUN pip install -U git+https://github.com/LumaTeam/luma3ds_exception_dump_parser.git

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

# Install bannertool
RUN wget https://github.com/Epicpkmn11/bannertool/releases/download/v1.2.2/bannertool.zip && \
    unzip bannertool.zip -d /bannertool && \
    cp /bannertool/linux-x86_64/bannertool /usr/local/bin && \
    chmod +x /usr/local/bin/bannertool && \
    rm -r /bannertool

# Install MakeROM for CIA packaging
RUN wget https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.18.3/makerom-v0.18.3-ubuntu_x86_64.zip && \
    unzip makerom-v0.18.3-ubuntu_x86_64.zip -d /usr/local/bin && \
    chmod +x /usr/local/bin/makerom

# Install custom third party libraries
COPY . /moonlight-N3DS

RUN source /etc/profile.d/devkit-env.sh && /moonlight-N3DS/3ds/build-expat.sh
RUN source /etc/profile.d/devkit-env.sh && /moonlight-N3DS/3ds/build-openssl.sh
RUN source /etc/profile.d/devkit-env.sh && /moonlight-N3DS/3ds/build-ffmpeg.sh
RUN rm -rf /moonlight-N3DS

CMD ["/bin/bash"]
