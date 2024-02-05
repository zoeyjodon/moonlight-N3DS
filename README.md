# Moonlight for the \*New\* Nintendo 3DS

Moonlight is an open source client for [Sunshine](https://github.com/LizardByte/Sunshine) and NVIDIA GameStream for the \*New\* Nintendo 3DS, forked from [Moonlight Embedded](https://github.com/moonlight-stream/moonlight-embedded). Moonlight allows you to stream your full collection of games and applications from your PC to other devices to play them remotely.

## Original 3DS Note

While this app is operable on the original 3DS, the hardware decoder must be disabled and the framerate will be significantly lower due to the slower CPU. It is not recommended to use this app on the original 3DS.

## Configuration

You can modify documented settings either in the app, or by creating/modifying the config file located at `sd:/3ds/moonlight/moonlight.conf`.

## Documentation

More information about installing and runnning Moonlight Embedded is available on the [wiki](https://github.com/moonlight-stream/moonlight-embedded/wiki).

## Build

I have included a Dockerfile which has all of the required build dependencies pre-installed. The easiest way to build moonlight.cia is by building and running the docker image.
If you are using VS Code as your editor, you can use the `Build Docker` and `Run Docker` tasks for this.
If you are not using VS Code, you can build and run the docker image with the following terminal commands from the root of the repository:

```bash
docker build --network=host -t moonlight-n3ds .
docker run --rm -it -v .:/moonlight-N3DS -w /moonlight-N3DS moonlight-n3ds:latest
```

Then, run the following commands in the docker commandline:

```bash
source /etc/profile.d/devkit-env.sh
make
```

## Install

You can download the CIA file (moonlight.cia) from the [Releases](https://github.com/zoeyjodon/moonlight-N3DS/releases/latest) page, and install it using [FBI](https://github.com/Steveice10/FBI).

Please note that to run Moonlight on the 3DS, you will need to install custom firmware. You can find instructions for installing CFW [here](https://3ds.hacks.guide/).

## See also

[Moonlight-common-c](https://github.com/moonlight-stream/moonlight-common-c) is the shared codebase between different Moonlight implementations

## Contribute

1. Fork us
2. Write code
3. Send Pull Requests
