#pragma once
#include "ssr.h"

typedef struct {
    void (*enter)(void);
    void (*run)(ssr_control_msg_t* msg, bool is_new);
    void (*exit)(void);
    const char* name;
} ssr_handler_t;

extern const ssr_handler_t off_handler;
extern const ssr_handler_t burst_handler;
extern const ssr_handler_t netzero_handler;
extern const ssr_handler_t softzero_handler;
