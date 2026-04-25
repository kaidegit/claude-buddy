# BLE Protocol

Claude Desktop communicates with the device over Nordic UART Service compatible
BLE. The payload format is newline-delimited JSON: one UTF-8 JSON object per
line, terminated by `\n`.

This firmware aims to look like a Claude Hardware Buddy device to the desktop
app while keeping parsing and storage failure modes explicit on the embedded
side.

## GATT Profile

| Item | UUID | Direction |
| --- | --- | --- |
| NUS service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | advertised primary service |
| RX characteristic | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | desktop to device, write/write without response |
| TX characteristic | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | device to desktop, notify |

The SiFli GATT table stores the 128-bit UUIDs in little-endian byte order, but
external BLE tools should show the canonical UUIDs above.

The target security mode is encrypted bonding with passkey display. Development
bring-up may temporarily relax security, but public firmware should require an
encrypted link before protocol payloads are exchanged.

## Transport Rules

- Device RX buffers bytes until a newline is received.
- A complete line is parsed as one JSON object.
- Lines exceeding the configured maximum are dropped and counted.
- Device TX responses are JSON objects with trailing newlines.
- Notifications are split into MTU-aware chunks.
- Unknown commands receive an explicit unsupported acknowledgement.

The firmware does not log raw protocol payloads by default because transcripts,
tool hints, and prompt ids may contain sensitive data.

## Snapshot Messages

Messages without `cmd` are treated as Claude runtime snapshots. The parser keeps
only bounded fields needed by the UI:

```json
{
  "total": 12,
  "running": 1,
  "waiting": 0,
  "tokens": 4321,
  "tokens_today": 9876,
  "msg": "Working",
  "entries": ["Read file", "Edited code"],
  "prompt": {
    "id": "req_123",
    "tool": "Edit",
    "hint": "Update project files"
  }
}
```

When `prompt.id` is present, the UI enters the approval flow. When the prompt is
removed or replaced, any pending local decision state is cleared.

## Device Commands

The desktop may send command objects with `cmd`.

| Command | Purpose | Success acknowledgement |
| --- | --- | --- |
| `status` | Report runtime connection, encryption, counters, identity, and species state | `{ "ack": "status", "ok": true, "data": { ... } }` |
| `name` | Set display name from `name` or `value` | `{ "ack": "name", "ok": true }` |
| `owner` | Set owner from `owner`, `name`, or `value` | `{ "ack": "owner", "ok": true }` |
| `time` | Synchronize epoch and timezone offset | `{ "ack": "time", "ok": true }` |
| `unpair` | Clear BLE bonds | `{ "ack": "unpair", "ok": true }` |
| `species` | Select a built-in ASCII character by numeric id | `{ "ack": "species", "ok": true }` |
| `delete_character` / `char_delete` | Delete active runtime character and fall back to built-in assets | `{ "ack": "<cmd>", "ok": true }` |
| `factory_reset` | Clear character files, preferences, bonds, and runtime state | `{ "ack": "factory_reset", "ok": true }` |

Unsupported commands return:

```json
{"ack":"<cmd>","ok":false,"error":"unsupported"}
```

## Permission Decisions

The device sends permission decisions to Claude only when a prompt is active and
the prompt id still matches the current snapshot:

```json
{"cmd":"permission","id":"req_123","decision":"once"}
{"cmd":"permission","id":"req_123","decision":"deny"}
```

The UI should consider a decision pending until a later snapshot removes or
replaces the prompt.

## Character Package Transfer

Runtime character updates are transferred as a staged package. The active
character is not replaced until all files and the manifest validate.

| Command | Purpose | Key fields |
| --- | --- | --- |
| `char_begin` | Start a package | `name`, `total` |
| `file` | Start one file | `path` or `name`, `size` |
| `chunk` | Append base64 data to the open file | `d` |
| `file_end` | Close and size-check the open file | optional `n` |
| `char_end` | Validate manifest and activate package | none |

Successful chunk acknowledgements report the decoded byte count written for the
current file:

```json
{"ack":"chunk","ok":true,"n":1024}
```

The manifest must be named `manifest.json` and must define every runtime state
used by the UI:

```json
{
  "name": "Custom Buddy",
  "states": {
    "sleep": "sleep.gif",
    "idle": "idle.gif",
    "busy": "busy.gif",
    "attention": "attention.gif",
    "celebrate": "celebrate.gif",
    "dizzy": "dizzy.gif",
    "heart": "heart.gif"
  }
}
```

Each state value may also be a non-empty array of filenames. All referenced
files must exist in the staged package.

## Path And Size Safety

The embedded side rejects unsafe package paths even if the desktop sender has
already filtered them. Rejected paths include:

- empty names
- hidden names beginning with `.`
- `.` or `..`
- absolute paths
- names containing `/` or `\`
- names longer than the internal path buffer

The device also rejects packages that exceed available filesystem space plus a
safety margin. A failed package must leave the previous active character intact.
