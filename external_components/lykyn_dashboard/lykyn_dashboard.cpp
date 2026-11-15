#include "lykyn_dashboard.h"

#include <string>
#include "static_files.h"

#include "esphome/core/log.h"
#include "esphome/components/web_server_idf/web_server_idf.h"

// Served at /esp (and /entities). Linked from the stock web_server component.
extern const uint8_t ESPHOME_WEBSERVER_INDEX_HTML[];
extern const size_t ESPHOME_WEBSERVER_INDEX_HTML_SIZE;

namespace esphome {
namespace lykyn_dashboard {

static const char *const TAG = "lykyn_dashboard";

using web_server_idf::AsyncWebHandler;
using web_server_idf::AsyncWebServerRequest;

static bool is_dashboard_html(const std::string &url) {
  return url == "/" || url == "/index.html";
}

static bool is_app_alias(const std::string &url) {
  return url == "/app" || url == "/app/" || url == "/app/index.html";
}

static bool is_esphome_ui(const std::string &url) {
  return url == "/esp" || url == "/esp/" || url == "/entities" || url == "/entities/";
}

class DashboardPage : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_GET)
      return false;
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    const std::string url(request->url_to(buf));
    if (is_dashboard_html(url) || is_app_alias(url) || is_esphome_ui(url))
      return true;
    for (size_t i = 0; i < STATIC_FILES_COUNT; i++) {
      if (url == STATIC_FILES[i].url)
        return true;
    }
    return false;
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    const std::string url(request->url_to(buf));

    if (is_app_alias(url)) {
      // beginResponse() on this IDF backend only knows a few status codes and
      // would turn 302 into 500. Use the first-class redirect helper instead.
      request->redirect("/");
      return;
    }

    if (is_esphome_ui(url)) {
      auto *res = request->beginResponse(200, "text/html", ESPHOME_WEBSERVER_INDEX_HTML,
                                         ESPHOME_WEBSERVER_INDEX_HTML_SIZE);
      request->send(res);
      return;
    }

    for (size_t i = 0; i < STATIC_FILES_COUNT; i++) {
      if (url == STATIC_FILES[i].url) {
        auto *res = request->beginResponse(200, STATIC_FILES[i].content_type, STATIC_FILES[i].data,
                                           STATIC_FILES[i].size);
        res->addHeader("Content-Encoding", "gzip");
        if (url.find("/_next/") != std::string::npos)
          res->addHeader("Cache-Control", "public, max-age=31536000, immutable");
        else
          res->addHeader("Cache-Control", "no-store");
        request->send(res);
        return;
      }
    }

    const StaticFile *html = nullptr;
    for (size_t i = 0; i < STATIC_FILES_COUNT; i++) {
      if (std::string(STATIC_FILES[i].url) == "/") {
        html = &STATIC_FILES[i];
        break;
      }
    }
    if (html == nullptr) {
      request->send(404, "text/plain", "Not Found");
      return;
    }
    auto *res = request->beginResponse(200, "text/html", html->data, html->size);
    res->addHeader("Content-Encoding", "gzip");
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  }
};

void LykynDashboard::setup() {
  this->base_->add_handler(new DashboardPage());  // NOLINT(cppcoreguidelines-owning-memory)
  ESP_LOGI(TAG, "Shroom Labs dashboard is at /; stock ESPHome UI is at /esp");
}

}  // namespace lykyn_dashboard
}  // namespace esphome
