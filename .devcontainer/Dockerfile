# .devcontainer/Dockerfile
FROM ubuntu:20.04

# [Optional] Uncomment this section to install additional OS packages.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
     && apt-get -y install --no-install-recommends gcc g++ gcc-multilib g++-multilib git make python3 python3-pip python3-setuptools

RUN git clone https://github.com/alliedmodders/ambuild /opt/ambuild
RUN python3 -m pip install /opt/ambuild

RUN mkdir -p /mnt/hl2sdk
RUN git clone -b css https://github.com/alliedmodders/hl2sdk /mnt/hl2sdk/hl2sdk-css
RUN git clone -b episode1 https://github.com/alliedmodders/hl2sdk /mnt/hl2sdk/hl2sdk-episode1

RUN git clone -b 1.6-dev https://github.com/alliedmodders/sourcemod /mnt/sourcemod
WORKDIR /mnt/sourcemod
RUN git submodule init
RUN git submodule update

RUN git clone -b 1.10-dev https://github.com/alliedmodders/metamod-source /mnt/metamod-source

# [Optional] Uncomment this line to install global node packages.
# RUN su vscode -c "source /usr/local/share/nvm/nvm.sh && npm install -g <your-package-here>" 2>&1
