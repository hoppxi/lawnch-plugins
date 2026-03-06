#include "plugin_base.hpp"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::string get_default_terminal() {
  const char *term = std::getenv("TERMINAL");
  if (term) {
    return term;
  }
  return "foot";
}

std::vector<Lawnch::Result> do_cmd_query(const std::string &term,
                                         const LawnchHostApi *host) {
  try {
    std::string cmd = term;
    if (cmd.empty()) {
      Lawnch::Result r;
      r.name = "Command Mode";
      r.comment = "Type a shell command to run";
      r.icon = "utilities-terminal";
      r.type = "cmd";
      return {r};
    }

    for (char &c : cmd) {
      if (std::iscntrl(static_cast<unsigned char>(c)))
        c = ' ';
    }

    static const std::string terminal = get_default_terminal();
    std::string full_command = terminal + " -e " + cmd;

    Lawnch::Result r;
    r.name = cmd;
    r.comment = "Run in terminal";
    r.icon = "utilities-terminal";
    r.command = full_command;
    r.type = "cmd";

    if (host && host->log_api && !cmd.empty()) {
      std::string msg = "Executing command: " + cmd;
      host->log_api->log("CommandPlugin", LAWNCH_LOG_DEBUG, msg.c_str());
    }

    return {r};
  } catch (const std::exception &e) {
    if (host && host->log_api) {
      std::string msg = std::string("Command preparation failed: ") + e.what();
      host->log_api->log("CommandPlugin", LAWNCH_LOG_ERROR, msg.c_str());
    }
    Lawnch::Result r;
    r.name = "Error preparing command";
    r.comment = e.what();
    r.icon = "dialog-error";
    r.type = "cmd";
    return {r};
  }
}

} // namespace

class CommandPlugin : public Lawnch::Plugin {
public:
  void init(const LawnchHostApi *host) override {
    Plugin::init(host);
    if (host && host->log_api) {
      host->log_api->log("CommandPlugin", LAWNCH_LOG_INFO, "Initialized");
    }
  }

  std::vector<std::string> get_triggers() override { return {":cmd", ">"}; }

  Lawnch::Result get_help() override {
    Lawnch::Result r;
    r.name = ":cmd / >";
    r.comment = "Run a shell command";
    r.icon = "utilities-terminal";
    r.type = "help";
    return r;
  }

  std::vector<Lawnch::Result> query(const std::string &term) override {
    return do_cmd_query(term, host_);
  }
};

LAWNCH_PLUGIN_DEFINE(CommandPlugin)
