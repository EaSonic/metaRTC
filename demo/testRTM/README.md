# testRTM (CentOS7 minimal pull demo)

功能:

1. minisdp方式拉流 (WHIP/WHEP URL)
2. 打印RTP包 (已在核心库通过 enableRtpDump 实现)
3. 音/视频组帧写文件: video.h264 (AnnexB), audio.opus (每帧2字节长度+数据)
4. 可选输出 FLV: out.flv (--flv)
5. 兼容 glibc 2.17, 提供 CentOS7 Dockerfile 构建

## 目录结构
```text
video.h264
audio.opus
out.flv (可选)
```

## 使用
```bash
./testRTM <webrtc_url> <out_dir> [--flv]
```
示例:
```bash
./testRTM "https://example.com/whip?publisher=xxx" ./dump --flv
```

## RTP包打印
在创建连接前设置: context.rtc.enableRtpDump=yangtrue; 代码里已默认打开。

## FLV
当前仅写入 H264 视频 Tag (若需更完整的 AVCSequenceHeader 可后续补充)。

## Docker (CentOS7)
见同目录 Dockerfile，执行：
```bash
# 构建镜像
docker build -t metartc-centos7 .
# 运行交互
docker run --rm -it -v $PWD/../../..:/workspace metartc-centos7 bash
# 进入容器后编译
cd /workspace/demo/testRTM/build && cmake .. && make -j4
```

## 待改进
 - 写 FLV 时自动生成 AVCDecoderConfigurationRecord (SPS/PPS) Tag
 - 增加简单命令行选项: 日志级别、只拉视频/音频
 - 增加超时与退出条件
