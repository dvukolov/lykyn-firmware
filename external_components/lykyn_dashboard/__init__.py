"""Serves the Shroom Labs chamber dashboard at /.

The Next.js export is gzip-embedded at compile time. The stock ESPHome entity
table is moved to /esp (also /entities).
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

CODEOWNERS = ["@themsquared"]
DEPENDENCIES = ["web_server", "esp32"]
AUTO_LOAD = ["web_server_base"]

lykyn_dashboard_ns = cg.esphome_ns.namespace("lykyn_dashboard")
LykynDashboard = lykyn_dashboard_ns.class_("LykynDashboard", cg.Component)

CONF_WEBAPP_OUT = "webapp_out"

COMPONENT_DIR = Path(__file__).parent
GENERATED_H = COMPONENT_DIR / "static_files.h"
GENERATED_CPP = COMPONENT_DIR / "static_files.cpp"
EMBED_SCRIPT = COMPONENT_DIR.parents[1] / "scripts" / "embed_static_files.py"


def _maybe_embed(config):
    # Resolve the Next export relative to the firmware repo (the YAML's folder).
    out_dir = Path(CORE.config_dir or ".") / config[CONF_WEBAPP_OUT]
    if out_dir.is_dir() and (out_dir / "index.html").is_file():
        import importlib.util

        spec = importlib.util.spec_from_file_location("embed_static_files", EMBED_SCRIPT)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        mod.embed(out_dir, GENERATED_H, GENERATED_CPP, namespace="lykyn_dashboard")
        return config
    if GENERATED_H.is_file() and GENERATED_CPP.is_file():
        return config
    raise cv.Invalid(
        "The dashboard has not been built. From the firmware repo run: "
        "scripts/build-webapp.sh  (needs Node, then writes webapp/out and embeds it)"
    )


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LykynDashboard),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
            cv.Optional(CONF_WEBAPP_OUT, default="webapp/out"): cv.string,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _maybe_embed,
)


async def to_code(config):
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    var = cg.new_Pvariable(config[CONF_ID], base)
    await cg.register_component(var, config)
