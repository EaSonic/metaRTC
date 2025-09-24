## Debian bullseye build environment for metaRTC testRTM demo
## 如果仍需 CentOS7 版本，可保留旧文件另存为 Dockerfile.centos7

FROM debian:bullseye

ENV DEBIAN_FRONTEND=noninteractive

# 基础开发工具 + 常用依赖。若项目 thirdparty 已提供静态库，可删除对应 -dev 包。
RUN apt-get update && \
  apt-get install -y --no-install-recommends \
  build-essential cmake git pkg-config ca-certificates wget curl bash \
  libssl-dev libsrtp2-dev libopus-dev libspeexdsp-dev libusrsctp-dev yasm nasm && \
  update-ca-certificates && \
  rm -rf /var/lib/apt/lists/*

# 设置工作目录，宿主源码通过 -v 挂载到 /workspace
WORKDIR /workspace

# 可选: 指定编译类型
ENV CMAKE_BUILD_TYPE=Release

# 使用说明 (镜像内查看: cat /workspace/demo/testRTM/Dockerfile)
# 1. 构建镜像:
#    docker build -t metartc-bullseye -f demo/testRTM/Dockerfile .
# 2. 运行并挂载源码根目录:
#    docker run --rm -it -v $(pwd):/workspace metartc-bullseye bash
# 3. 编译:
#    cd demo/testRTM && mkdir -p build && cd build
#    cmake .. -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
#    make -j$(nproc)
# 4. 运行 (示例):
#    ./testRTM "https://example.com/whip?publisher=xxx" ./dump --flv

# 缓存层: 创建空 build 目录(可选)
RUN mkdir -p /workspace/demo/testRTM/build

## 若需在构建镜像阶段直接编译（可选，默认注释掉）：
# COPY . /workspace
# RUN cd demo/testRTM/build && cmake .. && make -j$(nproc)


