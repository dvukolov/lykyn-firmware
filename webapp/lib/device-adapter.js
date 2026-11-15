/**
 * Everything the Lykyn page needs to know about this chamber that another chamber does not.
 * The page never checks the device type: it asks this file. See lib/stage-standard.js.
 */

const { canonicalStage } = require("./stage-standard")

/** Canonical stage to the name the Lykyn stage select uses. All four are native. */
const NATIVE_STAGE = { Colonization: "Colonization", Pinning: "Pinning", Fruiting: "Fruiting", Custom: "Custom" }

/** Canonical stages this chamber has, in the standard's order. */
const STAGES_AVAILABLE = ["Colonization", "Pinning", "Fruiting", "Custom"]

/** The Lykyn firmware loads Temp, Humidity, Light and Fresh air itself when its stage select changes. */
const PRESETS_ON_DEVICE = true

/** White is a 0-100 percent channel added on top of the colors. */
const WHITE_UNIT = "%"

/** The fan never runs below 33 percent. */
const FAN_MIN_PERCENT = 33

function nativeStage(stage) {
  return NATIVE_STAGE[stage] || null
}

/** Writes that put this chamber in a stage. The firmware sets the four knobs when the select changes. */
function stageWrites(stage) {
  const native = nativeStage(stage)
  return native ? [{ kind: "select", id: "stage", value: native }] : []
}

/** Canonical stage for what the chamber's stage select says now. */
function currentStage(nativeName) {
  return canonicalStage(nativeName)
}

/** White as the chamber counts it, for the Light card. */
function whiteDisplay(raw) {
  return Math.round(Number(raw) || 0)
}

module.exports = { STAGES_AVAILABLE, PRESETS_ON_DEVICE, WHITE_UNIT, FAN_MIN_PERCENT, nativeStage, stageWrites, currentStage, whiteDisplay }
