#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <stdio.h>
#include <climits>
#include <cerrno>
#include <fstream>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "dcm_cronparse.h"


// ======================= Mock Global Variables =======================
INT32 g_bMMEnable = 0;  // Define the missing global variable

// ======================= Mock Functions =======================
extern "C" {
    // Mock DCM Settings functions
    INT32 dcmSettingsInit(void** handle) {
        if (handle) {
            *handle = malloc(64);  // Allocate dummy memory
            return DCM_SUCCESS;
        }
        return DCM_FAILURE;
    }

    VOID dcmSettingsUnInit(void* handle) {
        if (handle) {
            free(handle);
        }
    }

    bool dcmSettingsGetMMFlag() {
        return (g_bMMEnable == 1);
    }

    INT8* dcmSettingsGetRDKPath(void* handle) {
        (void)handle;  // Suppress unused parameter warning
        static INT8 mockPath[] = "/opt/rdk";
        return mockPath;
    }

    INT8* dcmSettingsGetUploadProtocol(void* handle) {
        (void)handle;
        static INT8 mockProtocol[] = "HTTP";
        return mockProtocol;
    }

    INT8* dcmSettingsGetUploadURL(void* handle) {
        (void)handle;
        static INT8 mockURL[] = "http://mock.example.com/upload";
        return mockURL;
    }
}
