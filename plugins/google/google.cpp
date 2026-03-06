#include "plugin_base.hpp"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <curl/curl.h>
#include <deque>
#include <map>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
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

size_t write_callback(void *contents, size_t size, size_t nmemb,
                      std::string *s) {
  size_t newLength = size * nmemb;
  try {
    s->append((char *)contents, newLength);
  } catch (std::bad_alloc &e) {
    return 0;
  }
  return newLength;
}

std::vector<std::string> parse_google_json(const std::string &json) {
  std::vector<std::string> suggestions;

  size_t outer_bracket = json.find('[');
  if (outer_bracket == std::string::npos)
    return suggestions;

  size_t inner_start = json.find('[', outer_bracket + 1);
  if (inner_start == std::string::npos)
    return suggestions;

  size_t inner_end = json.find(']', inner_start);
  if (inner_end == std::string::npos)
    return suggestions;

  std::string list_str =
      json.substr(inner_start + 1, inner_end - inner_start - 1);

  bool in_quote = false;
  std::string current;
  for (size_t i = 0; i < list_str.length(); ++i) {
    char c = list_str[i];
    if (c == '"') {
      in_quote = !in_quote;
    } else if (c == ',' && !in_quote) {
      if (!current.empty()) {
        suggestions.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty())
    suggestions.push_back(current);

  return suggestions;
}

struct GoogleState {
  std::map<std::string, std::vector<std::string>> cache;
  std::deque<std::string> query_queue;
  std::mutex mutex;
  std::condition_variable cv;
  std::atomic<bool> running{false};
  std::thread worker_thread;
};

void worker_loop(GoogleState *state) {
  while (state->running) {
    std::string query;
    {
      std::unique_lock<std::mutex> lock(state->mutex);
      state->cv.wait(lock, [state] {
        return !state->query_queue.empty() || !state->running;
      });

      if (!state->running)
        break;

      query = state->query_queue.front();
      state->query_queue.pop_front();

      if (state->cache.count(query))
        continue;
    }

    CURL *curl = curl_easy_init();
    if (curl) {
      std::string encoded = url_encode(query);
      std::string url = "http://suggestqueries.google.com/complete/"
                        "search?client=firefox&q=" +
                        encoded;
      std::string response_string;

      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

      CURLcode res = curl_easy_perform(curl);
      curl_easy_cleanup(curl);

      if (res == CURLE_OK && !response_string.empty()) {
        auto sugs = parse_google_json(response_string);

        {
          std::lock_guard<std::mutex> lock(state->mutex);
          state->cache[query] = sugs;
        }
      }
    }
  }
}

std::vector<Lawnch::Result> do_google_query(const std::string &term,
                                            GoogleState *state,
                                            const LawnchHostApi *host) {
  try {
    std::string query_str = term;
    std::string display =
        query_str.empty() ? "Type to search Google..." : query_str;

    std::vector<Lawnch::Result> results;

    std::string encoded_term = url_encode(query_str);
    std::string command =
        "xdg-open \"https://www.google.com/search?q=" + encoded_term + "\"";

    Lawnch::Result r;
    r.name = display;
    r.comment = "Search Google (Enter to open)";
    r.icon = "web-browser";
    r.command = command;
    r.type = "google";
    results.push_back(r);

    bool cache_hit = false;
    std::vector<std::string> cached_sugs;

    if (!query_str.empty()) {
      std::lock_guard<std::mutex> lock(state->mutex);
      if (state->cache.count(query_str)) {
        cached_sugs = state->cache[query_str];
        cache_hit = true;
      } else {
        state->query_queue.push_back(query_str);
        state->cv.notify_one();
      }
    }

    if (cache_hit) {
      for (const auto &sug : cached_sugs) {
        std::string sug_cmd =
            "xdg-open \"https://www.google.com/search?q=" + url_encode(sug) +
            "\"";

        Lawnch::Result sr;
        sr.name = sug;
        sr.comment = "Google Suggestion";
        sr.icon = "system-search";
        sr.command = sug_cmd;
        sr.type = "google-suggest";
        results.push_back(sr);
      }
    }

    if (host && host->log_api && !term.empty()) {
      std::string msg = "Query '" + term + "' returned " +
                        std::to_string(results.size()) + " suggestions";
      host->log_api->log("GooglePlugin", LAWNCH_LOG_DEBUG, msg.c_str());
    }

    return results;

  } catch (const std::exception &e) {
    if (host && host->log_api) {
      std::string msg = std::string("Google query failed: ") + e.what();
      host->log_api->log("GooglePlugin", LAWNCH_LOG_ERROR, msg.c_str());
    }
    Lawnch::Result r;
    r.name = "Google Error";
    r.comment = e.what();
    r.icon = "dialog-error";
    r.type = "error";
    return {r};
  }
}

} // namespace

class GooglePlugin : public Lawnch::Plugin {
private:
  GoogleState state_;

public:
  void init(const LawnchHostApi *host) override {
    Plugin::init(host);
    if (host && host->log_api) {
      host->log_api->log("GooglePlugin", LAWNCH_LOG_INFO, "Initializing...");
    }
    curl_global_init(CURL_GLOBAL_ALL);
    state_.running = true;
    state_.worker_thread = std::thread(worker_loop, &state_);
    if (host && host->log_api) {
      host->log_api->log("GooglePlugin", LAWNCH_LOG_INFO,
                         "Worker thread started");
    }
  }

  void destroy() override {
    state_.running = false;
    state_.cv.notify_all();
    if (state_.worker_thread.joinable()) {
      state_.worker_thread.join();
    }
    curl_global_cleanup();
  }

  std::vector<std::string> get_triggers() override { return {":google", ":g"}; }

  Lawnch::Result get_help() override {
    Lawnch::Result r;
    r.name = ":google / :g";
    r.comment = "Search Google";
    r.icon = "web-browser";
    r.type = "help";
    return r;
  }

  std::vector<Lawnch::Result> query(const std::string &term) override {
    return do_google_query(term, &state_, host_);
  }
};

LAWNCH_PLUGIN_DEFINE(GooglePlugin)
