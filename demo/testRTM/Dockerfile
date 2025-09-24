## Debian bullseye build environment for metaRTC testRTM demo
## 如果仍需 CentOS7 版本，可保留旧文件另存为 Dockerfile.centos7

FROM debian:bullseye

ENV DEBIAN_FRONTEND=noninteractive

# 基础开发工具 + 常用依赖。若项目 thirdparty 已提供静态库，可删除对应 -dev 包。
RUN apt-get update && \
  apt-get install -y --no-install-recommends \
  build-essential cmake git pkg-config ca-certificates wget curl bash \
  libssl-dev libsrtp2-dev libopus-dev libspeexdsp-dev libusrsctp-dev libasound2-dev \
  yasm nasm && \
  update-ca-certificates && \
  rm -rf /var/lib/apt/lists/*

ARG BRANCH=minisdp
ARG CACHEBUST=1   # 新增
RUN echo "cachebust=$CACHEBUST" && \
  git clone --depth 1 --branch ${BRANCH} https://github.com/EaSonic/metaRTC.git ${META_ROOT}

ENV CMAKE_BUILD_TYPE=Release \
  META_ROOT=/opt/metaRTC

# 克隆指定分支源码
RUN git clone --depth 1 --branch ${BRANCH} https://github.com/EaSonic/metaRTC.git ${META_ROOT}

WORKDIR ${META_ROOT}

# 构建核心库: metartccore7 / metartc7 / yangwhip7
RUN set -eux; \
  for d in libmetartccore7 libmetartc7 libyangwhip7; do \
  cd ${META_ROOT}/$d; \
  rm -rf build && mkdir build && cd build; \
  cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ..; \
  make -j"$(nproc)"; \
  done; \
  mkdir -p ${META_ROOT}/bin/lib_release; \
  cp ${META_ROOT}/libmetartccore7/build/libmetartccore7.a ${META_ROOT}/bin/lib_release/ || true; \
  cp ${META_ROOT}/libmetartc7/build/libmetartc7.a ${META_ROOT}/bin/lib_release/ || true; \
  cp ${META_ROOT}/libyangwhip7/build/libyangwhip7.a ${META_ROOT}/bin/lib_release/ || true

# 构建 demo/testRTM
RUN set -eux; \
  cd ${META_ROOT}/demo/testRTM; \
  mkdir -p build && cd build; \
  cmake -DMETARTC_LIB_DIR=${META_ROOT}/bin/lib_release -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ..; \
  make -j"$(nproc)"; \
  ls -lh ./testRTM

# 运行提示
CMD ["/opt/metaRTC/demo/testRTM/build/testRTM", "--help"]


