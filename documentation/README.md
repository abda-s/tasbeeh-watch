# Documentation

Technical documentation for the Tasbeeh Smartwatch firmware. For an overview of the project, start with the
[main README](../README.md).

## Where to find things

| Document | What's in it |
|---|---|
| [User Interface](user-interface.md) | Screens, gestures, the navigation model (ring + modals), color palette, the time editor |
| [Architecture](architecture.md) | Source layout, data persistence, rendering performance |
| [Reminders & Notifications](reminders-and-notifications.md) | The reminder data model, the preset config table, the UI, tap-target tuning |
| [Power Management](power-management.md) | Every power optimization from V1 to V1.5, with measurements and the real battery capacity |
| [Deep Sleep & Battery Lockdown](deep-sleep-and-lockdown.md) | Real deep sleep, keeping accurate time across it, the <5% lockdown mode, boot-sequence fixes, bench tools |
| [Font System](fonts.md) | How Arabic text is shaped in LVGL and how this project's fonts are generated |
| [Hardware](hardware.md) | Board, display, touch, battery and charger details, and how the battery is read |
| [Build & Configuration](build-and-configuration.md) | Building and flashing, libraries, LVGL configuration, patches to vendored libraries |

## Elsewhere in the repository

| Location | What's there |
|---|---|
| [`../sim/README.md`](../sim/README.md) | The desktop simulator — how it works and what it fakes |
| [`../datasheets/`](../datasheets/) | Board schematic and component datasheets (ESP32-S3, GC9A01A, CST816S, QMI8658A) |
| [`../power_profiler/`](../power_profiler/README.md) | The INA226 power profiler used for the measurements in [Power Management](power-management.md) |
| [`screenshots/`](screenshots/) | Screenshots used in the README, captured from the simulator |

## Suggested reading order

1. [User Interface](user-interface.md) — what the watch does and how you move around it.
2. [Architecture](architecture.md) — how the code is organized.
3. [Power Management](power-management.md) and [Deep Sleep & Battery Lockdown](deep-sleep-and-lockdown.md) —
   the part of this project that took the most engineering, and why.
