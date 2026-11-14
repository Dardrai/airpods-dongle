// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_BLUETOOTH_DEBUG_CONSOLE_H
#define AIRPODS_BLUETOOTH_DEBUG_CONSOLE_H

// Register the AVDTP source test console with the BTstack stdin
// (bluetooth_debug_console.c; built when HAVE_BTSTACK_STDIN is defined).
void bluetooth_debug_console_init(void);

#endif
