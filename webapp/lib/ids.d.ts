export function toDeviceId(id: string): string
export function toUiId(id: string): string
export function expandEntityKey(key: string): string[]
export function aliasEvent<T extends { id?: string }>(event: T): T[]
export function aliasEntityMap(map: Record<string, unknown>): Record<string, unknown>
export const NUMBER_DEVICE_IDS: string[]
export function restPath(kind: string, id: string): string
export function incomingKeys(eventId: string): string[]
export function snakeName(name: string): string
