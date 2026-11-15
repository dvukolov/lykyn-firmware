#pragma once

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace lykyn_dashboard {

class LykynDashboard : public Component {
 public:
  explicit LykynDashboard(web_server_base::WebServerBase *base) : base_(base) {}
  void setup() override;
  // Higher than web_server (WIFI - 1) so this handler is registered first and wins "/".
  float get_setup_priority() const override { return setup_priority::WIFI - 0.5f; }

 protected:
  web_server_base::WebServerBase *base_;
};

}  // namespace lykyn_dashboard
}  // namespace esphome
