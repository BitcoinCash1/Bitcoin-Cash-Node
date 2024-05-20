FROM debian:buster

# disable installation of suggested and recommended packages
RUN echo 'APT::Install-Suggests "false";' >> /etc/apt/apt.conf && \
  echo 'APT::Install-Recommends "false";' >> /etc/apt/apt.conf && \
  # initial package manager config and requirements, silence apt interactive warnings
  echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections && \
  export DEBIAN_FRONTEND=noninteractive && \
  export APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE=true && \
  apt-get -y update && \
  apt-get -y install \
    apt-utils \
    && \
  apt-get upgrade -y && \
  # minimal components to allow us to add repos and keys
  apt-get -y install \
    ca-certificates \
    gnupg \
    wget \
    && \
  # Add LLVM repos and key (for clang-11)
  echo "deb http://apt.llvm.org/buster/ llvm-toolchain-buster-11 main" >> /etc/apt/sources.list && \
  echo "deb-src http://apt.llvm.org/buster/ llvm-toolchain-buster-11 main" >> /etc/apt/sources.list && \
  wget -O /etc/llvm-snapshot.gpg.key https://apt.llvm.org/llvm-snapshot.gpg.key 2>&1 && \
  apt-key add /etc/llvm-snapshot.gpg.key && \
  # install required packages
  apt-get -y update && \
  apt-get upgrade -y && \
  apt-get -y install \
    # system and build tools
    apt-utils \
    autoconf \
    automake \
    bash \
    curl \
    gperf \
    libtool \
    locales \
    pkg-config \
    unzip \
    wget \
    # BCHN build requirements
    bison \
    bsdmainutils \
    build-essential \
    ccache \
    cmake \
    libboost-chrono-dev \
    libboost-filesystem-dev \
    libboost-system-dev \
    libboost-test-dev \
    libboost-thread-dev \
    libdb++-dev \
    libdb-dev \
    libevent-dev \
    libminiupnpc-dev \
    libprotobuf-dev \
    libqrencode-dev \
    libqt5core5a \
    libqt5dbus5 \
    libqt5gui5 \
    libssl-dev \
    libzmq3-dev \
    protobuf-compiler \
    python3 \
    python3-zmq \
    qttools5-dev \
    qttools5-dev-tools \
    # Support windows build
    g++-mingw-w64-x86-64 \
    # Support ARM build
    g++-arm-linux-gnueabihf \
    gcc-arm-linux-gnueabihf \
    # Support AArch64 build
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    qemu-user-static \
    # Support OSX build
    python3-setuptools \
    # Support clang build
    clang-11 \
    # Add tools for static checking & Gitlab CI processing of results
    arcanist \
    clang-format-11 \
    eatmydata \
    git \
    nodejs \
    npm \
    python3-dev \
    python3-pip \
    python3-scipy \
    php-codesniffer \
    shellcheck \
    xmlstarlet \
    && \
  # Clean up cache
  apt-get clean && \
  # Make sure UTF-8 isn't borked
  export LANG=en_US.UTF-8 && \
  export LANGUAGE=en_US:en && \
  export LC_ALL=en_US.UTF-8 && \
  echo "en_US UTF-8" > /etc/locale.gen && \
  # Add de_DE.UTF-8 for specific JSON number formatting unit tests
  echo "de_DE.UTF-8 UTF-8" >> /etc/locale.gen && \
  # Generate all locales
  locale-gen && \
  # Fetch ninja >= 1.10 to get the restat tool
  wget https://github.com/ninja-build/ninja/releases/download/v1.10.0/ninja-linux.zip 2>&1 && \
  unzip ninja-linux.zip && \
  cp ./ninja /usr/local/bin/ninja && \
  cp ./ninja /usr/bin/ninja && \
  ninja -t restat && \
  # Support windows build
  update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix && \
  update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix && \
  # Add tools for static checking & Gitlab CI processing of results
  npm install -g npm@latest && \
  npm install -g markdownlint-cli && \
  # Linter dependencies
  pip3 install --no-cache-dir \
    flake8 \
    mypy \
    yamllint
