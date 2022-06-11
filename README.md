<div align="center">

![Logo](https://github.com/Slontia/lgtbot/blob/master/images/gradient_logo_text.svg)

![image](https://img.shields.io/badge/author-slontia-blue.svg) ![image](https://img.shields.io/badge/language-c++20-green.svg)

</div>

## 1 项目简介

[LGTBot](https://github.com/Slontia/lgtbot) 是一个基于 C++ 实现的，用于在 **聊天室** 或 **其它通讯软件** 中，实现多人 **文字推理游戏** 的裁判机器人库。

- 提供了通用的 **聊天信息交互接口**，需要由已实现好的机器人框架调用使用
- 支持 **群组公开游戏** 和 **私密游戏** 两种方式，无需在群组内也可以进行游戏
- 内置了 7 种已实现好的游戏，同时提供了通用的**游戏框架**，便于后续开发新游戏
- 基于 CMake 编译，支持跨平台

其中，「LGT」源自日本漫画家甲斐谷忍创作的《Liar Game》中的虚构组织「**L**iar **G**ame **T**ournament事务所」。

本项目是一个面向开发者的机器人库，需要和已有的机器人框架进行结合，如您想直接在通讯工具中使用该机器人库，可以参考以下已结合好的解决方案：

而 [lgtbot-mirai](https://github.com/Slontia/lgtbot-mirai) 基于 [mirai-cpp](https://github.com/cyanray/mirai-cpp) 框架，将 LGTBot 库适配到了 Mirai，通过 [mirai-api-http](https://github.com/project-mirai/mirai-api-http) 实现与机器人的交互。

**欢迎入群体验（QQ 群）：1059834024、541402580**

## 2 我要通过 Docker 直接启动 lgtbot-mirai

请参考以下任意文档：

- [GitHub wiki——使用 Docker 启动 lgtbot-mirai](https://github.com/Slontia/lgtbot-mirai/wiki/%E4%BD%BF%E7%94%A8-Docker-%E5%90%AF%E5%8A%A8-lgtbot-mirai)
- [知乎——使用 Docker 启动 lgtbot-mirai](https://zhuanlan.zhihu.com/p/527454141)

## 3 我要手动编译 lgtbot-mirai

### 3.1 编译

请确保您的编译器支持 **C++20** 语法，建议使用 g++10 以上版本

以 Centos 为例：

``` bash
# 安装依赖库（Ubuntu 系统）
sudo apt-get install -y libgoogle-glog-dev libgflags-dev libgtest-dev libsqlite3-dev libqt5webkit5-dev

# 完整克隆本项目
git clone github.com/slontia/lgtbot-mirai

# 安装子模块
git submodule update --init --recursive

# 构建二进制路径
mkdir build

# 编译项目
cmake .. -DWITH_GCOV=OFF -DWITH_ASAN=OFF -DWITH_GLOG=OFF -DWITH_SQLITE=ON -DWITH_TEST=OFF -DWITH_SIMULATOR=ON -DWITH_GAMES=ON
make

```

### 3.2 启动

**先启动 mirai-api-http 服务，再执行编译出来的 `lgtbot-mirai`**。其中涉及到的一些参数为：

```
  Flags from /home/bjcwgqm/projects/lgtbot/src/src/main.cpp:
    -admins (Administrator user id list) type: string default: ""
    -allow_temp (Allow temp message) type: bool default: true
    -auth (The AuthKey for mirai-api-http) type: string default: ""
    -db_path (Path of database) type: string default: "./lgtbot_data.db"
    -game_path (The path of game modules) type: string default: "plugins"
    -image_path (The path of images cache) type: string default: "images"
    -ip (The IP address) type: string default: "127.0.0.1"
    -port (The port) type: int32 default: 8080
    -qq (Bot's QQ ID) type: uint64 default: 0
    -thread (The number of threads) type: int32 default: 4
```


必选项：

- auth：mirai-api-http 所需要的 AuthKey
- game_path：游戏动态库文件所在文件夹路径
- ip：本机 IP 地址，供 mirai-api-http 访问（若 mirai-api-http 部署在本机，可不填，即为 127.0.0.1）
- port：端口号，默认为 8080
- qq：机器人 QQ 账号

可选项：

- allow_temp：是否允许临时会话，1 是允许，0 是不允许（考虑到允许有可能会被封号，最好是不允许）
- admins：管理员列表，管理员之间通过半角逗号「,」分割，例如：admins=114514,1919810
- db_path：战绩数据存储路径
- image_path：（暂不生效）
- thread：接受并处理 QQ 消息的线程数
