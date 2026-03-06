#include "plugin_base.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string url_encode(const std::string &value) {
  CURL *curl = curl_easy_init();
  if (curl) {
    char *output = curl_easy_escape(curl, value.c_str(), value.length());
    if (output) {
      std::string result(output);
      curl_free(output);
      curl_easy_cleanup(curl);
      return result;
    }
    curl_easy_cleanup(curl);
  }
  return "";
}

std::vector<Lawnch::Result> do_youtube_query(const std::string &term,
                                             const LawnchHostApi *host) {
  try {
    std::string display = term.empty() ? "Type to search YouTube..." : term;
    std::string encoded_term = url_encode(term);
    std::string command =
        "xdg-open \"https://www.youtube.com/results?search_query=" +
        encoded_term + "\"";

    Lawnch::Result r;
    r.name = display;
    r.comment = "Search YouTube (Enter to open)";
    r.icon = "multimedia-video-player";
    r.command = command;
    r.type = "youtube";

    if (host && host->log_api && !term.empty()) {
      std::string msg = "Executing youtube search for: " + term;
      host->log_api->log("YoutubePlugin", LAWNCH_LOG_DEBUG, msg.c_str());
    }

    return {r};

  } catch (const std::exception &e) {
    if (host && host->log_api) {
      std::string msg = std::string("Youtube search failed: ") + e.what();
      host->log_api->log("YoutubePlugin", LAWNCH_LOG_ERROR, msg.c_str());
    }
    Lawnch::Result r;
    r.name = "Youtube Error";
    r.comment = e.what();
    r.icon = "dialog-error";
    r.type = "error";
    return {r};
  }
}

} // namespace

class YoutubePlugin : public Lawnch::Plugin {
public:
  void init(const LawnchHostApi *host) override {
    Plugin::init(host);
    if (host && host->log_api) {
      host->log_api->log("YoutubePlugin", LAWNCH_LOG_INFO, "Initialized");
    }
  }

  std::vector<std::string> get_triggers() override {
    return {":youtube", ":yt"};
  }

  Lawnch::Result get_help() override {
    Lawnch::Result r;
    r.name = ":youtube / :yt";
    r.comment = "Search YouTube";
    r.icon = "multimedia-video-player";
    r.type = "help";
    return r;
  }

  std::vector<Lawnch::Result> query(const std::string &term) override {
    return do_youtube_query(term, host_);
  }
};

LAWNCH_PLUGIN_DEFINE(YoutubePlugin)
