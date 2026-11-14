// SPDX-License-Identifier: GPL-3.0-only
#ifndef AIRPODS_HOST_TEST_VISIBILITY_H
#define AIRPODS_HOST_TEST_VISIBILITY_H

// Module state and helpers stay private to their translation unit in firmware.
// Host tests compile a unit with -DHOST_TEST_VISIBILITY so the test linked
// against it can observe and reset that state through the module's internal
// header. Firmware code generation is unchanged: there the marker is `static`.
#ifdef HOST_TEST_VISIBILITY
#define PRIVATE_UNLESS_HOST_TEST
#else
#define PRIVATE_UNLESS_HOST_TEST static
#endif

#endif
