# docker build --network=host -t moonlight-n3ds
FROM ubuntu:20.04

# Use bash instead of sh
SHELL ["/bin/bash", "-c"]

# Make sure Docker does not freeze while setting up the timezone
ENV TZ=Asia/Dubai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y build-essential \
    curl \
    gdb \
    lsb-release \
    libreadline-dev \
    software-properties-common \
    wget

# Install devkitPro for 3DS
RUN wget https://apt.devkitpro.org/install-devkitpro-pacman && \
    chmod +x ./install-devkitpro-pacman && \
    echo "y" | ./install-devkitpro-pacman
RUN dkp-pacman -S 3ds-dev --noconfirm && \
    dkp-pacman -Syu 3ds-curl --noconfirm && \
    dkp-pacman -Syu 3ds-libarchive 3ds-jansson 3ds-libjpeg-turbo 3ds-libpng --noconfirm

CMD ["/bin/bash"]
