#include "plugin_base.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct PowerOption {
  std::string name;
  std::string comment;
  std::string icon;
  std::string command;
};

struct PowermenuState {
  std::map<std::string, PowerOption> options;
  std::vector<std::string> option_order;
};

const char *get_config_or_null(const LawnchHostApi *host, const char *key) {
  if (!host || !host->get_config_value)
    return nullptr;
  return host->get_config_value(host, key);
}

void load_defaults(PowermenuState *state) {
  state->options.clear();
  state->option_order.clear();

  // Default options and order
  state->options["shutdown"] = {"Shutdown", "Power off the computer",
                                "system-shutdown-symbolic", "shutdown -h now"};
  state->options["reboot"] = {"Reboot", "Restart the computer",
                              "system-reboot-symbolic", "reboot"};
  state->options["suspend"] = {"Suspend", "Suspend the computer",
                               "system-suspend-symbolic", "systemctl suspend"};
  state->options["hibernate"] = {"Hibernate", "Save session to disk",
                                 "system-hibernate-symbolic",
                                 "systemctl hibernate"};
  state->options["log-out"] = {"Log Out", "Log out of your session",
                               "system-log-out-symbolic",
                               "loginctl terminate-session ${XDG_SESSION_ID-}"};
  state->options["lockscreen"] = {"Lockscreen", "Lock the screen",
                                  "lock-symbolic",
                                  "pidof hyprlock || hyprlock"};

  state->option_order = {"shutdown",  "reboot",  "suspend",
                         "hibernate", "log-out", "lockscreen"};
}

void init_powermenu(PowermenuState *state, const LawnchHostApi *host) {
  load_defaults(state);

  // Override order if specified in config
  const char *order_str = get_config_or_null(host, "order");
  if (order_str) {
    state->option_order.clear();
    std::stringstream ss(order_str);
    std::string item;
    while (std::getline(ss, item, ',')) {
      // trim whitespace
      item.erase(0, item.find_first_not_of(" \t\n\r"));
      item.erase(item.find_last_not_of(" \t\n\r") + 1);
      if (!item.empty()) {
        state->option_order.push_back(item);
      }
    }
  }

  // For each option (either default or from config order), override
  // properties from config
  for (const auto &option_key : state->option_order) {
    if (state->options.find(option_key) == state->options.end()) {
      state->options[option_key] = {option_key, "", "", ""};
      if (host && host->log_api) {
        std::string msg = "Custom option '" + option_key +
                          "' has no defaults, may be improper";
        host->log_api->log("PowermenuPlugin", LAWNCH_LOG_WARNING, msg.c_str());
      }
    }

    PowerOption &option = state->options[option_key];

    const char *name_val =
        get_config_or_null(host, (option_key + "_name").c_str());
    if (name_val)
      option.name = name_val;

    const char *comment_val =
        get_config_or_null(host, (option_key + "_comment").c_str());
    if (comment_val)
      option.comment = comment_val;

    const char *icon_val =
        get_config_or_null(host, (option_key + "_icon").c_str());
    if (icon_val)
      option.icon = icon_val;

    const char *command_val =
        get_config_or_null(host, (option_key + "_command").c_str());
    if (command_val)
      option.command = command_val;
  }
}

std::vector<Lawnch::Result> do_power_query(const std::string &term,
                                           PowermenuState *state,
                                           const LawnchHostApi *host) {
  try {
    std::string lower_term = term;
    std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    std::vector<Lawnch::Result> results;

    for (const auto &option_name : state->option_order) {
      const auto &option = state->options.at(option_name);

      bool match = false;
      if (lower_term.empty()) {
        match = true;
      } else {
        std::string name = option.name;
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (name.find(lower_term) != std::string::npos) {
          match = true;
        }
      }

      if (match) {
        Lawnch::Result r;
        r.name = option.name;
        r.comment = option.comment;
        r.icon = option.icon;
        r.command = option.command;
        r.type = "plugin";
        results.push_back(r);
      }
    }

    if (host && host->log_api && !term.empty()) {
      std::string msg = "Query '" + term + "' returned " +
                        std::to_string(results.size()) + " power options";
      host->log_api->log("PowermenuPlugin", LAWNCH_LOG_DEBUG, msg.c_str());
    }

    return results;

  } catch (const std::exception &e) {
    if (host && host->log_api) {
      std::string msg = std::string("Powermenu query failed: ") + e.what();
      host->log_api->log("PowermenuPlugin", LAWNCH_LOG_ERROR, msg.c_str());
    }
    Lawnch::Result r;
    r.name = "Powermenu Error";
    r.comment = e.what();
    r.icon = "dialog-error";
    r.type = "error";
    return {r};
  }
}

} // namespace

class PowermenuPlugin : public Lawnch::Plugin {
private:
  PowermenuState state_;

public:
  void init(const LawnchHostApi *host) override {
    Plugin::init(host);
    init_powermenu(&state_, host);
    if (host && host->log_api) {
      std::string msg = "Loaded " + std::to_string(state_.option_order.size()) +
                        " power options";
      host->log_api->log("PowermenuPlugin", LAWNCH_LOG_INFO, msg.c_str());
    }
  }

  void destroy() override {
    state_.options.clear();
    state_.option_order.clear();
  }

  std::vector<std::string> get_triggers() override { return {":power", ":p"}; }

  Lawnch::Result get_help() override {
    Lawnch::Result r;
    r.name = ":power / :p";
    r.comment = "Power options (shutdown, reboot, etc.)";
    r.icon = "system-shutdown-symbolic";
    r.type = "help";
    return r;
  }

  std::vector<Lawnch::Result> query(const std::string &term) override {
    return do_power_query(term, &state_, host_);
  }

  uint32_t get_flags() const override {
    return LAWNCH_PLUGIN_FLAG_DISABLE_SORT;
  }
};

LAWNCH_PLUGIN_DEFINE(PowermenuPlugin)
