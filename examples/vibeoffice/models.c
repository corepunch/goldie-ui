#include "models.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build/generated/examples/vibeoffice/vibeoffice.h"

static const vibe_model_info_t g_models[] = {
  { 1, "MiMo V2.5 Free", "opencode/mimo-v2.5-free" },
  { 2, "Qwen 3.6 35B",   "lmstudio/qwen/qwen3.6-35b-a3b" },
  { 3, "Gemma 4 31B",    "lmstudio/google/gemma-4-31b-qat" },
  { 4, "Gemma 4 E4B",    "lmstudio/google/gemma-4-e4b" },
  { 5,  "Claude Fable 5",          "opencode/claude-fable-5" },
  { 6,  "Claude Opus 4.8",         "opencode/claude-opus-4-8" },
  { 7,  "Claude Opus 4.7",         "opencode/claude-opus-4-7" },
  { 8,  "Claude Opus 4.6",         "opencode/claude-opus-4-6" },
  { 9,  "Claude Opus 4.5",         "opencode/claude-opus-4-5" },
  { 10, "Claude Opus 4.1",         "opencode/claude-opus-4-1" },
  { 11, "Claude Sonnet 5",         "opencode/claude-sonnet-5" },
  { 12, "Claude Sonnet 4.6",       "opencode/claude-sonnet-4-6" },
  { 13, "Claude Sonnet 4.5",       "opencode/claude-sonnet-4-5" },
  { 14, "Claude Sonnet 4",         "opencode/claude-sonnet-4" },
  { 15, "Claude Haiku 4.5",        "opencode/claude-haiku-4-5" },
  { 16, "Gemini 3.5 Flash",        "opencode/gemini-3.5-flash" },
  { 17, "Gemini 3.1 Pro",          "opencode/gemini-3.1-pro" },
  { 18, "Gemini 3 Flash",          "opencode/gemini-3-flash" },
  { 19, "GPT 5.6 Sol",             "opencode/gpt-5.6-sol" },
  { 20, "GPT 5.6 Terra",           "opencode/gpt-5.6-terra" },
  { 21, "GPT 5.6 Luna",            "opencode/gpt-5.6-luna" },
  { 22, "GPT 5.5",                 "opencode/gpt-5.5" },
  { 23, "GPT 5.5 Pro",             "opencode/gpt-5.5-pro" },
  { 24, "GPT 5.4",                 "opencode/gpt-5.4" },
  { 25, "GPT 5.4 Pro",             "opencode/gpt-5.4-pro" },
  { 26, "GPT 5.4 Mini",            "opencode/gpt-5.4-mini" },
  { 27, "GPT 5.4 Nano",            "opencode/gpt-5.4-nano" },
  { 28, "GPT 5.3 Codex Spark",     "opencode/gpt-5.3-codex-spark" },
  { 29, "GPT 5.3 Codex",           "opencode/gpt-5.3-codex" },
  { 30, "GPT 5.2",                 "opencode/gpt-5.2" },
  { 31, "GPT 5.2 Codex",           "opencode/gpt-5.2-codex" },
  { 32, "GPT 5.1",                 "opencode/gpt-5.1" },
  { 33, "GPT 5.1 Codex Max",       "opencode/gpt-5.1-codex-max" },
  { 34, "GPT 5.1 Codex",           "opencode/gpt-5.1-codex" },
  { 35, "GPT 5.1 Codex Mini",      "opencode/gpt-5.1-codex-mini" },
  { 36, "GPT 5",                   "opencode/gpt-5" },
  { 37, "GPT 5 Codex",             "opencode/gpt-5-codex" },
  { 38, "GPT 5 Nano",              "opencode/gpt-5-nano" },
  { 39, "Grok Build 0.1",          "opencode/grok-build-0.1" },
  { 40, "Grok 4.5",                "opencode/grok-4.5" },
  { 41, "DeepSeek V4 Pro",         "opencode/deepseek-v4-pro" },
  { 42, "DeepSeek V4 Flash",       "opencode/deepseek-v4-flash" },
  { 43, "GLM 5.2",                 "opencode/glm-5.2" },
  { 44, "GLM 5.1",                 "opencode/glm-5.1" },
  { 45, "GLM 5",                   "opencode/glm-5" },
  { 46, "MiniMax M3",              "opencode/minimax-m3" },
  { 47, "MiniMax M2.7",            "opencode/minimax-m2.7" },
  { 48, "MiniMax M2.5",            "opencode/minimax-m2.5" },
  { 49, "Kimi K2.7 Code",          "opencode/kimi-k2.7-code" },
  { 50, "Kimi K2.6",               "opencode/kimi-k2.6" },
  { 51, "Kimi K2.5",               "opencode/kimi-k2.5" },
  { 52, "Qwen3.6 Plus",            "opencode/qwen3.6-plus" },
  { 53, "Qwen3.5 Plus",            "opencode/qwen3.5-plus" },
  { 54, "Big Pickle",              "opencode/big-pickle" },
  { 55, "DeepSeek V4 Flash Free",  "opencode/deepseek-v4-flash-free" },
  { 56, "HY3 Free",                "opencode/hy3-free" },
  { 57, "Nemotron 3 Ultra Free",   "opencode/nemotron-3-ultra-free" },
  { 58, "North Mini Code Free",    "opencode/north-mini-code-free" },
};

