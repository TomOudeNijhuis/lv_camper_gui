FROM debian:bookworm-slim

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Update and install required packages
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Add ARM64 architecture and install cross-development libraries
RUN dpkg --add-architecture arm64 && \
    apt-get update && apt-get install -y \
    libcurl4-openssl-dev:arm64 \
    libsdl2-dev:arm64 \
    libsdl2-image-dev:arm64 \
    libevdev-dev:arm64 \
    libdrm-dev:arm64 \
    libjson-c-dev:arm64 \
    pkg-config:arm64 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

    # Configure pkg-config for cross-compilation
ENV PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig
ENV PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
ENV PKG_CONFIG_SYSROOT_DIR=/

# Create a wrapper script for pkg-config to ensure it finds ARM64 packages
RUN echo '#!/bin/bash' > /usr/local/bin/arm64-pkg-config && \
    echo 'PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig pkg-config "$@"' >> /usr/local/bin/arm64-pkg-config && \
    chmod +x /usr/local/bin/arm64-pkg-config

# Create find-curl script to help CMake locate ARM64 curl
RUN echo '#!/bin/bash' > /usr/local/bin/find-curl && \
echo 'echo CURL_INCLUDE_DIR=/usr/include/aarch64-linux-gnu' >> /usr/local/bin/find-curl && \
echo 'echo CURL_LIBRARY=/usr/lib/aarch64-linux-gnu/libcurl.so' >> /usr/local/bin/find-curl && \
chmod +x /usr/local/bin/find-curl

# Create find-jsonc script to help CMake locate ARM64 json-c
RUN echo '#!/bin/bash' > /usr/local/bin/find-jsonc && \
echo 'echo JSONC_INCLUDE_DIRS=/usr/include/aarch64-linux-gnu/json-c' >> /usr/local/bin/find-jsonc && \
echo 'echo JSONC_LIBRARIES=/usr/lib/aarch64-linux-gnu/libjson-c.so' >> /usr/local/bin/find-jsonc && \
chmod +x /usr/local/bin/find-jsonc


# Create working directory
WORKDIR /app

# The source code will be mounted from the host
VOLUME ["/app"]

# Set the entrypoint to bash for interactive use
ENTRYPOINT ["/bin/bash"]