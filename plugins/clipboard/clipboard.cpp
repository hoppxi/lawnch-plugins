#include "plugin_base.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string exec(const char *cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

bool contains_ignore_case(const std::string &str, const std::string &sub) {
  if (sub.empty())
    return true;
  auto it = std::search(str.begin(), str.end(), sub.begin(), sub.end(),
                        [](char ch1, char ch2) {
                          return std::toupper(ch1) == std::toupper(ch2);
                        });
  return (it != str.end());
}

std::vector<Lawnch::Result> do_clip_query(const std::string &term,
                                          const LawnchHostApi *host) {
  std::vector<Lawnch::Result> results;
  std::string output;
  try {
    output = exec("cliphist list");
  } catch (const std::exception &e) {
    if (host && host->log_api) {
      host->log_api->log("ClipboardPlugin", LAWNCH_LOG_ERROR, e.what());
    }
    Lawnch::Result r;
    r.name = "Clipboard unavailable";
    r.comment = e.what();
    r.icon = "dialog-error";
    r.type = "info";
    return {r};
  }

  if (output.empty()) {
    if (host && host->log_api) {
      host->log_api->log("ClipboardPlugin", LAWNCH_LOG_WARNING,
                         "cliphist returned empty output");
    }
    Lawnch::Result r;
    r.name = "Clipboard unavailable";
    r.comment = "cliphist not found or empty";
    r.icon = "dialog-error";
    r.type = "info";
    return {r};
  }

  std::stringstream ss(output);
  std::string line;

  while (std::getline(ss, line)) {
    auto tab = line.find('\t');
    if (tab == std::string::npos)
      continue;

    std::string id = line.substr(0, tab);
    std::string content = line.substr(tab + 1);

    if (!term.empty() && !contains_ignore_case(content, term))
      continue;

    if (content.size() > 100)
      content = content.substr(0, 100) + "…";

    Lawnch::Result r;
    r.name = content;
    r.comment = "Cliphist ID: " + id;
    r.icon = "edit-paste";
    r.command = "cliphist decode " + id + " | wl-copy";
    r.type = "clipboard";

    results.push_back(r);
  }

  if (results.empty() && !term.empty()) {
    Lawnch::Result r;
    r.name = "No matches";
    r.comment = "Clipboard history exists but no match";
    r.icon = "dialog-information";
    r.type = "info";
    results.push_back(r);
  }

  return results;
}

} // namespace

class ClipboardPlugin : public Lawnch::Plugin {
public:
  void init(const LawnchHostApi *host) override {
    Plugin::init(host);
    if (host && host->log_api) {
      host->log_api->log("ClipboardPlugin", LAWNCH_LOG_INFO, "Initialized");
    }
  }

  std::vector<std::string> get_triggers() override { return {":clip", ":c"}; }

  Lawnch::Result get_help() override {
    Lawnch::Result r;
    r.name = ":clip / :c";
    r.comment = "Clipboard history";
    r.icon = "edit-paste";
    r.type = "help";
    return r;
  }

  std::vector<Lawnch::Result> query(const std::string &term) override {
    auto results = do_clip_query(term, host_);
    if (host_ && host_->log_api) {
      std::string msg = "Query '" + term + "' returned " +
                        std::to_string(results.size()) + " results";
      host_->log_api->log("ClipboardPlugin", LAWNCH_LOG_DEBUG, msg.c_str());
    }
    return results;
  }

  uint32_t get_flags() const override {
    return LAWNCH_PLUGIN_FLAG_DISABLE_HISTORY;
  }
};

LAWNCH_PLUGIN_DEFINE(ClipboardPlugin)
