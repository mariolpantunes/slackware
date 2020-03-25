/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/mocks/mock_deferrable_deletion.h"
#include "gtest/gtest.h"

using namespace OCLRT;

const int threadCount = 4;

struct ClearQueueTest : public ::testing::Test,
                        public ::testing::WithParamInterface<int /*elements in queue*/> {

    void SetUp() override {
        threadStopped = 0;
        startClear = false;
        deleter.reset(new MockDeferredDeleter());
    }

    void TearDown() override {
        EXPECT_TRUE(deleter->isQueueEmpty());
        EXPECT_EQ(0, deleter->getElementsToRelease());
    }
    static void threadMethod(MockDeferredDeleter *deleter) {
        while (!startClear)
            ;
        deleter->clearQueue();
        threadStopped++;
    }
    MockDeferrableDeletion *createDeletion() {
        return new MockDeferrableDeletion();
    }
    std::unique_ptr<MockDeferredDeleter> deleter;
    static std::atomic<bool> startClear;
    static std::atomic<int> threadStopped;
};
std::atomic<bool> ClearQueueTest::startClear;
std::atomic<int> ClearQueueTest::threadStopped;

TEST_P(ClearQueueTest, clearQueueAfterFeed) {
    auto elementsInQueue = GetParam();
    EXPECT_EQ(0, deleter->clearCalled);
    for (int i = 0; i < elementsInQueue; i++) {
        deleter->DeferredDeleter::deferDeletion(createDeletion());
    }
    std::thread threads[threadCount];
    for (int i = 0; i < threadCount; i++) {
        threads[i] = std::thread(threadMethod, deleter.get());
    }
    EXPECT_EQ(0, deleter->clearCalled);
    EXPECT_EQ(elementsInQueue, deleter->getElementsToRelease());
    startClear = true;
    for (int i = 0; i < threadCount; i++) {
        threads[i].join();
    }
    EXPECT_EQ(threadCount, deleter->clearCalled);
    EXPECT_EQ(0, deleter->getElementsToRelease());
}

int paramsForClearQueueTest[] = {1, 10, 20, 50, 100};

INSTANTIATE_TEST_CASE_P(DeferredDeleterMtTests,
                        ClearQueueTest,
                        ::testing::ValuesIn(paramsForClearQueueTest));

class MyDeferredDeleter : public DeferredDeleter {
  public:
    bool isQueueEmpty() {
        std::lock_guard<std::mutex> lock(queueMutex);
        return queue.peekIsEmpty();
    }
    int getElementsToRelease() {
        return elementsToRelease;
    }
    bool isWorking() {
        return doWorkInBackground;
    }
    bool isThreadRunning() {
        return worker != nullptr;
    }
    int getClientsNum() {
        return numClients;
    }
    void forceSafeStop() {
        safeStop();
    }
};

struct DeferredDeleterMtTest : public ::testing::Test {

    void SetUp() override {
        deleter.reset(new MyDeferredDeleter());
    }

    void TearDown() override {
        EXPECT_TRUE(deleter->isQueueEmpty());
        EXPECT_EQ(0, deleter->getElementsToRelease());
    }

    void waitForAsyncThread() {
        while (!deleter->isWorking()) {
            std::this_thread::yield();
        }
    }
    std::unique_ptr<MyDeferredDeleter> deleter;
};

TEST_F(DeferredDeleterMtTest, asyncThreadsStopDeferredDeleter) {
    deleter->addClient();

    waitForAsyncThread();
    EXPECT_TRUE(deleter->isThreadRunning());
    EXPECT_TRUE(deleter->isWorking());

    // Start worker thread
    std::thread t([&]() {
        deleter->forceSafeStop();
        EXPECT_FALSE(deleter->isThreadRunning());
        EXPECT_FALSE(deleter->isWorking());
    });

    deleter->forceSafeStop();
    EXPECT_FALSE(deleter->isThreadRunning());
    EXPECT_FALSE(deleter->isWorking());

    t.join();

    deleter->removeClient();
    EXPECT_EQ(0, deleter->getClientsNum());
}
