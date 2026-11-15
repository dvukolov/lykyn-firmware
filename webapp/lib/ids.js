// Lykyn REST object ids, and the names the dashboard already uses.
// The UI talks in the Shrooly-style ids it was written with. This file is
// the only place that names the Lykyn entities those calls must hit.

const DEVICE_TO_UI = {
  current_temperature: "temperature",
  current_humidity: "humidity",
  humidity_tolerance: "humidity__hysteresis",
  temperature_target: "temperature__target",
  temperature_tolerance: "temperature__hysteresis",
  mist_level: "humidifier__speed",
  air_exchange_every: "air_exchange__period__min_",
  air_exchange_run: "air_exchange__run_duration__s_",
  air_exchange_fan_speed: "air_exchange__speed",
  light_sunrise_hour: "lights__sunrise_hour",
  light_hours: "lights__duration__hours_",
  light_white: "white_led_intensity",
  light_red: "red_led_intensity",
  light_green: "green_led_intensity",
  light_blue: "blue_led_intensity",
  stage: "grow_phase",
  humidifier: "humidifier",
  target_humidity: "target_humidity",
  wifi_mode: "wifi_mode",
  wifi_ssid: "wifi_ssid",
  ip_address: "ip_address",
  control_mode: "control_mode",
  light_enabled: "light_enabled",
  light_brightness: "light_brightness",
  inlet_fan_manual: "inlet_fan_manual",
  outlet_fan_manual: "outlet_fan_manual",
  led_test: "led_test",
  humidifier_status: "humidifier_status",
  air_exchange_status: "air_exchange_status",
  light_status: "light_status",
  active_alerts: "active_alerts",
  installed_version: "installed_version",
  grow_light: "grow_light",
}

const UI_TO_DEVICE = {}
for (const [deviceId, uiId] of Object.entries(DEVICE_TO_UI)) {
  UI_TO_DEVICE[uiId] = deviceId
  if (!(deviceId in UI_TO_DEVICE)) UI_TO_DEVICE[deviceId] = deviceId
}

const KINDS = [
  "number",
  "sensor",
  "text_sensor",
  "switch",
  "select",
  "button",
  "binary_sensor",
  "light",
]

function splitEntityKey(key) {
  const text = String(key || "")
  for (const kind of KINDS) {
    const prefix = `${kind}-`
    if (text.startsWith(prefix)) return { kind, id: text.slice(prefix.length) }
  }
  return { kind: "", id: text }
}

function toDeviceId(id) {
  return UI_TO_DEVICE[id] || id
}

function toUiId(id) {
  return DEVICE_TO_UI[id] || id
}

function expandEntityKey(key) {
  const { kind, id } = splitEntityKey(key)
  const ids = new Set([id, toUiId(id), toDeviceId(id)])
  return [...ids].map((item) => (kind ? `${kind}-${item}` : item))
}

// ESPHome 2026.10 web_server matches and emits the entity *name*, not the object id.
const DEVICE_TO_NAME = {
  current_temperature: "Current Temperature",
  current_humidity: "Current Humidity",
  wifi_signal: "WiFi Signal",
  uptime: "Uptime",
  current_air_exchange_fan_speed: "Current Air Exchange Fan Speed",
  humidifier: "Humidifier",
  light_enabled: "Light Enabled",
  grow_light: "Grow Light",
  control_mode: "Control Mode",
  stage: "Stage",
  target_humidity: "Target Humidity",
  humidity_tolerance: "Humidity Tolerance",
  temperature_target: "Temperature Target",
  temperature_tolerance: "Temperature Tolerance",
  mist_level: "Mist Level",
  air_exchange_every: "Air Exchange Every",
  air_exchange_run: "Air Exchange Run",
  air_exchange_fan_speed: "Air Exchange Fan Speed",
  inlet_fan_manual: "Inlet Fan Manual",
  outlet_fan_manual: "Outlet Fan Manual",
  light_sunrise_hour: "Light Sunrise Hour",
  light_hours: "Light Hours",
  light_brightness: "Light Brightness",
  light_white: "Light White",
  light_red: "Light Red",
  light_green: "Light Green",
  light_blue: "Light Blue",
  ip_address: "IP Address",
  wifi_ssid: "WiFi SSID",
  wifi_mode: "WiFi Mode",
  humidifier_status: "Humidifier Status",
  air_exchange_status: "Air Exchange Status",
  light_status: "Light Status",
  active_alerts: "Active Alerts",
  status_json: "Status JSON",
  installed_version: "Installed Version",
  restart: "Restart",
  led_test: "LED Test",
}

function snakeName(name) {
  return String(name || "")
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "")
}

function restPath(kind, id) {
  const deviceId = toDeviceId(id)
  const name = DEVICE_TO_NAME[deviceId] || DEVICE_TO_NAME[id] || id
  return `/${kind}/${encodeURIComponent(name)}`
}

function incomingKeys(eventId) {
  const text = String(eventId || "")
  let kind = ""
  let name = text
  if (text.includes("/")) {
    const i = text.indexOf("/")
    kind = text.slice(0, i)
    name = text.slice(i + 1)
  } else {
    const split = splitEntityKey(text)
    kind = split.kind
    name = split.id
  }
  const slug = snakeName(name)
  const keys = new Set()
  const add = (k, i) => { if (k && i) keys.add(`${k}-${i}`) }
  add(kind, slug)
  add(kind, toUiId(slug))
  add(kind, toDeviceId(slug))
  add(kind, name)
  return [...keys]
}

function aliasEvent(event) {
  if (!event || !event.id) return [event]
  const keys = incomingKeys(event.id)
  if (!keys.length) return [event]
  return keys.map((id) => ({ ...event, id }))
}


const NUMBER_DEVICE_IDS = [
  "target_humidity",
  "humidity_tolerance",
  "mist_level",
  "temperature_target",
  "temperature_tolerance",
  "air_exchange_every",
  "air_exchange_run",
  "air_exchange_fan_speed",
  "light_sunrise_hour",
  "light_hours",
  "light_brightness",
  "light_white",
  "light_red",
  "light_green",
  "light_blue",
  "inlet_fan_manual",
  "outlet_fan_manual",
]

function aliasEntityMap(map) {
  const out = { ...map }
  for (const [key, value] of Object.entries(map || {})) {
    for (const alias of incomingKeys(key).length ? incomingKeys(key) : expandEntityKey(key)) {
      if (!(alias in out)) out[alias] = value
    }
    for (const alias of expandEntityKey(key)) {
      if (!(alias in out)) out[alias] = value
    }
  }
  return out
}

module.exports = {
  DEVICE_TO_UI,
  UI_TO_DEVICE,
  DEVICE_TO_NAME,
  NUMBER_DEVICE_IDS,
  toDeviceId,
  toUiId,
  expandEntityKey,
  aliasEvent,
  aliasEntityMap,
  splitEntityKey,
  snakeName,
  restPath,
  incomingKeys,
}
