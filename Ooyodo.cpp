#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <unistd.h>
#include <curl/curl.h>
#include "cpp-json/json.h"

using namespace std;

static const string HIDE_KEYBOARD = "{\"hide_keyboard\":true,\"selective\":true}";
static const string BOT_TOKEN = ""; //Enter your bot token here
static const string BOT_ID = ""; //Be included in TOKEN
static bool app_quiting = false;

static void tg_send_message(const string &text, const string &chat_id, const string &keyboard, const string &reply_to = "");
static json::value fetch_quest_info();


static void tg_dispatch_message(const json::value &message) {
    clog << "ID:   " << to_string(message["message_id"]) << endl
         << "From: " << to_string(message["from"]["username"]) << endl;
    try {
        clog << "Chat: " << to_string(message["chat"]["title"]) << endl;
    } catch(json::invalid_index) {}
    try {
        clog << "Text: " << to_string(message["text"]) << endl;
    } catch(json::invalid_index) {}
    clog << endl;

    string msg_id = to_string(message["message_id"]);
    string chat_id = to_string(message["from"]["id"]);
    try {
        chat_id = to_string(message["chat"]["id"]);
    } catch(json::invalid_index) {}
    string text;
    try {
        text = to_string(message["text"]);
    } catch(json::invalid_index) {}
    string reply_to_id;
    try {
        reply_to_id = to_string(message["reply_to_message"]["from"]["id"]);
    } catch(json::invalid_index) {}

    // Handle message

    if(text.find("ping") != text.npos) {
        tg_send_message("Pong!", chat_id, HIDE_KEYBOARD, msg_id);
    }

    auto quest_pos = text.npos;
    auto tmp_pos = text.find("/");
    if(tmp_pos == 0 && text.compare(0, 6, "/quest") != 0)
        quest_pos = 1;
    tmp_pos = text.find("任务");
    if(tmp_pos != text.npos)
        quest_pos = min(quest_pos, tmp_pos + 6);
    tmp_pos = text.find("任務");
    if(tmp_pos != text.npos)
        quest_pos = min(quest_pos, tmp_pos + 6);
    tmp_pos = text.find("クエスト");
    if(tmp_pos != text.npos)
        quest_pos = min(quest_pos, tmp_pos + 12);
    tmp_pos = text.find("quest");
    if(tmp_pos != text.npos)
        quest_pos = min(quest_pos, tmp_pos + 5);
    if (reply_to_id == BOT_ID)
        quest_pos = 0;
    if(quest_pos != text.npos && quest_pos != text.length()) {
        for(;;) {
            if(quest_pos != text.length() && text[quest_pos] == ' ')
                quest_pos++;
            else if(text.compare(quest_pos, 1, "\xe2\x80\x80") == 0)
                quest_pos += 3;
            else if(text.compare(quest_pos, 1, "\xe3\x80\x80") == 0)
                quest_pos += 3;
            else
                break;
        }
        string quest_content(text, quest_pos, text.npos);

        json::value quest_info = fetch_quest_info();
        const auto reply_quest_info = [&](const json::value &quest_info) {
            string reply;
            reply += "任務：";
            reply += to_string(quest_info["wiki_id"]);
            reply += "\n名前：";
            reply += to_string(quest_info["name"]);
            reply += "\n内容：";
            reply += to_string(quest_info["detail"]);
            reply += "\n條件：";
            reply += to_string(quest_info["condition"]);
            reply += "\n獲得：燃 ";
            reply += to_string(quest_info["reward_fuel"]);
            for(auto i=to_string(quest_info["reward_fuel"]).length();i<4;i++)
                 reply += "\xe2\x80\x87";
            reply += "鋼 ";
            reply += to_string(quest_info["reward_steel"]);
            reply += "\n　　　弾 ";
            reply += to_string(quest_info["reward_bullet"]);
            for(auto i=to_string(quest_info["reward_bullet"]).length();i<4;i++)
                 reply += "\xe2\x80\x87";
            reply += "ボ ";
            reply += to_string(quest_info["reward_alum"]);
            reply += "\n　　　";
            reply += to_string(quest_info["reward_other"]);
            tg_send_message(reply, chat_id, HIDE_KEYBOARD, msg_id);
        };
        for(const auto &i : to_array(quest_info)) {
            const string &wiki_id = to_string(i["wiki_id"]);
            if(quest_content.compare(0, wiki_id.length(), wiki_id) == 0) {
                reply_quest_info(i);
                return;
            }
        }

        //Easter egg
        if(quest_content.compare(0, 2, "A2")  == 0) {
        tg_send_message("A2？不，不认识的孩子呢。",  chat_id, HIDE_KEYBOARD, msg_id);
        return ;
    }

        std::list<json::value> quest_matches;
        for(const auto &i : to_array(quest_info)) {
            const string &name = to_string(i["name"]);
            const string &condition = to_string(i["condition"]);
            if(name.find(quest_content) != name.npos
            || condition.find(quest_content) != condition.npos) {
                quest_matches.push_back(i);
            }
        }
        if(quest_matches.size() == 0) {
        } else if(quest_matches.size() == 1) {
            reply_quest_info(quest_matches.front());
            return;
        } else {
            string keyboard;
            size_t count = 0;
            keyboard += "{\"keyboard\":[";
            for(const auto &quest_info : quest_matches) {
                keyboard += "[\"";
                keyboard += to_string(quest_info["name"]);
                keyboard += "\"]";
                if(++count != quest_matches.size()) {
                    keyboard += ",";
                }
            }
            keyboard += "],\"resize_keyboard\":true,\"one_time_keyboard\":true,\"selective\":true}";
            tg_send_message("请选择你要查询的任务", chat_id, keyboard, msg_id);
            return;
        }
        if(text.compare(0, 6, "/quest") == 0) {
            tg_send_message("そのような任務（クエスト）はありません。", chat_id, HIDE_KEYBOARD, msg_id);
        }
    }
}

