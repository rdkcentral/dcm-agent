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

extern "C" {
    #include "dcm_types.h"
    #include "dcm_cronparse.h"
    #include "dcm.h"
    #include "dcm_utils.h"
}

using namespace testing;
using namespace std;

// ======================= Mock Global Variables =======================
INT32 g_bMMEnable = 0;

// ======================= DCM Settings Mock Class =======================
class MockDcmSettings {
public:
    MOCK_METHOD(INT32, dcmSettingsInit, (void** handle), ());
    MOCK_METHOD(void, dcmSettingsUnInit, (void* handle), ());
    MOCK_METHOD(bool, dcmSettingsGetMMFlag, (), ());
    MOCK_METHOD(INT8*, dcmSettingsGetRDKPath, (void* handle), ());
    MOCK_METHOD(INT8*, dcmSettingsGetUploadProtocol, (void* handle), ());
    MOCK_METHOD(INT8*, dcmSettingsGetUploadURL, (void* handle), ());
    MOCK_METHOD(INT32, dcmSettingParseConf, (void* handle, const INT8* confPath, INT8* logCron, INT8* difdCron), ());
    MOCK_METHOD(INT32, dcmSettingDefaultBoot, (), ());
};

// ======================= Global Mock Instances =======================
MockDcmSettings* g_mockSettings = nullptr;


// ======================= C Function Wrappers =======================
extern "C" {
    // DCM Settings functions
    INT32 dcmSettingsInit(void** handle) {
        return g_mockSettings ? g_mockSettings->dcmSettingsInit(handle) : DCM_SUCCESS;
    }

    VOID dcmSettingsUnInit(void* handle) {
        if (g_mockSettings) g_mockSettings->dcmSettingsUnInit(handle);
    }

    INT32 dcmSettingsGetMMFlag() {
        return g_mockSettings ? g_mockSettings->dcmSettingsGetMMFlag() : DCM_SUCCESS;
    }

    INT8* dcmSettingsGetRDKPath(void* handle) {
        return g_mockSettings ? g_mockSettings->dcmSettingsGetRDKPath(handle) : (INT8*)"/opt/rdk";
    }

    INT8* dcmSettingsGetUploadProtocol(void* handle) {
        return g_mockSettings ? g_mockSettings->dcmSettingsGetUploadProtocol(handle) : (INT8*)"HTTP";
    }

    INT8* dcmSettingsGetUploadURL(void* handle) {
        return g_mockSettings ? g_mockSettings->dcmSettingsGetUploadURL(handle) : (INT8*)"http://mock.example.com/upload";
    }

    INT32 dcmSettingParseConf(void* handle, const INT8* confPath, INT8* logCron, INT8* difdCron) {
        return g_mockSettings ? g_mockSettings->dcmSettingParseConf(handle, confPath, logCron, difdCron) : DCM_SUCCESS;
    }

    INT32 dcmSettingDefaultBoot() {
        return g_mockSettings ? g_mockSettings->dcmSettingDefaultBoot() : DCM_SUCCESS;
    }

}

/*
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
extern "C" {
#include "dcm_cronparse.h"
#include "dcm_utils.h"
}

// ======================= Mock Global Variables =======================
INT32 g_bMMEnable = 0;  // Define the missing global variable

// ======================= Mock Functions =======================
extern "C" {
    // Mock DCM Settings functions
    INT32 dcmSettingsInit(void** handle) {
        return DCM_FAILURE;
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

    INT32 dcmSettingsGetMMFlag() {
        if(g_bMMEnable == 0)
           return DCM_SUCCESS;
        else
           return DCM_FAILURE;
           
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
*/
