# docker build --network=host -t moonlight-n3ds
# docker run --rm -it -v C:\Users\jodon\source\repos\moonlight-N3DS:/moonlight-N3DS -w /moonlight-N3DS moonlight-n3ds:latest
FROM ubuntu:20.04

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
RUN apt-get install apt-transport-https && \
    mkdir -p /usr/local/share/keyring/ && \
    wget -O /usr/local/share/keyring/devkitpro-pub.gpg https://apt.devkitpro.org/devkitpro-pub.gpg && \
    echo "deb [signed-by=/usr/local/share/keyring/devkitpro-pub.gpg] https://apt.devkitpro.org stable main" > /etc/apt/sources.list.d/devkitpro.list
RUN apt-get update && apt-get install -y devkitpro-pacman
RUN echo "\n" | dkp-pacman -S 3ds-dev

CMD ["/bin/bash"]
