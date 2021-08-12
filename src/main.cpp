#include <iostream>
#include <map>
#include <mirai.h>
#include <gflags/gflags.h>
#include "myheader.h"
#include "bot_core/bot_core.h"

DEFINE_string(ip, "127.0.0.1", "The IP address");
DEFINE_int32(port, 8080, "The port");
DEFINE_string(auth, "", "The AuthKey for mirai-api-http");
DEFINE_int32(thread, 4, "The number of threads");
DEFINE_uint64(qq, 0, "Bot's QQ ID");
DEFINE_string(game_path, "plugins", "The path of game modules");
DEFINE_string(admins, "", "Administrator user id list");

DEFINE_string(db_addr, "", "Address of database <ip>:<port>");
DEFINE_string(db_user, "root", "User of database");
DEFINE_string(db_name, "lgtbot", "Name of database");
DEFINE_string(db_passwd, "", "Password of database");

static Cyan::MiraiBot* g_mirai_bot = nullptr;

struct Messager
{
    Messager(const uint64_t id, const bool is_uid) : id_(id), is_uid_(is_uid) {}
    const uint64_t id_;
    const bool is_uid_;
    Cyan::MessageChain msg_;
};

void* OpenMessager(const uint64_t id, const bool is_uid)
{
    return new Messager(id, is_uid);
}

void MessagerPostText(void* p, const char* data, uint64_t len)
{
    Messager* const messager = static_cast<Messager*>(p);
    messager->msg_.Plain(std::string_view(data, len));
}

void MessagerPostUser(void* p, uint64_t uid, bool is_at)
{
    Messager* const messager = static_cast<Messager*>(p);

    const auto post_user_id = [&](const uint64_t uid)
    {
        messager->msg_.Plain("<" + std::to_string(uid) + ">");
    };

    const auto try_post_name = [&](const uint64_t uid)
    {
        try {
            for (const auto& f : g_mirai_bot->GetFriendList()) {
                if (uid == f.QQ.ToInt64()) {
                    messager->msg_.Plain("<" + f.NickName + "(" + std::to_string(uid) + ")>");
                    return;
                }
            }
            // If the user is not friend, we will not find his name.
        } catch (std::exception& e) {
            std::cerr << "UserName failed: uid=" << uid << " info=" << e.what() << std::endl;
        }
        post_user_id(uid);
    };

    const auto try_post_member_name = [&](const uint64_t uid, const uint64_t gid)
    {
        try {
            messager->msg_.Plain("<" + g_mirai_bot->GetGroupMemberInfo(Cyan::GID_t(gid), Cyan::QQ_t(uid)).MemberName
                           + "(" + std::to_string(uid) + ")>");
            return;
        } catch (std::exception& e) {
            std::cerr << "GroupUserName failed: uid=" << uid << " gid=" << gid << " info=" << e.what() << std::endl;
        }
        try_post_name(uid);
    };

    if (is_at) {
        if (!messager->is_uid_) {
            messager->msg_.At(Cyan::QQ_t(uid));
        } else if (uid == messager->id_) {
            messager->msg_.Plain("<你>");
        } else {
            try_post_name(uid);
        }
    } else {
        if (!messager->is_uid_) {
            try_post_member_name(uid, messager->id_);
        } else {
            try_post_name(uid);
        }
    }
}

void MessagerFlush(void* p)
{
    Messager* const messager = static_cast<Messager*>(p);
    try {
        if (messager->is_uid_) {
            g_mirai_bot->SendMessage(Cyan::QQ_t(messager->id_), messager->msg_);
        } else {
            g_mirai_bot->SendMessage(Cyan::GID_t(messager->id_), messager->msg_);
        }
    } catch (std::exception& e) {
        std::cerr << "Sending message failed: is_uid=" << std::boolalpha << messager->is_uid_ << std::noboolalpha
                  << " id=" << messager->id_ << " info=" << e.what() << std::endl;
    }
    messager->msg_.Clear();
}

void CloseMessager(void* p)
{
    Messager* const messager = static_cast<Messager*>(p);
    delete messager;
}

