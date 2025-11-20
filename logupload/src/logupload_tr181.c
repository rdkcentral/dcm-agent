#include "logupload.h"
#include <rbus/rbus.h>
#include <string.h>

static bool tr181_fetch(const char* param, rbusValue_t* value_out) {
    rbusHandle_t handle;
    if(rbus_open(&handle, LOGUPLOAD_MODULE) != RBUS_ERROR_SUCCESS)
        return false;
    rbusValue_t value = NULL;
    bool ok = (rbus_get(handle, param, &value) == RBUS_ERROR_SUCCESS && value);
    rbus_close(handle);
    if(ok) *value_out = value;
    return ok;
}

bool tr181_get_string(const char* param, char* out, size_t out_sz) {
    if(!out || out_sz == 0) return false;
    out[0]=0; 
    rbusValue_t value=NULL;
    if(!tr181_fetch(param, &value)) return false;
    bool success=false;
    if(rbusValue_GetType(value)==RBUS_STRING) {
        const char* s = rbusValue_GetString(value,NULL); if(s){ strncpy(out,s,out_sz-1); out[out_sz-1]=0; success=true;}
    }
    rbusValue_Release(value); return success;
}
bool tr181_get_bool(const char* param, bool* out) {
    if(!out) return false; 
    rbusValue_t value=NULL;
    if(!tr181_fetch(param, &value)) return false;
    bool success=false;
    if(rbusValue_GetType(value)==RBUS_BOOLEAN) { *out = rbusValue_GetBoolean(value); success=true;}
    rbusValue_Release(value); return success;
}
