#include <iostream>
#include <map>
#include <mirai.h>
#include <gflags/gflags.h>
#include "myheader.h"
#include "bot_core/bot_core.h"
#include <curl/curl.h>
#include <csignal>
#include <condition_variable>

#if defined(LINUX) || defined(__linux__)
#include <unistd.h>
#include <sys/wait.h>
#endif

DEFINE_string(ip, "127.0.0.1", "The IP address");
DEFINE_int32(port, 8080, "The port");
DEFINE_string(auth, "", "The AuthKey for mirai-api-http");
DEFINE_int32(thread, 4, "The number of threads");
DEFINE_uint64(qq, 0, "Bot's QQ ID");
DEFINE_string(game_path, "plugins", "The path of game modules");
DEFINE_string(image_path, "images", "The path of images cache");
DEFINE_string(admins, "", "The administrator user id list");
DEFINE_string(db_path, "./lgtbot_data.db", "The path of sqlite database file");
DEFINE_bool(allow_temp, true, "Allow temp message");
DEFINE_bool(allow_private, true, "Allow private message");
DEFINE_string(conf_path, "./lgtbot_config.json", "The path of configuration file");
DEFINE_bool(auto_accept_friend, true, "Accept friend request automatically");

#if defined(LINUX)|| defined(__linux__)
DEFINE_bool(guard, false, "Create a guard process to keep alive");
#endif

static Cyan::MiraiBot* g_mirai_bot = nullptr;
static void* g_lgt_bot = nullptr;

struct Messager
{
    Messager(const uint64_t id, const bool is_uid) : id_(id), is_uid_(is_uid) {}
    const uint64_t id_;
    const bool is_uid_;
    Cyan::MessageChain msg_;
};

void* OpenMessager(const char* const id_str, const bool is_uid)
{
    return new Messager(atoll(id_str), is_uid);
}

void MessagerPostText(void* p, const char* data, uint64_t len)
{
    Messager* const messager = static_cast<Messager*>(p);
    messager->msg_.Plain(std::string_view(data, len));
}

std::string user_id_str(const uint64_t uid)
{
    return "<" + std::to_string(uid) + ">";
};

std::string user_name_str(const uint64_t uid)
{
    try {
        for (const auto& f : g_mirai_bot->GetFriendList()) {
            if (uid == f.QQ.ToInt64()) {
                return "<" + f.NickName + "(" + std::to_string(uid) + ")>";
            }
        }
        // If the user is not friend, we will not find his name.
    } catch (std::exception& e) {
        std::cerr << "UserName failed: uid=" << uid << " info=" << e.what() << std::endl;
    }
    return user_id_str(uid);
};

std::string member_name_str(const uint64_t uid, const uint64_t gid)
{
    try {
        return "<" + g_mirai_bot->GetGroupMemberInfo(Cyan::GID_t(gid), Cyan::QQ_t(uid)).MemberName
                   + "(" + std::to_string(uid) + ")>";
    } catch (std::exception& e) {
        std::cerr << "GroupUserName failed: uid=" << uid << " gid=" << gid << " info=" << e.what() << std::endl;
    }
    return user_name_str(uid);
};

const char* GetUserName(const char* const uid_str, const char* const gid_str)
{
    thread_local static std::string str;
    const uint64_t uid = atoll(uid_str);
    if (gid_str != nullptr) {
        const uint64_t gid = atoll(gid_str);
        str = member_name_str(uid, gid);
    } else {
        str = user_name_str(uid);
    }
    return str.c_str();
}

bool DownloadUserAvatar(const char* const uid_str, const std::filesystem::path::value_type* const dest_filename)
{
    const std::string url = std::string("http://q1.qlogo.cn/g?b=qq&nk=") + uid_str + "&s=640"; // the url to download avatars
    CURL* const curl = curl_easy_init();
    if (!curl) {
        std::cerr << "DownloadUserAvatar curl_easy_init() failed" << std::endl;
        return false;
    }
#ifdef _WIN32
    FILE* const fp = _wfopen(dest_filename, "wb");
#else
    FILE* const fp = fopen(dest_filename, "wb");
#endif
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    const CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "DownloadUserAvatar curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    }
    curl_easy_cleanup(curl);
    fclose(fp);
    return res == CURLE_OK;
}

void MessagerPostUser(void* const p, const char* const uid_str, const bool is_at)
{
    const uint64_t uid = atoll(uid_str);
    Messager* const messager = static_cast<Messager*>(p);
    if (is_at) {
        if (!messager->is_uid_) {
            messager->msg_.At(Cyan::QQ_t(uid));
        } else if (uid == messager->id_) {
            messager->msg_.Plain("<你>");
        } else {
            messager->msg_.Plain(user_name_str(uid));
        }
    } else {
        if (!messager->is_uid_) {
            messager->msg_.Plain(member_name_str(uid, messager->id_));
        } else {
            messager->msg_.Plain(user_name_str(uid));
        }
    }
}

