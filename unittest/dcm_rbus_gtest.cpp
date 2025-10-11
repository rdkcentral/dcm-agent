#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "./mocks/mockrbus.h‎"

extern "C" {
#include "dcm_rbus.h"
#include "dcm_types.h"
}
#include "dcm_rbus.c"
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::StrictMock;

class DcmRbusTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockRBus = new StrictMock<MockRBus>();
        mock_rbus_set_global_mock(mockRBus);
        mock_rbus_reset();
    }
    
    void TearDown() override {
        mock_rbus_clear_global_mock();
        delete mockRBus;
    }
    
    MockRBus* mockRBus;
};

TEST_F(DcmRbusTest, dcmRbusInit_Success) {
    void* handle = nullptr;
    rbusHandle_t mockHandle = mock_rbus_get_mock_handle();
    
    EXPECT_CALL(*mockRBus, rbus_checkStatus())
        .WillOnce(Return(RBUS_ENABLED));
    
    EXPECT_CALL(*mockRBus, rbus_open(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(mockHandle), Return(RBUS_ERROR_SUCCESS)));
    
    int result = dcmRbusInit(&handle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
    EXPECT_NE(handle, nullptr);
    
    // Cleanup
    if (handle) {
        EXPECT_CALL(*mockRBus, rbusEvent_Unsubscribe(_, _))
            .Times(2)
            .WillRepeatedly(Return(RBUS_ERROR_SUCCESS));
        EXPECT_CALL(*mockRBus, rbus_unregDataElements(_, _, _))
            .WillOnce(Return(RBUS_ERROR_SUCCESS));
        EXPECT_CALL(*mockRBus, rbus_close(_))
            .WillOnce(Return(RBUS_ERROR_SUCCESS));
        
        dcmRbusUnInit(handle);
    }
}

TEST_F(DcmRbusTest, dcmRbusSendEvent_Success) {
    // Setup mock DCM handle
    DCMRBusHandle dcmHandle;
    dcmHandle.pRbusHandle = mock_rbus_get_mock_handle();
    
    EXPECT_CALL(*mockRBus, rbusValue_Init(_))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusValue_SetString(_, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusObject_Init(_, _))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusObject_SetValue(_, _, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusEvent_Publish(_, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusValue_Release(_))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusObject_Release(_))
        .Times(1);
    
    // Set global event subscription flag
    extern int g_eventsub;
    g_eventsub = 1;
    
    int result = dcmRbusSendEvent(&dcmHandle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
}
