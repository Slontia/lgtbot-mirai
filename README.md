<div align="center">

![Logo](https://github.com/Slontia/lgtbot/blob/master/logo.svg)

![image](https://img.shields.io/badge/author-slontia-blue.svg) ![image](https://img.shields.io/badge/language-c++20-green.svg)

</div>

# lgtbot-mirai

基于 Mirai 框架的 LGTBot 游戏裁判机器人。

LGTBot 相关内容，请访问 [LGTBot 主页](http://github/Slontia/lgtbot)

关于项目的构建，请参考 [LGTBot 开始](https://github.com/Slontia/lgtbot#2-%E5%BC%80%E5%A7%8B)

## 使用说明

```
  Flags from /home/bjcwgqm/projects/lgtbot/src/src/main.cpp:
    -admins (Administrator user id list) type: string default: ""
    -auth (The AuthKey for mirai-api-http) type: string default: ""
    -db_path (Path of database) type: string default: "./lgtbot_data.db"
    -game_path (The path of game modules) type: string default: "plugins"
    -image_path (The path of images cache) type: string default: "images"
    -ip (The IP address) type: string default: "127.0.0.1"
    -port (The port) type: int32 default: 8080
    -qq (Bot's QQ ID) type: uint64 default: 0
    -thread (The number of threads) type: int32 default: 4
```

- 必选项：
    - `auth`：Mirai 所需要的 AuthKey
    - `game_path`：游戏动态库文件所在文件夹路径
    - `ip`：本机 IP 地址，供 Mirai-http 访问（若 Mirai-http 部署在本机，可不填，即为 127.0.0.1）
    - `port`：端口号，默认为 8080
    - `qq`：机器人 QQ 号

- 可选项：
    - `db_path`：战绩数据存储路径
    - `admins`：管理员列表，管理员之间通过半角逗号「,」分割，例如：admins=114514,1919810
    - `image_path`：（暂不生效）
    - `thread`：接受并处理 QQ 消息的线程数