void MessagerPostImage(void* p, const std::filesystem::path::value_type* path)
{
    Messager* const messager = static_cast<Messager*>(p);
    std::basic_string<std::filesystem::path::value_type> path_str(path);
    try {
        if (messager->is_uid_) {
            auto img = g_mirai_bot->UploadFriendImage(std::string(path_str.begin(), path_str.end()));
            messager->msg_.Image(img);
        } else {
            auto img = g_mirai_bot->UploadGroupImage(std::string(path_str.begin(), path_str.end()));
            messager->msg_.Image(img);
        }
    } catch (std::exception& e) {
        std::cerr << "Upload image failed: is_uid=" << std::boolalpha << messager->is_uid_ << std::noboolalpha
                  << " id=" << messager->id_ << " info=" << e.what() << std::endl;
    }
}

void MessagerFlush(void* p)
{
    Messager* const messager = static_cast<Messager*>(p);
    try {
        if (!messager->is_uid_) {
            g_mirai_bot->SendMessage(Cyan::GID_t(messager->id_), messager->msg_);
        } else if (FLAGS_allow_private && (FLAGS_allow_temp || std::ranges::any_of(g_mirai_bot->GetFriendList(),
                    [id = messager->id_](const auto& f) { return f.QQ.ToInt64() == id; }))) {
            g_mirai_bot->SendMessage(Cyan::QQ_t(messager->id_), messager->msg_);
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

void GuardProcessIfNeed()
{
#if defined(LINUX)|| defined(__linux__)
    if(!FLAGS_guard) {
        return;
    }
    while (true) {
        std::cout << "[Guard] Creating guard process..." << std::endl;
        const pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "[Guard] Failed to fork process" << std::endl;
            exit(1);
        } else if (pid == 0) {
            // Child process - execute the bot logic
            break;
        } else {
            // Parent process - wait for the child process to terminate
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                std::cout << "[Guard] Bot exited with status " << WEXITSTATUS(status) << std::endl;
                exit(0);
            } else {
                std::cerr << "[Guard] Bot terminated unexpectedly, restarting in 3 seconds" << std::endl;
                sleep(3);
            }
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

    GuardProcessIfNeed();

    const std::filesystem::path db_path = std::filesystem::path(FLAGS_db_path);
    const std::filesystem::path conf_path = std::filesystem::path(FLAGS_conf_path);
    const std::string qq_str = std::to_string(FLAGS_qq);
    const BotOption option {
        .this_uid_ = qq_str.c_str(),
        .game_path_ = FLAGS_game_path.c_str(),
        .image_path_ = FLAGS_image_path.c_str(),
        .admins_ = FLAGS_admins.c_str(),
        .db_path_ = db_path.empty() ? nullptr : db_path.c_str(),
        .conf_path_ = conf_path.empty() ? nullptr : conf_path.c_str(),
    };
    if (!(g_lgt_bot = BOT_API::Init(&option))) {
        std::cerr << "Init bot core failed" << std::endl;
        return -1;
    }

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
            std::cerr << "Connect bot failed errmsg:" << ex.what() << std::endl;
        }
        Cyan::MiraiBot::SleepSeconds(1);
    }

    bot.On<Cyan::GroupMessage>([](Cyan::GroupMessage m)
        {
            try {
                if (m.AtMe()) {
                    const auto uid = std::to_string(m.Sender.QQ.ToInt64());
                    const auto gid = std::to_string(m.Sender.Group.GID.ToInt64());
                    BOT_API::HandlePublicRequest(g_lgt_bot, gid.c_str(), uid.c_str(),
                            m.MessageChain.GetPlainText().c_str());
                }
            } catch (const std::exception& ex) {
                std::cout << ex.what() << std::endl;
            }
        });

    const auto handle_private_message = [](auto m)
        {
            try {
                const auto uid = std::to_string(m.Sender.QQ.ToInt64());
                BOT_API::HandlePrivateRequest(g_lgt_bot, uid.c_str(), m.MessageChain.GetPlainText().c_str());
            } catch (const std::exception& ex) {
                std::cout << ex.what() << std::endl;
            }
        };

    bot.On<Cyan::FriendMessage>(handle_private_message);
    bot.On<Cyan::StrangerMessage>(handle_private_message);

    if (FLAGS_allow_temp) {
        bot.On<Cyan::TempMessage>([](Cyan::TempMessage m)
            {
                try {
                    auto msg = Cyan::MessageChain();
                    msg.Plain("[错误] 请先添加我为好友吧，这样我们就能一起玩耍啦~");
                    m.Reply(msg);
                } catch (const std::exception& ex) {
                    std::cout << ex.what() << std::endl;
                }
            });
    }

    if (FLAGS_auto_accept_friend) {
        bot.OnEventReceived<Cyan::NewFriendRequestEvent>([&](Cyan::NewFriendRequestEvent newFriend)
            {
                try {
                    newFriend.Accept();
                } catch (const std::exception& ex) {
                    std::cout << ex.what() << std::endl;
                }
            });
    }

    std::cout << "Bot Working... Press <Enter> to shutdown." << std::endl;

    while (std::getchar() && !BOT_API::ReleaseIfNoProcessingGames(g_lgt_bot)) {
        std::cout << "There are processing games, please retry later or force exit by kill -9." << std::endl;
    }

    bot.Disconnect();
    std::cout << "Bye." << std::endl;

    return 0;
}
