#ifndef UI_ENUM_PARSE_H
#define UI_ENUM_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *name;
  int value;
} enum_token_t;

static inline int enum_parse_token(const char *s, const enum_token_t *tokens,
                                   size_t count, int fallback) {
  if (!s || !*s || !tokens) return fallback;
  for (size_t i = 0; i < count; i++) {
    if (tokens[i].name && strcmp(tokens[i].name, s) == 0)
      return tokens[i].value;
  }

  char *end = NULL;
  long n = strtol(s, &end, 0);
  if (end && *end == '\0' && n >= (long)INT32_MIN && n <= (long)INT32_MAX)
    return (int)n;
  return fallback;
}

static inline const char *enum_token_name(int value, const enum_token_t *tokens,
                                          size_t count, const char *fallback) {
  if (!tokens) return fallback;
  for (size_t i = 0; i < count; i++) {
    if (tokens[i].value == value && tokens[i].name && tokens[i].name[0])
      return tokens[i].name;
  }
  return fallback;
}

#endif
