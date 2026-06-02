#pragma once

/*
 * Runs the first-boot setup wizard.
 * On return, auth_username is set, password is set, timezone is set.
 * Only called if this is the first boot (no username stored yet).
 */
void install_run(void);
