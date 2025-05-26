/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "MultiViewBuffer_test"
//#define LOG_NDEBUG 0

#include <gtest/gtest.h>

#include "MockConsumer.h"

#include <android/hardware_buffer.h>
#include <gui/BufferItem.h>
#include <gui/BufferItemConsumer.h>
#include <gui/BufferQueue.h>
#include <gui/IProducerListener.h>
#include <gui/Surface.h>
#include <private/android/AHardwareBufferHelpers.h>
#include <ui/GraphicBuffer.h>
#include <utils/Log.h>
#include <vndk/hardware_buffer.h>

#include <string.h>
#include <string>

namespace android {
class MultiViewBufferTest : public ::testing::Test {
public:
protected:
    sp<IGraphicBufferProducer> mProducer;
    sp<IGraphicBufferConsumer> mConsumer;
};

#define TEST_STRING_LEFT "LEFT_VIEW_"
#define TEST_STRING_RIGHT "RIGHT_VIEW_"

TEST_F(MultiViewBufferTest, MultiViewBufferProduceAndConsume) {
    const uint64_t GRALLOC_USAGE_PRIVATE_MULTIVIEW = 1ULL << 58;
    const uint64_t TEST_PRODUCE_USAGE_BITS =
            GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_PRIVATE_MULTIVIEW;
    BufferQueue::createBufferQueue(&mProducer, &mConsumer);
    sp<MockConsumer> mc = new MockConsumer();
    mConsumer->consumerConnect(mc, false);
    IGraphicBufferProducer::QueueBufferOutput output;
    mProducer->connect(new StubProducerListener, NATIVE_WINDOW_API_CPU, false, &output);

    // Producer dequeue buffers
    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> producerBuffer;
    status_t result = mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0, TEST_PRODUCE_USAGE_BITS,
                                               nullptr, nullptr);
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION, result)
            << "Failed to dequeue buffer: " << result;
    result = mProducer->requestBuffer(slot, &producerBuffer);
    ASSERT_EQ(OK, result) << "Failed to request buffer: " << result;

    AHardwareBuffer* producerBufferAHB = AHardwareBuffer_from_GraphicBuffer(producerBuffer.get());

    // 1. AHardwareBuffer_getBaseView
    uint32_t baseView = 0;
    result = AHardwareBuffer_getBaseView(producerBufferAHB, &baseView);
    ASSERT_EQ(result, NO_ERROR) << "Failed to get base view: " << result;
    EXPECT_EQ(baseView, static_cast<uint32_t>(VIEW_LEFT)); // Assuming VIEW_LEFT is the base view
    ALOGD("AHardwareBuffer_getBaseView successful. Base view: %u", baseView);

    // 2. AHardwareBuffer_getAuxiliaryBuffer
    AHardwareBuffer* producerBufferAuxAHB =
            AHardwareBuffer_getAuxiliaryBuffer(producerBufferAHB, VIEW_MASK_RIGHT);
    ASSERT_NE(producerBufferAuxAHB, nullptr);
    ALOGD("AHardwareBuffer_getAuxiliaryBuffer successful. Auxiliary buffer: %p",
          producerBufferAuxAHB);

    // Validate auxiliary buffer properties
    AHardwareBuffer_Desc auxDesc;
    AHardwareBuffer_describe(producerBufferAuxAHB, &auxDesc);
    EXPECT_EQ(auxDesc.width, producerBuffer->getWidth());
    EXPECT_EQ(auxDesc.height, producerBuffer->getHeight());
    EXPECT_EQ(auxDesc.format, static_cast<uint32_t>(producerBuffer->getPixelFormat()));
    ALOGD("Auxiliary buffer properties validated successfully. Width: %u, Height: %u, Format: %u",
          auxDesc.width, auxDesc.height, auxDesc.format);

    // 3. AHardwareBuffer_getAuxiliaryViewInfo
    size_t numberOfViewsProducer = 0;
    BufferView* viewListProducer = nullptr;
    status_t statusProducer =
            AHardwareBuffer_getAuxiliaryViewInfo(producerBufferAHB,
                                                 reinterpret_cast<uint32_t*>(viewListProducer),
                                                 &numberOfViewsProducer);
    EXPECT_EQ(statusProducer, 0);
    ASSERT_EQ(numberOfViewsProducer, 2u);

    viewListProducer = new BufferView[numberOfViewsProducer];
    statusProducer =
            AHardwareBuffer_getAuxiliaryViewInfo(producerBufferAHB,
                                                 reinterpret_cast<uint32_t*>(viewListProducer),
                                                 &numberOfViewsProducer);
    EXPECT_EQ(statusProducer, 0);
    ASSERT_NE(nullptr, viewListProducer);
    EXPECT_EQ(viewListProducer[0], 1u);
    EXPECT_EQ(viewListProducer[1], 2u);

    uint32_t viewsProducer = 0;
    for (size_t i = 0; i < numberOfViewsProducer; ++i) {
        viewsProducer |= viewListProducer[i];
    }
    EXPECT_GT(viewsProducer, VIEW_MASK_LEFT);

    ALOGD("Producer: AHardwareBuffer_getAuxiliaryViewInfo successful. Views bitmask: 0x%x, Number "
          "of views: %zu",
          viewsProducer, numberOfViewsProducer);
    for (size_t i = 0; i < numberOfViewsProducer; ++i) {
        ALOGD("ViewProducer[%zu] = 0x%x", i, viewListProducer[i]);
    }

    // Write test data to buffers
    std::string left_producer = TEST_STRING_LEFT;
    std::string right_producer = TEST_STRING_RIGHT;
    // write left buffer
    char* dataIn;
    ASSERT_EQ(OK,
              AHardwareBuffer_lock(producerBufferAHB, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                                   nullptr, reinterpret_cast<void**>(&dataIn)));
    strcpy(dataIn, left_producer.c_str());
    // write right buffer
    char* dataInAux;
    ASSERT_EQ(OK,
              AHardwareBuffer_lock(producerBufferAuxAHB, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                                   nullptr, reinterpret_cast<void**>(&dataInAux)));
    strcpy(dataInAux, right_producer.c_str());
    ASSERT_EQ(OK, AHardwareBuffer_unlock(producerBufferAHB, nullptr));
    ASSERT_EQ(OK, AHardwareBuffer_unlock(producerBufferAuxAHB, nullptr));

    // Producer queue buffers
    IGraphicBufferProducer::QueueBufferInput input(0, false, HAL_DATASPACE_UNKNOWN,
                                                   Rect(0, 0, 1, 1),
                                                   NATIVE_WINDOW_SCALING_MODE_FREEZE, 0,
                                                   Fence::NO_FENCE);
    ASSERT_EQ(OK, mProducer->queueBuffer(slot, input, &output));

    // Consumer acquire buffers
    BufferItem item;
    ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, static_cast<nsecs_t>(0)));
    sp<GraphicBuffer> consumerBuffer = item.mGraphicBuffer;
    AHardwareBuffer* consumerBufferAHB = AHardwareBuffer_from_GraphicBuffer(consumerBuffer.get());

    // Get auxiliary view masks
    size_t numberOfViewsConsumer = 0;
    BufferView* viewListConsumer = nullptr;
    status_t statusConsumer =
            AHardwareBuffer_getAuxiliaryViewInfo(consumerBufferAHB,
                                                 reinterpret_cast<uint32_t*>(viewListConsumer),
                                                 &numberOfViewsConsumer);
    EXPECT_EQ(statusConsumer, 0);
    ASSERT_EQ(numberOfViewsConsumer, 2u);

    viewListConsumer = new BufferView[numberOfViewsConsumer];
    statusConsumer =
            AHardwareBuffer_getAuxiliaryViewInfo(consumerBufferAHB,
                                                 reinterpret_cast<uint32_t*>(viewListConsumer),
                                                 &numberOfViewsConsumer);
    EXPECT_EQ(statusConsumer, 0);
    ASSERT_NE(nullptr, viewListConsumer);
    EXPECT_EQ(viewListConsumer[0], 1u);
    EXPECT_EQ(viewListConsumer[1], 2u);

    uint32_t viewsConsumer = 0;
    for (size_t i = 0; i < numberOfViewsConsumer; ++i) {
        viewsConsumer |= viewListConsumer[i];
    }
    EXPECT_GT(viewsConsumer, VIEW_MASK_LEFT);

    ALOGD("AHardwareBuffer_getAuxiliaryViewInfo successful. Views bitmask: 0x%x, Number of views: "
          "%zu",
          viewsConsumer, numberOfViewsConsumer);
    for (size_t i = 0; i < numberOfViewsConsumer; ++i) {
        ALOGD("ViewConsumer[%zu] = 0x%x", i, viewListConsumer[i]);
    }

    // Get auxiliary view buffer
    AHardwareBuffer* consumerBufferAuxAHB =
            AHardwareBuffer_getAuxiliaryBuffer(consumerBufferAHB, VIEW_MASK_RIGHT);
    ASSERT_NE(consumerBufferAuxAHB, nullptr);
    ALOGD("AHardwareBuffer_getAuxiliaryBuffer successful. Auxiliary buffer: %p",
          consumerBufferAuxAHB);

    // Read buffers
    std::string left_consumer = TEST_STRING_LEFT;
    std::string right_consumer = TEST_STRING_RIGHT;
    // read left buffer
    char* dataOut;
    ASSERT_EQ(OK,
              AHardwareBuffer_lock(consumerBufferAHB, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1,
                                   nullptr, reinterpret_cast<void**>(&dataOut)));
    ASSERT_EQ(0, strcmp(dataOut, left_consumer.c_str()));
    // read right buffer
    char* dataOutAux;
    ASSERT_EQ(OK,
              AHardwareBuffer_lock(consumerBufferAuxAHB, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1,
                                   nullptr, reinterpret_cast<void**>(&dataOutAux)));
    ASSERT_EQ(0, strcmp(dataOutAux, right_consumer.c_str()));
    ASSERT_EQ(OK, AHardwareBuffer_unlock(consumerBufferAHB, nullptr));
    ASSERT_EQ(OK, AHardwareBuffer_unlock(consumerBufferAuxAHB, nullptr));

    // Consumer release buffers
    ASSERT_EQ(OK,
              mConsumer->releaseBuffer(item.mSlot, item.mFrameNumber, EGL_NO_DISPLAY,
                                       EGL_NO_SYNC_KHR, Fence::NO_FENCE));
}
} // namespace android