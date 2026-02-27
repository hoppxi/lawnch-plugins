#include "plugin_base.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool is_image_ext(const std::string &ext) {
  std::string e = ext;
  std::transform(e.begin(), e.end(), e.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".webp" ||
         e == ".bmp";
}

std::string build_command(const std::string &tpl, const std::string &path) {
  std::string cmd = tpl;
  size_t pos = cmd.find("{}");
  if (pos != std::string::npos) {
    cmd.replace(pos, 2, path);
  }
  return cmd;
}

struct WallpapersState {
  std::string wallpaper_dir;
  std::string command_template;
};

std::string to_lower(std::string s, const LawnchHostApi *host) {
  if (host && host->str_api && host->str_api->to_lower_copy) {
    char *res = host->str_api->to_lower_copy(s.c_str());
    if (res) {
      std::string ret(res);
      if (host->str_api->free_str)
        host->str_api->free_str(res);
      else
        free(res);
      return ret;
    }
  }
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::string expand_home(const std::string &path, const LawnchHostApi *host) {
  if (host && host->fs_api && host->fs_api->expand_tilde) {
    char *res = host->fs_api->expand_tilde(path.c_str());
    if (res) {
      std::string s(res);
      if (host->fs_api->free_path)
        host->fs_api->free_path(res);
      else
        free(res);
      return s;
    }
  }
  if (!path.empty() && path[0] == '~') {
    const char *home = std::getenv("HOME");
    if (home)
      return std::string(home) + path.substr(1);
  }
  return path;
}

void init_wallpapers(WallpapersState *state, const LawnchHostApi *host) {
  const char *dir = host->get_config_value(host, "dir");
  if (dir && *dir) {
    state->wallpaper_dir = expand_home(dir, host);
  } else {
    state->wallpaper_dir = expand_home("~/Pictures/Wallpapers", host);
  }

  const char *cmd = host->get_config_value(host, "command");
  if (cmd && *cmd) {
    state->command_template = cmd;
  } else {
    state->command_template = "";
  }
}

std::vector<Lawnch::Result> do_wallpaper_query(const std::string &term,
                                               WallpapersState *state,
                                               const LawnchHostApi *host) {
  if (!fs::exists(state->wallpaper_dir) ||
      !fs::is_directory(state->wallpaper_dir)) {
    return {};
  }

  std::string search = to_lower(term, host);
  std::vector<Lawnch::Result> results;

  fs::recursive_directory_iterator it(
      state->wallpaper_dir, fs::directory_options::skip_permission_denied);

  for (const auto &entry : it) {
    if (!entry.is_regular_file())
      continue;

    const fs::path &p = entry.path();
    if (!is_image_ext(p.extension().string()))
      continue;

    std::string fname = to_lower(p.filename().string(), host);
    if (!search.empty() && fname.find(search) == std::string::npos)
      continue;

    std::string abs_path = p.string();

    Lawnch::Result r;
    r.name = p.filename().string();
    r.comment = p.parent_path().string();
    r.icon = "preferences-desktop-wallpaper";
    r.command = build_command(state->command_template, abs_path);
    r.type = "wallpaper-plugin";
    r.preview_image_path = (p.parent_path() / p.filename()).string();

    results.push_back(r);
  }
  return results;
}

} // namespace

class WallpapersPlugin : public Lawnch::Plugin {
private:
  WallpapersState state_;

public:
  void init(const LawnchHostApi *host) override {
    Plugin::init(host);
    init_wallpapers(&state_, host);
  }

  std::vector<std::string> get_triggers() override { return {":wall", ":wp"}; }

  Lawnch::Result get_help() override {
    Lawnch::Result r;
    r.name = ":wall / :wp";
    r.comment = "Set wallpaper";
    r.icon = "preferences-desktop-wallpaper";
    r.type = "help";
    return r;
  }

  std::vector<Lawnch::Result> query(const std::string &term) override {
    return do_wallpaper_query(term, &state_, host_);
  }
};

LAWNCH_PLUGIN_DEFINE(WallpapersPlugin)