static const std::vector<uint64_t> LoadAdmins()
{
    if (FLAGS_admins.empty()) {
        return {};
    }
    std::vector<uint64_t> admins;
    std::string::size_type begin = 0;
    while (true) {
        admins.emplace_back(atoll(FLAGS_admins.c_str() + begin));
        begin = FLAGS_admins.find_first_of(',', begin);
        if (begin == std::string::npos || ++begin == FLAGS_admins.size()) {
            break;
        }
    }
    return admins;
}

static void ConnectDatabase(void* const bot)
{
#ifdef WITH_MYSQL
    if (!FLAGS_db_addr.empty()) {
        const char* errmsg = nullptr;
        if (FLAGS_db_passwd.empty()) {
            char passwd[128] = {0};
            std::cout << "Password: ";
            std::cin >> passwd;
            BOT_API::ConnectDatabase(bot, FLAGS_db_addr.c_str(), FLAGS_db_user.c_str(), passwd, FLAGS_db_name.c_str(), &errmsg);
        } else {
            BOT_API::ConnectDatabase(bot, FLAGS_db_addr.c_str(), FLAGS_db_user.c_str(), FLAGS_db_passwd.c_str(), FLAGS_db_name.c_str(), &errmsg);
        }
        if (errmsg) {
            std::cerr << "Connect database failed errmsg:" << *errmsg << std::endl;
        } else {
            std::cout << "Connect database success" << std::endl;
        }
    }
#endif
}

int main(int argc, char** argv)
{
#if defined(WIN32) || defined(_WIN32)
    // make Windows console show UTF-8 characters
	system("chcp 65001");
#endif
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    const auto admins = LoadAdmins();
    const std::unique_ptr<void, void(*)(void*)> bot_core(
            BOT_API::Init(FLAGS_qq, FLAGS_game_path.c_str(), admins.data(), admins.size()),
            BOT_API::Release);
    if (!bot_core) {
        std::cerr << "Init bot core failed" << std::endl;
        return -1;
    }
    ConnectDatabase(bot_core.get());

    Cyan::MiraiBot bot;
    Cyan::SessionOptions opts;
    opts.BotQQ = Cyan::QQ_t(FLAGS_qq);
    opts.HttpHostname = FLAGS_ip;
    opts.WebSocketHostname = FLAGS_ip;
    opts.HttpPort = FLAGS_port;
    opts.WebSocketPort = FLAGS_port;
    opts.VerifyKey = FLAGS_auth;
    //opts.ThreadPoolSize = FLAGS_thread;

    g_mirai_bot = &bot;
    while (true) {
        try {
            bot.Connect(opts);
            break;
        } catch (const std::exception& ex) {
            std::cerr << "connect bot failed errmsg:" << ex.what() << std::endl;
        }
        Cyan::MiraiBot::SleepSeconds(1);
    }
    std::cout << "Bot Working..." << std::endl;

    bot.On<Cyan::GroupMessage>([&bot_core](Cyan::GroupMessage m)
        {
            try {
                if (m.AtMe()) {
                    BOT_API::HandlePublicRequest(bot_core.get(), m.Sender.Group.GID.ToInt64(), m.Sender.QQ.ToInt64(),
                            m.MessageChain.GetPlainText().c_str());
                }
            } catch (const std::exception& ex) {
                std::cout << ex.what() << std::endl;
            }
        });

    bot.On<Cyan::FriendMessage>([&bot_core](Cyan::FriendMessage m)
        {
            try {
                BOT_API::HandlePrivateRequest(bot_core.get(), m.Sender.QQ.ToInt64(), m.MessageChain.GetPlainText().c_str());
            } catch (const std::exception& ex) {
                std::cout << ex.what() << std::endl;
            }
        });

    bot.On<Cyan::TempMessage>([&bot_core](Cyan::TempMessage m)
        {
            try {
                m.Reply("[错误] 请先添加我为好友吧，这样我们就能一起玩耍啦~" + m.MessageChain);
            } catch (const std::exception& ex) {
                std::cout << ex.what() << std::endl;
            }
        });

    bot.OnEventReceived<Cyan::NewFriendRequestEvent>([&](Cyan::NewFriendRequestEvent newFriend)
        {
            try {
                newFriend.Accept();
            } catch (const std::exception& ex) {
                std::cout << ex.what() << std::endl;
            }
        });

	for (std::string command; std::cin >> command; ) {
		if (command == "exit") {
			bot.Disconnect();
			break;
		}
	}

    return 0;
}
