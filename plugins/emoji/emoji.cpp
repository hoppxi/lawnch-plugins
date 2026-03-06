#include "plugin_base.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

bool contains_ignore_case(const std::string &str, const std::string &sub) {
  if (sub.empty())
    return true;
  auto it = std::search(str.begin(), str.end(), sub.begin(), sub.end(),
                        [](char ch1, char ch2) {
                          return std::toupper(ch1) == std::toupper(ch2);
                        });
  return (it != str.end());
}

struct EmojiState {
  json emoji_cache;
  bool loaded = false;
};

void init_emoji(EmojiState *state, const LawnchHostApi *host) {
  if (state->loaded)
    return;
  const char *data_dir_path = host->get_data_dir(host);
  if (!data_dir_path) {
    if (host && host->log_api)
      host->log_api->log("EmojiPlugin", LAWNCH_LOG_ERROR,
                         "Host did not provide a data directory.");
    return;
  }

  fs::path emoji_path = fs::path(data_dir_path) / "emoji.json";

  std::ifstream f(emoji_path);
  if (f.good()) {
    try {
      f >> state->emoji_cache;
      state->loaded = true;
    } catch (const std::exception &e) {
      if (host && host->log_api) {
        std::string msg = std::string("Failed to parse ") +
                          emoji_path.string() + ": " + e.what();
        host->log_api->log("EmojiPlugin", LAWNCH_LOG_ERROR, msg.c_str());
      }
    }
  } else {
    if (host && host->log_api) {
      std::string msg = std::string("Failed to open ") + emoji_path.string();
      host->log_api->log("EmojiPlugin", LAWNCH_LOG_ERROR, msg.c_str());
    }
  }
}

std::vector<Lawnch::Result> do_emoji_query(const std::string &term,
                                           EmojiState *state,
                                           const LawnchHostApi *host) {
  try {
    if (!state->loaded) {
      Lawnch::Result err_result;
      err_result.name = "Error: emoji.json not loaded";
      err_result.comment = "Could not find or parse asset file";
      err_result.icon = "dialog-error";
      err_result.type = "error";
      return {err_result};
    }

    std::string term_str = term;
    std::vector<Lawnch::Result> results_vec;

    for (const auto &item : state->emoji_cache) {
      if (!item.is_object())
        continue;
      std::string text = item.value("text", "");
      std::string emoji = item.value("emoji", "");

      bool match = contains_ignore_case(text, term_str);
      if (!match && item.contains("keywords")) {
        for (const auto &keyword_item : item["keywords"]) {
          if (contains_ignore_case(keyword_item.get<std::string>(), term_str)) {
            match = true;
            break;
          }
        }
      }

      if (match) {
        Lawnch::Result r;
        r.name = emoji + "  " + text;
        r.comment = item.value("category", "");
        r.command = "echo -n '" + emoji + "' | wl-copy";
        r.type = "emoji";
        results_vec.push_back(r);

        if (results_vec.size() >= 50)
          break;
      }
    }

    if (host && host->log_api && !term.empty()) {
      std::string msg = "Query '" + term + "' returned " +
                        std::to_string(results_vec.size()) + " emojis";
      host->log_api->log("EmojiPlugin", LAWNCH_LOG_DEBUG, msg.c_str());
    }

    return results_vec;

  } catch (const std::exception &e) {
    if (host && host->log_api) {
      std::string msg = std::string("Emoji query failed: ") + e.what();
      host->log_api->log("EmojiPlugin", LAWNCH_LOG_ERROR, msg.c_str());
    }
    Lawnch::Result r;
    r.name = "Emoji Error";
    r.comment = e.what();
    r.icon = "dialog-error";
    r.type = "error";
    return {r};
  }
}

} // namespace

class EmojiPlugin : public Lawnch::Plugin {
private:
  EmojiState state_;

public:
  void init(const LawnchHostApi *host) override {
    Plugin::init(host);
    if (host && host->log_api) {
      host->log_api->log("EmojiPlugin", LAWNCH_LOG_INFO, "Initializing...");
    }
    init_emoji(&state_, host);
    if (host && host->log_api && state_.loaded) {
      host->log_api->log("EmojiPlugin", LAWNCH_LOG_INFO, "Emoji data loaded");
    }
  }

  void destroy() override {
    if (state_.loaded) {
      state_.emoji_cache.clear();
      state_.loaded = false;
    }
  }

  std::vector<std::string> get_triggers() override { return {":emoji", ":e"}; }

  Lawnch::Result get_help() override {
    Lawnch::Result r;
    r.name = ":emoji / :e";
    r.comment = "Search for emoji";
    r.icon = "face-smile";
    r.type = "help";
    return r;
  }

  std::vector<Lawnch::Result> query(const std::string &term) override {
    return do_emoji_query(term, &state_, host_);
  }
};

LAWNCH_PLUGIN_DEFINE(EmojiPlugin)
