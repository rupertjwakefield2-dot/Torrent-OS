#pragma once
#include <stdint.h>

#define AUTH_USER_MAX  65   /* 64 chars + NUL */
#define AUTH_PASS_MIN  4
#define AUTH_PASS_MAX  100
#define AUTH_MAX_TRIES 10

/* global state — read by desktop, shell */
extern char auth_username[AUTH_USER_MAX];
extern int  auth_dark_mode;   /* 1 = dark (default), 0 = light */

void auth_init(void);

/* returns 1 on success, 0 if passwords didn't match */
int  auth_set_password(const char *pass, const char *confirm);

/*
 * Shows the password-entry login screen.
 * Loops until correct password entered.
 * Halts the system after AUTH_MAX_TRIES wrong attempts.
 */
void auth_login(void);

/* validate a single password attempt; returns 1 = match */
int  auth_check(const char *attempt);
