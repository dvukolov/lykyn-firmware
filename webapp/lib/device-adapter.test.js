const test = require("node:test")
const assert = require("node:assert/strict")
const adapter = require("./device-adapter")

test("Lykyn has all four stages under their own names and lets the firmware load the recipe", () => {
  assert.deepEqual(adapter.STAGES_AVAILABLE, ["Colonization", "Pinning", "Fruiting", "Custom"])
  assert.equal(adapter.PRESETS_ON_DEVICE, true)
  for (const stage of adapter.STAGES_AVAILABLE) {
    assert.equal(adapter.nativeStage(stage), stage)
    assert.deepEqual(adapter.stageWrites(stage), [{ kind: "select", id: "stage", value: stage }])
  }
})

test("the fan floor and white unit are this chamber's own", () => {
  assert.equal(adapter.FAN_MIN_PERCENT, 33)
  assert.equal(adapter.WHITE_UNIT, "%")
  assert.equal(adapter.whiteDisplay("20.2"), 20)
})

test("the current stage is read back through the canonical name", () => {
  assert.equal(adapter.currentStage("Colonization"), "Colonization")
  assert.equal(adapter.currentStage("Idle"), null)
})
