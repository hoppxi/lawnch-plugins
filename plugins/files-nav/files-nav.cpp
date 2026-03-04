#include "plugin_base.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool is_hidden(const fs::path &p) {
  auto name = p.filename().string();
  return !name.empty() && name[0] == '.';
}

std::string str_to_lower(const std::string &s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return out;
}

std::string expand_home(const std::string &path, const LawnchHostApi *host) {
  if (host && host->fs_api && host->fs_api->expand_tilde) {
    char *res = host->fs_api->expand_tilde(path.c_str());
    if (res) {
      std::string s(res);
      if (host->fs_api->free_path)
        host->fs_api->free_path(res);
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

std::string get_icon(const fs::directory_entry &e) {
  std::error_code ec;
  if (e.is_directory(ec))
    return "folder-symbolic";

  std::string ext = e.path().extension().string();
  if (ext.empty())
    return "text-x-generic";

  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  static const std::unordered_map<std::string, std::string> icon_map = {
      // Programming & Scripts
      {".c", "text-x-script"},
      {".cpp", "text-x-script"},
      {".h", "text-x-script"},
      {".hpp", "text-x-script"},
      {".py", "text-x-script"},
      {".js", "text-x-script"},
      {".ts", "text-x-script"},
      {".go", "text-x-script"},
      {".rs", "text-x-script"},
      {".java", "text-x-script"},
      {".sh", "text-x-script"},
      {".bash", "text-x-script"},
      {".php", "text-x-script"},
      {".rb", "text-x-script"},
      {".pl", "text-x-script"},
      {".lua", "text-x-script"},
      {".sql", "text-x-script"},
      {".html", "text-html"},
      {".css", "text-css"},
      {".json", "text-x-script"},
      {".xml", "text-xml"},
      {".nix", "text-x-script"},
      {".toml", "text-x-script"},
      {".yaml", "text-x-script"},
      {".yml", "text-x-script"},
      // Documents
      {".pdf", "document-pdf"},
      {".doc", "x-office-document"},
      {".docx", "x-office-document"},
      {".odt", "x-office-document"},
      {".rtf", "x-office-document"},
      {".xls", "x-office-spreadsheet"},
      {".xlsx", "x-office-spreadsheet"},
      {".ods", "x-office-spreadsheet"},
      {".csv", "x-office-spreadsheet"},
      {".ppt", "x-office-presentation"},
      {".pptx", "x-office-presentation"},
      {".odp", "x-office-presentation"},
      {".txt", "text-x-generic"},
      {".md", "text-markdown"},
      {".tex", "text-x-tex"},
      // Archives
      {".zip", "package-x-generic"},
      {".tar", "package-x-generic"},
      {".gz", "package-x-generic"},
      {".bz2", "package-x-generic"},
      {".xz", "package-x-generic"},
      {".7z", "package-x-generic"},
      {".rar", "package-x-generic"},
      {".deb", "package-x-generic"},
      {".rpm", "package-x-generic"},
      {".iso", "drive-optical"},
      // Images
      {".png", "image-x-generic"},
      {".jpg", "image-x-generic"},
      {".jpeg", "image-x-generic"},
      {".gif", "image-x-generic"},
      {".svg", "image-x-generic"},
      {".bmp", "image-x-generic"},
      {".webp", "image-x-generic"},
      {".ico", "image-x-generic"},
      {".tiff", "image-x-generic"},
      // Audio
      {".mp3", "audio-x-generic"},
      {".wav", "audio-x-generic"},
      {".ogg", "audio-x-generic"},
      {".flac", "audio-x-generic"},
      {".m4a", "audio-x-generic"},
      {".aac", "audio-x-generic"},
      {".opus", "audio-x-generic"},
      // Video
      {".mp4", "video-x-generic"},
      {".mkv", "video-x-generic"},
      {".avi", "video-x-generic"},
      {".mov", "video-x-generic"},
      {".wmv", "video-x-generic"},
      {".webm", "video-x-generic"},
      {".flv", "video-x-generic"},
      // Others
      {".bin", "application-x-executable"},
      {".appimage", "application-x-executable"},
      {".flatpak", "package-x-generic"},
      {".conf", "emblem-system"},
      {".cfg", "emblem-system"},
      {".ini", "emblem-system"},
      {".log", "text-x-generic-template"},
  };

  auto it = icon_map.find(ext);
  return (it != icon_map.end()) ? it->second : "text-x-generic";
}

std::vector<Lawnch::Result> list_directory(const fs::path &dir,
                                           const std::string &filter) {
  std::vector<Lawnch::Result> results;

  std::error_code ec;
  if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
    return results;

  std::string filter_lower = str_to_lower(filter);

  std::vector<fs::directory_entry> entries;
  try {
    for (const auto &e : fs::directory_iterator(
             dir, fs::directory_options::skip_permission_denied, ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      if (is_hidden(e.path()))
        continue;
      entries.push_back(e);
    }
  } catch (...) {
    return results;
  }

  std::sort(entries.begin(), entries.end(),
            [](const fs::directory_entry &a, const fs::directory_entry &b) {
              std::error_code ec2;
              bool a_dir = a.is_directory(ec2);
              bool b_dir = b.is_directory(ec2);
              if (a_dir != b_dir)
                return a_dir > b_dir;
              auto a_name = a.path().filename().string();
              auto b_name = b.path().filename().string();
              std::transform(a_name.begin(), a_name.end(), a_name.begin(),
                             [](unsigned char c) { return std::tolower(c); });
              std::transform(b_name.begin(), b_name.end(), b_name.begin(),
                             [](unsigned char c) { return std::tolower(c); });
              return a_name < b_name;
            });

  for (const auto &e : entries) {
    std::string name = e.path().filename().string();

    if (!filter_lower.empty() &&
        str_to_lower(name).find(filter_lower) == std::string::npos)
      continue;

    std::error_code ec3;
    bool is_dir = e.is_directory(ec3);
    std::string icon = get_icon(e);
    std::string path_str = e.path().string();

    std::string cmd = is_dir ? path_str : ("xdg-open \"" + path_str + "\"");

    Lawnch::Result r;
    r.name = name;
    r.comment = path_str;
    r.icon = icon;
    r.command = cmd;
    r.type = "plugin";
    r.preview_image_path = "";
    r.has_submenu = is_dir;

    if (icon == "image-x-generic") {
      r.preview_image_path = path_str;
    }

    results.push_back(r);
  }

  return results;
}

void parse_query(const std::string &term, fs::path &out_dir,
                 std::string &out_query, const std::string &default_root,
                 const LawnchHostApi *host) {
  out_dir = default_root;
  out_query = term;

  auto pos = term.find(":dir");
  if (pos == std::string::npos)
    return;

  size_t start = term.find_first_not_of(" ", pos + 4);
  if (start == std::string::npos)
    return;

  if (term[start] == '\'' || term[start] == '"') {
    char q = term[start];
    size_t end = term.find(q, start + 1);
    if (end != std::string::npos) {
      out_dir = expand_home(term.substr(start + 1, end - start - 1), host);
      out_query = term.substr(end + 1);
    }
  } else {
    size_t end = term.find(' ', start);
    out_dir = expand_home(term.substr(start, end - start), host);
    out_query = end == std::string::npos ? "" : term.substr(end);
  }

  out_query.erase(0, out_query.find_first_not_of(" "));
}

} // namespace

class FilesNavPlugin : public Lawnch::Plugin {
private:
  std::string root_dir_ = "/";

public:
  void init(const LawnchHostApi *host) override {
    Plugin::init(host);

    if (host && host->get_config_value) {
      const char *root = host->get_config_value(host, "root");
      if (root && root[0] != '\0') {
        root_dir_ = expand_home(root, host);
        return;
      }
    }

    root_dir_ = "/";
  }

  std::vector<std::string> get_triggers() override {
    return {":files-nav", ":fn"};
  }

  Lawnch::Result get_help() override {
    Lawnch::Result r;
    r.name = ":files-nav / :fn";
    r.comment = "Navigate files & folders (:dir <path>)";
    r.icon = "folder-symbolic";
    r.command = "";
    r.type = "help";
    r.preview_image_path = "";
    r.has_submenu = false;
    return r;
  }

  uint32_t get_flags() const override {
    return LAWNCH_PLUGIN_FLAG_DISABLE_SORT | LAWNCH_PLUGIN_FLAG_DISABLE_HISTORY;
  }

  std::vector<Lawnch::Result> query(const std::string &term) override {
    fs::path dir;
    std::string query_str;
    parse_query(term, dir, query_str, root_dir_, host_);
    return list_directory(dir, query_str);
  }

  std::vector<Lawnch::Result> query_submenu(const std::string &result_command,
                                            const std::string &term) override {
    fs::path dir(result_command);
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
      return {};

    return list_directory(dir, term);
  }
};

LAWNCH_PLUGIN_DEFINE(FilesNavPlugin)
