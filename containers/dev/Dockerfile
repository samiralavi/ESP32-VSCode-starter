FROM ubuntu:24.04

ARG USERNAME=ubuntu
ARG USER_UID=1000
ARG USER_GID=$USER_UID
ARG USER_HOME=/home/$USERNAME

# Create the user
# [Optional] Add sudo support. Omit if you don't need to install software after connecting.
RUN apt-get update \
    && apt-get install -y sudo \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Install dependencies
RUN DEBIAN_FRONTEND="noninteractive" apt-get -y install curl gnupg \
    build-essential bash-completion nano git ca-certificates curl \
    wget gcc-multilib build-essential git tzdata \
    unzip udev usbutils libusb-dev libncurses5-dev libncursesw5-dev \
    libpcre3 libpcre3-dev iputils-ping python3 python3-dev python3-pip \
    flex bison gperf python3-venv ninja-build ccache libffi-dev libssl-dev \
    dfu-util libusb-1.0-0 clang-tidy-14 locales npm \
    && rm -rf /var/lib/apt/lists/*

# Install inliner tool to inline web files into single html file
RUN npm install -g inliner

# Set the locale
RUN sed -i '/en_US.UTF-8/s/^# //g' /etc/locale.gen && \
    locale-gen
ENV LANG en_US.UTF-8  
ENV LANGUAGE en_US:en  
ENV LC_ALL en_US.UTF-8     

# Links for python, pip, and python debugger
RUN ln -s /usr/bin/python3 /usr/bin/python

# Downloads and installs the latest version of cmake
ENV CMAKE_VERSION=3.31.8
RUN wget https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.sh \
    && chmod +x cmake-${CMAKE_VERSION}-linux-x86_64.sh \
    && ./cmake-${CMAKE_VERSION}-linux-x86_64.sh --prefix=/usr/local --skip-license

USER $USERNAME

# Install latest version of poetry python package manager
RUN curl -sSL https://install.python-poetry.org | python3 -
ENV PATH="${USER_HOME}/.local/bin:${PATH}"

# Install ESP-IDF
RUN mkdir -p $USER_HOME/esp \
    && cd $USER_HOME/esp \
    && git clone -b release/v5.4 --depth 1 --recursive https://github.com/espressif/esp-idf.git \
    && cd $USER_HOME/esp/esp-idf \
    && ./install.sh esp32s2,esp32s3

USER root

# Add user to dialout and plugdev groups for serial port access
RUN usermod -a -G dialout,plugdev $USERNAME
