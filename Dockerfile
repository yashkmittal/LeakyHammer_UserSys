FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive  

RUN apt-get update && apt-get install -y \
    build-essential=12.8ubuntu1.1 \
    g++-10=10.5.0-1ubuntu1~20.04 \
    gcc-10-base:amd64=10.5.0-1ubuntu1~20.04 \
    wget=1.20.3-1ubuntu2.1 \
    curl=7.68.0-1ubuntu2.25 \
    git=1:2.25.1-1ubuntu3.14 \
    cmake=3.16.3-1ubuntu1 \
    cmake-data=3.16.3-1ubuntu1 \
    python3=3.8.2-0ubuntu2 \
    python3-pip=20.0.2-5ubuntu1.11 \
    scons=3.1.2-2 \
    python3-dev \
    libpython3-dev:amd64=3.8.2-0ubuntu2 \
    libpython3-stdlib:amd64=3.8.2-0ubuntu2 \
    libprotobuf-dev:amd64=3.6.1.3-2ubuntu5.2 \
    protobuf-compiler=3.6.1.3-2ubuntu5.2 \
    libgoogle-perftools-dev:amd64=2.7-1ubuntu2 \
    libboost-date-time1.71.0:amd64=1.71.0-6ubuntu6 \
    libboost-dev:amd64=1.71.0.0ubuntu2 \
    libboost-filesystem1.71.0:amd64=1.71.0-6ubuntu6 \
    libboost-iostreams1.71.0:amd64=1.71.0-6ubuntu6 \
    libboost-locale1.71.0:amd64=1.71.0-6ubuntu6 \
    libboost-thread1.71.0:amd64=1.71.0-6ubuntu6 \
    libboost1.71-dev:amd64=1.71.0-6ubuntu6 \
    libc-bin=2.31-0ubuntu9.18 \
    m4=1.4.18-4


WORKDIR /app

RUN pip3 install matplotlib==3.1.2 pandas==1.3.4 seaborn==0.11.2 pyyaml wget==3.2 scipy==1.3.3 numpy==1.17.4 scons==3.1.2


# RUN ln -sf $(which g++-10) /usr/local/bin/g++
# RUN ln -sf $(which gcc-10) /usr/local/bin/gcc

ENV CXX=g++-10
ENV CC=gcc-10

RUN ln -s /usr/bin/python3 /usr/bin/python


ENTRYPOINT [ "/bin/bash", "-l", "-c" ]