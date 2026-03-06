#include "plugin_base.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct FileEntry {
  fs::path path;
  std::string name;
  std::string name_lower;
  bool is_dir;
};

struct DirIndex {
  std::vector<FileEntry> files;
  std::shared_mutex mutex;
  std::atomic<bool> scanning{false};
  std::atomic<bool> stop{false};
  std::once_flag once;
};

bool is_hidden(const fs::path &p) {
  auto name = p.filename().string();
  return !name.empty() && name[0] == '.';
}

void scan_dir(DirIndex *index, fs::path root, int max_depth) {
  index->scanning = true;

  std::vector<std::pair<fs::path, int>> stack;
  stack.emplace_back(root, 0);

  const auto slice = std::chrono::milliseconds(6);

  while (!stack.empty() && !index->stop.load()) {
    auto start = std::chrono::steady_clock::now();

    while (!stack.empty()) {
      auto [dir, depth] = stack.back();
      stack.pop_back();

      if (depth > max_depth || !fs::exists(dir))
        continue;

      std::error_code ec;
      for (const auto &e : fs::directory_iterator(dir, ec)) {
        if (ec || is_hidden(e.path()))
          continue;

        bool is_dir = e.is_directory(ec);
        if (ec)
          continue;

        FileEntry fe;
        fe.path = e.path();
        fe.name = fe.path.filename().string();
        std::string lower = fe.name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        fe.name_lower = lower;
        fe.is_dir = is_dir;

        {
          std::unique_lock lock(index->mutex);
          index->files.emplace_back(std::move(fe));
        }

        if (is_dir)
          stack.emplace_back(e.path(), depth + 1);
      }

      if (std::chrono::steady_clock::now() - start > slice)
        break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  index->scanning = false;
}

std::string get_icon_for_file(const FileEntry &f) {
  if (f.is_dir) {
    return "folder-symbolic";
  }

  std::string ext = f.path.extension().string();
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

      // ohers
      {".bin", "application-x-executable"},
      {".appimage", "application-x-executable"},
      {".flatpak", "package-x-generic"},
      {".conf", "emblem-system"},
      {".cfg", "emblem-system"},
      {".ini", "emblem-system"},
      {".log", "text-x-generic-template"}};

  auto it = icon_map.find(ext);
  if (it != icon_map.end()) {
    return it->second;
  }

  return "text-x-generic";
}

struct FilesState {
  std::unordered_map<std::string, DirIndex> indexes;
  std::mutex index_map_mutex;
  int max_depth = 4;
};

std::string to_lower(const std::string &str, const LawnchHostApi *host) {
  if (host && host->str_api && host->str_api->to_lower_copy) {
    char *res = host->str_api->to_lower_copy(str.c_str());
    if (res) {
      std::string ret(res);
      if (host->str_api->free_str)
        host->str_api->free_str(res);
      else
        free(res);
      return ret;
    }
  }

  return str;
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

  return path;
}

void init_files(FilesState *state, const LawnchHostApi *host) {
  const char *max_depth = host->get_config_value(host, "max_depth");
  if (max_depth && *max_depth) {
    state->max_depth = std::stoi(max_depth);
  }
  if (host && host->log_api) {
    std::string msg =
        "Initialized with max_depth=" + std::to_string(state->max_depth);
    host->log_api->log("FilesPlugin", LAWNCH_LOG_INFO, msg.c_str());
  }
}

void parse_query(const std::string &term, fs::path &out_dir,
                 std::string &out_query, const LawnchHostApi *host) {
  out_dir = expand_home("~", host);
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

std::vector<Lawnch::Result> do_file_query(const std::string &term,
                                          FilesState *state,
                                          const LawnchHostApi *host) {
  try {
    fs::path dir;
    std::string query_str;
    parse_query(term, dir, query_str, host);

    if (!fs::exists(dir)) {
      if (host && host->log_api) {
        std::string msg = "Directory not found: " + dir.string();
        host->log_api->log("FilesPlugin", LAWNCH_LOG_WARNING, msg.c_str());
      }
      return {};
    }

    DirIndex *index;
    {
      std::lock_guard lock(state->index_map_mutex);
      index = &state->indexes[dir.string()];
    }

    std::call_once(index->once, [&]() {
      std::thread(scan_dir, index, dir, state->max_depth).detach();
    });

    std::string q = to_lower(query_str, host);
    std::vector<Lawnch::Result> out;

    const char *editor = std::getenv("EDITOR") ?: "nano";
    const char *terminal = std::getenv("TERMINAL") ?: "xterm";

    {
      std::shared_lock lock(index->mutex);

      for (const auto &f : index->files) {
        if (!q.empty() && f.name_lower.find(q) == std::string::npos)
          continue;

        std::string cmd = f.is_dir ? std::string(terminal) + " -e " + editor +
                                         " \"" + f.path.string() + "\""
                                   : "xdg-open \"" + f.path.string() + "\"";

        Lawnch::Result r;
        r.name = f.name;
        r.comment = f.path.string();

        std::string icon_name = get_icon_for_file(f);
        r.icon = icon_name;

        r.command = cmd;
        r.type = "plugin";

        if (icon_name == "image-x-generic") {
          r.preview_image_path = f.path.string();
        }

        out.push_back(r);
      }
    }

    if (host && host->log_api && !term.empty()) {
      std::string msg = "Query '" + term + "' returned " +
                        std::to_string(out.size()) + " files";
      host->log_api->log("FilesPlugin", LAWNCH_LOG_DEBUG, msg.c_str());
    }

    return out;

  } catch (const std::exception &e) {
    if (host && host->log_api) {
      std::string msg = std::string("File query failed: ") + e.what();
      host->log_api->log("FilesPlugin", LAWNCH_LOG_ERROR, msg.c_str());
    }
    Lawnch::Result r;
    r.name = "File Plugin Error";
    r.comment = e.what();
    r.icon = "dialog-error";
    r.type = "error";
    return {r};
  }
}

} // namespace

class FilesPlugin : public Lawnch::Plugin {
private:
  FilesState state_;

public:
  void init(const LawnchHostApi *host) override {
    Plugin::init(host);
    init_files(&state_, host);
  }

  void destroy() override {
    std::lock_guard lock(state_.index_map_mutex);
    for (auto &[path, idx] : state_.indexes) {
      idx.stop = true;
    }
    state_.indexes.clear();
  }

  std::vector<std::string> get_triggers() override { return {":file", ":f"}; }

  Lawnch::Result get_help() override {
    Lawnch::Result r;
    r.name = ":file / :f";
    r.comment = "Search files (:dir <path>)";
    r.icon = "folder-symbolic";
    r.type = "help";
    return r;
  }

  std::vector<Lawnch::Result> query(const std::string &term) override {
    return do_file_query(term, &state_, host_);
  }
};

LAWNCH_PLUGIN_DEFINE(FilesPlugin)
