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

void HandleMessages(void* handler, const char* const id, const int is_uid, const LGTBot_Message* messages, const size_t size)
{
    Cyan::MessageChain msg_chain;
    for (size_t i = 0; i < size; ++i) {
        const auto& msg = messages[i];
        switch (msg.type_) {
        case LGTBOT_MSG_TEXT:
            msg_chain.Plain(msg.str_);
            break;
        case LGTBOT_MSG_USER_MENTION:
            msg_chain.At(Cyan::QQ_t(atoll(msg.str_)));
            break;
        case LGTBOT_MSG_IMAGE:
            try {
                msg_chain.Image(is_uid ? g_mirai_bot->UploadFriendImage(msg.str_) : g_mirai_bot->UploadGroupImage(msg.str_));
            } catch (std::exception& e) {
                std::cerr << "Upload image failed: is_uid=" << std::boolalpha << is_uid << std::noboolalpha
                        << " id=" << id << " info=" << e.what() << std::endl;
            }
            break;
        default:
            assert(false);
        }
    }
    try {
        const auto id_int = atoll(id);
        if (!is_uid) {
            g_mirai_bot->SendMessage(Cyan::GID_t(id_int), msg_chain);
        } else if (FLAGS_allow_private && (FLAGS_allow_temp || std::ranges::any_of(g_mirai_bot->GetFriendList(),
                    [id_int](const auto& f) { return f.QQ.ToInt64() == id_int; }))) {
            g_mirai_bot->SendMessage(Cyan::QQ_t(id_int), msg_chain);
        }
    } catch (std::exception& e) {
        std::cerr << "Sending message failed: is_uid=" << std::boolalpha << is_uid << std::noboolalpha
                  << " id=" << id << " info=" << e.what() << std::endl;
    }
}

void GetUserName(void* handler, char* const buffer, const size_t size, const char* const uid_str)
{
    try {
        const auto uid = atoll(uid_str);
        for (const auto& f : g_mirai_bot->GetFriendList()) {
            if (uid == f.QQ.ToInt64()) {
                snprintf(buffer, size, "<%s(%s)>", f.NickName.c_str(), uid_str);
            }
        }
        // If the user is not friend, we will not find his name.
    } catch (std::exception& e) {
        std::cerr << "UserName failed: uid=" << uid_str << " info=" << e.what() << std::endl;
    }
    snprintf(buffer, size, "<%s>", uid_str);
}

void GetUserNameInGroup(void* handler, char* const buffer, const size_t size, const char* const gid_str, const char* const uid_str)
{
    try {
        const auto& member_info = g_mirai_bot->GetGroupMemberInfo(Cyan::GID_t(atoll(gid_str)), Cyan::QQ_t(atoll(uid_str)));
        snprintf(buffer, size, "<%s(%s)>", member_info.MemberName.c_str() , uid_str);
    } catch (std::exception& e) {
        std::cerr << "GroupUserName failed: uid=" << uid_str << " gid=" << gid_str << " info=" << e.what() << std::endl;
        GetUserName(handler, buffer, size, uid_str);
    }
}

int DownloadUserAvatar(void* handler, const char* const uid_str, const char* const dest_filename)
{
    const std::string url = std::string("http://q1.qlogo.cn/g?b=qq&nk=") + uid_str + "&s=640"; // the url to download avatars
    CURL* const curl = curl_easy_init();
    if (!curl) {
        std::cerr << "DownloadUserAvatar curl_easy_init() failed" << std::endl;
        return false;
    }
    FILE* const fp = fopen(dest_filename, "wb");
    if (!fp) {
        std::cerr << "DownloadUserAvatar open dest file failed, path: " << dest_filename << std::endl;
        return false;
    }
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

    const LGTBot_Option option {
        .game_path_ = FLAGS_game_path.c_str(),
        .admins_ = FLAGS_admins.c_str(),
        .db_path_ = FLAGS_db_path.empty() ? nullptr : FLAGS_db_path.c_str(),
        .conf_path_ = FLAGS_conf_path.empty() ? nullptr : FLAGS_conf_path.c_str(),
        .callbacks_ = LGTBot_Callback{
            .get_user_name = GetUserName,
            .get_user_name_in_group = GetUserNameInGroup,
            .download_user_avatar = DownloadUserAvatar,
            .handle_messages = HandleMessages,
        },
    };
    const char* errmsg = nullptr;
    if (!(g_lgt_bot = LGTBot_Create(&option, &errmsg))) {
        std::cerr << "[ERROR] Init bot core failed: " << errmsg << std::endl;
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
                    LGTBot_HandlePublicRequest(g_lgt_bot, gid.c_str(), uid.c_str(),
                            m.MessageChain.GetPlainText().c_str());
                }
            } catch (const std::exception& ex) {
                std::cout << ex.what() << std::endl;
            }
        });

    const auto handle_private_message = [](auto m)
        {
            try {

                if (FLAGS_qq == m.Sender.QQ.ToInt64()) {
                    std::cerr << "Receive message from self: " << m.MessageChain.GetPlainText().c_str() << std::endl;
                    return;
                }
                const auto uid = std::to_string(m.Sender.QQ.ToInt64());
                LGTBot_HandlePrivateRequest(g_lgt_bot, uid.c_str(), m.MessageChain.GetPlainText().c_str());
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

    while (std::getchar() && !LGTBot_ReleaseIfNoProcessingGames(g_lgt_bot)) {
        std::cout << "There are processing games, please retry later or force exit by kill -9." << std::endl;
    }

    bot.Disconnect();
    std::cout << "Bye." << std::endl;

    return 0;
}