enum { MODEL_COL_ID, MODEL_COL_NAME, MODEL_COL_OPENCODE_ID };

static const db_field_msg_binding_t g_model_bindings[] = {
  { "id", MODEL_COL_ID }, { "name", MODEL_COL_NAME }, { "model_id", MODEL_COL_OPENCODE_ID },
};

int vibe_model_count(void) { return (int)ARRAY_LEN(g_models); }

const vibe_model_info_t *vibe_model_at(int index) {
  return index >= 0 && index < vibe_model_count() ? &g_models[index] : NULL;
}

const vibe_model_info_t *vibe_model_by_id(int id) {
  for (int i = 0; i < vibe_model_count(); i++) if (g_models[i].id == id) return &g_models[i];
  return NULL;
}

const vibe_model_info_t *vibe_model_by_opencode_id(const char *opencode_id) {
  if (!opencode_id) return NULL;
  for (int i = 0; i < vibe_model_count(); i++)
    if (!strcmp(g_models[i].opencode_id, opencode_id)) return &g_models[i];
  return NULL;
}

static result_t model_object_proc(const void *object, uint32_t msg,
                                  uint32_t wparam, void *lparam) {
  const vibe_model_info_t *model = (const vibe_model_info_t *)object;
  char *buf = (char *)lparam; size_t size = HIWORD(wparam);
  if (!model || msg != dbObjGetFieldText || !buf || !size) return false;
  switch (LOWORD(wparam)) {
    case MODEL_COL_ID:          snprintf(buf, size, "%d", model->id); return true;
    case MODEL_COL_NAME:        snprintf(buf, size, "%s", model->name); return true;
    case MODEL_COL_OPENCODE_ID: snprintf(buf, size, "%s", model->opencode_id); return true;
    default: return false;
  }
}

static lresult_t vibe_model_database(database_t *db, uint32_t msg,
                                     uint32_t wparam, void *lparam) {
  (void)db;
  switch (msg) {
    case dbCreate: case dbLoad: case dbDestroy: return 1;
    case dbFetch: {
      if (LOWORD(wparam) != TABLE_MODELS) return 0;
      result_node_t *head = NULL, *tail = NULL;
      for (int i = 0; i < vibe_model_count(); i++) {
        result_node_t *node = malloc(sizeof(*node) + sizeof(vibe_model_info_t *));
        if (!node) { free_result_list(head); return 0; }
        node->next = NULL; *(const vibe_model_info_t **)node->data = &g_models[i];
        if (tail) tail->next = node; else head = node; tail = node;
      }
      return (lresult_t)head;
    }
    case dbGetObjectProc: return LOWORD(wparam) == TABLE_MODELS ? (lresult_t)model_object_proc : 0;
    case dbGetFieldBindings:
      if (LOWORD(wparam) != TABLE_MODELS) return 0;
      if (lparam) *(int *)lparam = (int)ARRAY_LEN(g_model_bindings);
      return (lresult_t)g_model_bindings;
    default: return 0;
  }
}

database_t *vibe_models_create(void) {
  register_database_class(&(db_class_desc_t){ "vibe_model_database", vibe_model_database });
  return create_database("models", "vibe_model_database", NULL);
}
