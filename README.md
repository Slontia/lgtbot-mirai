# lgtbot-mirai

基于 Mirai 框架的 LGTBot 游戏裁判机器人。

LGTBot 相关内容，请访问 [LGTBot 主页](http://github/slontia/lgtbot)

## 使用说明

```
  Flags from /home/bjcwgqm/projects/lgtbot/src/src/main.cpp:
    -admins (Administrator user id list) type: string default: ""
    -auth (The AuthKey for mirai-api-http) type: string default: ""
    -db_addr (Address of database <ip>:<port>) type: string default: ""
    -db_name (Name of database) type: string default: "lgtbot"
    -db_passwd (Password of database) type: string default: ""
    -db_user (User of database) type: string default: "root"
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
    - `admins`：管理员列表，管理员之间通过半角逗号「,」分割，例如：admins=114514,1919810
    - `image_path`：（暂不生效）
    - `thread`：接受并处理 QQ 消息的线程数

- 数据库相关（如不需要存储用户数据，则可不填）：
    - `db_addr`：数据库地址
    - `db_name`：数据库名称
    - `db_passwd`：数据库密码
    - `db_user`：数据库用户
