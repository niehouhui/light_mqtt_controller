#ifndef __cjson_handle_h__
#define __cjson_handle_h__

#include "cJSON.h"

typedef enum
{
    reset_led,
    reset_led_err,
    set_led,
    set_led_err,
    reset_all,
    reset_all_err,
} handle_result_t;

handle_result_t json_msg_handle(cJSON *json);

#endif