static json::value fetch_quest_info() {
    ifstream quest_json("quest.json");
    return json::parse(quest_json);
}

static string http_escape(string s) {
    string result;
    for(char i : s) {
        if(isalnum(i)) {
            result += i;
        } else {
            static char hexconv[17] = "0123456789abcdef";
            char buf[3];
            buf[0] = '%';
            buf[1] = hexconv[(unsigned char) i >> 4];
            buf[2] = hexconv[i & 0xf];
            result.append(buf, 3);
        }
    }
    return result;
}

static size_t tg_get_updates_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    string *response = (string *) userdata;
    response->append((const char *) ptr, size*nmemb);
    return nmemb;
}

static json::value tg_get_updates() {
    static int request_message_offset = 0;

    string addr = "https://api.telegram.org/";
    addr += BOT_TOKEN;
    addr += "/getUpdates?timeout=10&offset=";
    addr += to_string(request_message_offset);

    string response;

    auto curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, addr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, tg_get_updates_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    auto curlcode = curl_easy_perform(curl);
    if(curlcode != 0) {
        cerr << curl_easy_strerror(curlcode) << endl;
        return json::value(nullptr);
    }
    curl_easy_cleanup(curl);

    json::value response_json = json::parse(response);
    for(const auto &message : to_array(response_json["result"])) {
        int message_id = to_number(message["update_id"]);
        request_message_offset = max(request_message_offset, message_id+1);
    }

    return response_json;
}

static size_t tg_send_message_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    return nmemb;
}

static void tg_send_message(const string &text, const string &chat_id, const string &keyboard, const string &reply_to) {
    clog << "Send:  " << text << endl
         << "To:    " << chat_id << endl
         << "Reply: " << reply_to << endl
         << endl;

    string addr = "https://api.telegram.org/";
    addr += BOT_TOKEN;
    addr += "/sendMessage?chat_id=";
    addr += http_escape(chat_id);
    if(reply_to.length() != 0) {
        addr += "&reply_to_message_id=";
        addr += http_escape(reply_to);
    }
    addr += "&reply_markup=";
    addr += http_escape(keyboard);
    addr += "&text=";
    addr += http_escape(text);

    auto curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, addr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, tg_send_message_callback);
    auto curlcode = curl_easy_perform(curl);
    if(curlcode != 0) {
        cerr << curl_easy_strerror(curlcode) << endl;
    }
    curl_easy_cleanup(curl);
}

int main(int argc, char *argv[]) {
    curl_global_init(CURL_GLOBAL_ALL);
    while(!app_quiting) {
        sleep(1);
        string response;
        auto messages_json = tg_get_updates();
        for(const auto &message : to_array(messages_json["result"])) {
            tg_dispatch_message(message["message"]);
        }
    }
    curl_global_cleanup();
}
