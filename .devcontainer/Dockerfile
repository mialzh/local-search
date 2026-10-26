# syntax=docker/dockerfile:1
ARG BASE_IMAGE=ghcr.io/userver-framework/ubuntu-22.04-userver:latest
FROM ${BASE_IMAGE}

USER root
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
       build-essential cmake ninja-build git ca-certificates pkg-config \
       libicu-dev nlohmann-json3-dev gdb curl \
    && rm -rf /var/lib/apt/lists/*

ARG BUILD_JOBS=2
COPY dependency-build /opt/local-search-dev/dependency-build
COPY check /opt/local-search-dev/check
COPY scripts/install-dependencies.sh /opt/local-search-dev/install-dependencies.sh
COPY scripts/check-env.sh /usr/local/bin/local-search-check-env
RUN sed -i 's/\r$//' /opt/local-search-dev/install-dependencies.sh \
       /usr/local/bin/local-search-check-env \
    && chmod +x /opt/local-search-dev/install-dependencies.sh \
       /usr/local/bin/local-search-check-env \
    && BUILD_JOBS="${BUILD_JOBS}" /opt/local-search-dev/install-dependencies.sh \
    && local-search-check-env

WORKDIR /work
EXPOSE 8080
ENTRYPOINT []
CMD ["sleep", "infinity"]
