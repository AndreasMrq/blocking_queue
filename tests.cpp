#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "BlockingQueue.hpp"
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <sstream>
#include <algorithm>
#include <functional> 

class ConstructorCounter{
  public:
    struct Counter
    {
      unsigned int copyConstructorCalls = 0;
      unsigned int copyAssignmentCalls = 0;
      unsigned int moveConstructorCalls = 0;
      unsigned int moveAssignmentCalls = 0;
    };

    Counter m_counter;

    ConstructorCounter() = default;

    ConstructorCounter(Counter counter) : m_counter{counter}
    {
    }

    ConstructorCounter(const ConstructorCounter& other) // II. copy constructor
      : ConstructorCounter(other.m_counter)
    {
      ++m_counter.copyConstructorCalls;
    }

    ConstructorCounter& operator=(const ConstructorCounter& other) // III. copy assignment
    {
      if (this == &other)
        return *this;

      ConstructorCounter temp(other.m_counter); // use the copy constructor
      std::swap(m_counter, temp.m_counter); // exchange the underlying resource
      ++m_counter.copyAssignmentCalls;

      return *this;
    }

    ConstructorCounter(ConstructorCounter&& other) noexcept // IV. move constructor
      : m_counter(std::exchange(other.m_counter, Counter{}))
      {
        ++m_counter.moveConstructorCalls;
      }

    ConstructorCounter& operator=(ConstructorCounter&& other) noexcept // V. move assignment
    {
      ConstructorCounter temp(std::move(other));
      std::swap(m_counter, temp.m_counter);
      ++m_counter.moveAssignmentCalls;
      return *this;
    }
};

using namespace std::literals::chrono_literals;

TEST(BlockingQueueTests, EmptyQueueIsEmpty) 
{
  BlockingQueue<int> bq;

  const auto isEmpty = bq.IsEmpty();

  ASSERT_TRUE(isEmpty);
}

TEST(BlockingQueueTests, AddItemQueueNotEmpty) 
{
  BlockingQueue<int> bq;

  bq.Push(1);
  const auto isEmpty = bq.IsEmpty();

  ASSERT_FALSE(isEmpty);
}

TEST(BlockingQueueTests, TryPopReturnsNulloptForEmpty) 
{
  BlockingQueue<int> bq;
  ASSERT_TRUE(bq.IsEmpty());

  const auto result = bq.TryPop();

  ASSERT_FALSE(result.has_value());
}

TEST(BlockingQueueTests, WaitAndTryPopReturnsNullopt) 
{
  BlockingQueue<int> bq;
  ASSERT_TRUE(bq.IsEmpty());

  const auto result = bq.WaitAndTryPop(1us);

  ASSERT_FALSE(result.has_value());
}

TEST(BlockingQueueTests, GivenElementAlreadyInQueue_WhenWaitAndTryPop_DoesNotWait) 
{
  BlockingQueue<int> bq;
  ASSERT_TRUE(bq.IsEmpty());

  bq.Push(1);
  const auto tic = std::chrono::steady_clock::now();
  const auto result = bq.WaitAndTryPop(1s);
  const auto toc = std::chrono::steady_clock::now();

  ASSERT_TRUE(toc-tic < 0.5s);
  ASSERT_EQ(*result,1);
}

TEST(BlockingQueueEfficiencyTests, GivenEmplace_WhenBlockAndPop_NoCopyOperationsUsed)
{
  BlockingQueue<ConstructorCounter> bq;
  ASSERT_TRUE(bq.IsEmpty());

  bq.Emplace();

  ConstructorCounter result{bq.BlockAndPop()};

  ASSERT_EQ(result.m_counter.copyAssignmentCalls, 0);
  ASSERT_EQ(result.m_counter.moveAssignmentCalls, 0);
  ASSERT_EQ(result.m_counter.copyConstructorCalls, 0);
  ASSERT_EQ(result.m_counter.moveConstructorCalls, 1);
}

TEST(BlockingQueueEfficiencyTests, GivenEmplace_WhenTryPop_NoCopyOperationsUsed)
{
  BlockingQueue<ConstructorCounter> bq;
  ASSERT_TRUE(bq.IsEmpty());

  bq.Emplace();

  auto result{bq.TryPop()};

  ASSERT_EQ(result->m_counter.copyAssignmentCalls, 0);
  ASSERT_EQ(result->m_counter.moveAssignmentCalls, 0);
  ASSERT_EQ(result->m_counter.copyConstructorCalls, 0);
  ASSERT_EQ(result->m_counter.moveConstructorCalls, 2);
}

TEST(BlockingQueueEfficiencyTests, GivenEmplace_WhenWaitAndTryPop_NoCopyOperationsUsed)
{
  BlockingQueue<ConstructorCounter> bq;
  ASSERT_TRUE(bq.IsEmpty());

  bq.Emplace();

  auto result{bq.WaitAndTryPop(std::chrono::microseconds(1))};

  ASSERT_EQ(result->m_counter.copyAssignmentCalls, 0);
  ASSERT_EQ(result->m_counter.moveAssignmentCalls, 0);
  ASSERT_EQ(result->m_counter.copyConstructorCalls, 0);
  ASSERT_EQ(result->m_counter.moveConstructorCalls, 2);
}

TEST(BlockingQueueEfficiencyTests, GivenPushRValue_WhenBlockAndPop_NoCopyOperationsUsed)
{
  BlockingQueue<ConstructorCounter> bq;
  ASSERT_TRUE(bq.IsEmpty());

  ConstructorCounter counter;
  bq.Push(std::move(counter));

  ConstructorCounter result{bq.BlockAndPop()};

  ASSERT_EQ(result.m_counter.copyAssignmentCalls, 0);
  ASSERT_EQ(result.m_counter.moveAssignmentCalls, 0);
  ASSERT_EQ(result.m_counter.copyConstructorCalls, 0);
  ASSERT_EQ(result.m_counter.moveConstructorCalls, 2);
}

TEST(BlockingQueueEfficiencyTests, GivenPushRValue_WhenBlockAndPopWithToken_NoCopyOperationsUsed)
{
  BlockingQueue<ConstructorCounter> bq;
  ASSERT_TRUE(bq.IsEmpty());

  ConstructorCounter counter;
  bq.Push(std::move(counter));
  auto stop_source = std::stop_source{};

  auto resultOpt = bq.BlockAndPop(stop_source.get_token());

  ASSERT_TRUE(resultOpt);
  ASSERT_EQ(resultOpt->m_counter.copyAssignmentCalls, 0);
  ASSERT_EQ(resultOpt->m_counter.moveAssignmentCalls, 0);
  ASSERT_EQ(resultOpt->m_counter.copyConstructorCalls, 0);
  ASSERT_EQ(resultOpt->m_counter.moveConstructorCalls, 3);
}

TEST(BlockingQueueEfficiencyTests, GivenPushLValue_WhenBlockAndPop_OneCopyOperationsUsed)
{
  BlockingQueue<ConstructorCounter> bq;
  ASSERT_TRUE(bq.IsEmpty());

  ConstructorCounter counter;
  bq.Push(counter);

  ConstructorCounter result{bq.BlockAndPop()};

  ASSERT_EQ(result.m_counter.copyAssignmentCalls, 0);
  ASSERT_EQ(result.m_counter.moveAssignmentCalls, 0);
  ASSERT_EQ(result.m_counter.copyConstructorCalls, 1);
  ASSERT_EQ(result.m_counter.moveConstructorCalls, 1);
}

TEST(BlockingQueueEfficiencyTests, GivenPushLValue_WhenBlockAndPopWithToken_OneCopyOperationsUsed)
{
  BlockingQueue<ConstructorCounter> bq;
  ASSERT_TRUE(bq.IsEmpty());

  ConstructorCounter counter;
  bq.Push(counter);
  auto stop_source = std::stop_source{};

  auto resultOpt = bq.BlockAndPop(stop_source.get_token());

  ASSERT_TRUE(resultOpt);
  ASSERT_EQ(resultOpt->m_counter.copyAssignmentCalls, 0);
  ASSERT_EQ(resultOpt->m_counter.moveAssignmentCalls, 0);
  ASSERT_EQ(resultOpt->m_counter.copyConstructorCalls, 1);
  ASSERT_EQ(resultOpt->m_counter.moveConstructorCalls, 2);
}

  template<typename T>
void Consume(BlockingQueue<T> &queue, std::promise<std::vector<T>> &&resultPromise, std::size_t dataSize)
{
  std::vector<T> result{};
  while(result.size() < 10)
  {
    auto item = queue.BlockAndPop();
    result.emplace_back(item);
  }
  resultPromise.set_value(result);
}

  template<typename T>
void ConsumeBlocking(std::stop_token stopToken, BlockingQueue<T> &queue, std::promise<std::vector<T>> &&resultPromise)
{
  std::vector<T> result{};
  while(!stopToken.stop_requested())
  {
    auto item = queue.BlockAndPop(stopToken);
    if(item.has_value())
    {
      result.emplace_back(*item);
    }
  }
  resultPromise.set_value(result);
}

  template<typename T>
void Produce(BlockingQueue<T> &queue, std::vector<T> testData)
{
  for(const auto &item: testData)
  {
    queue.Push(item);
  }
}

TEST(BlockingQueueTests, GivenProducerAndConsumer_WhenBlockAndPop_CorrectItemsTakenFromQueue)
{
  std::promise<std::vector<int>> dataPromise;
  auto fut = dataPromise.get_future();
  BlockingQueue<int> queue;
  std::vector<int> expectedData{1,2,3,4,5,6,7,8,9,10};

  auto th1 = std::jthread(&Consume<int>, std::ref(queue), std::move(dataPromise), 10);
  auto th2 = std::jthread(&Produce<int>, std::ref(queue), expectedData);

  auto consumerResult = fut.get();

  ASSERT_THAT(expectedData, ::testing::ContainerEq(consumerResult));
}

TEST(BlockingQueueTests, GivenMultipleProducerAndSingleConsumer_WhenBlockAndPop_CorrectItemsTakenFromQueue)
{
  std::promise<std::vector<int>> dataPromise;
  auto fut = dataPromise.get_future();
  BlockingQueue<int> queue;
  std::vector<int> expectedData{1,2,3,4,5,6,7,8,9,10};

  auto th1 = std::jthread(&Consume<int>, std::ref(queue), std::move(dataPromise), 10);
  auto th2 = std::jthread(&Produce<int>, std::ref(queue), std::vector<int>{1,2,3});
  auto th3 = std::jthread(&Produce<int>, std::ref(queue), std::vector<int>{4,5,6});
  auto th4 = std::jthread(&Produce<int>, std::ref(queue), std::vector<int>{7,8});
  auto th5 = std::jthread(&Produce<int>, std::ref(queue), std::vector<int>{9,10});

  auto consumerResult = fut.get();
  std::sort(std::begin(consumerResult), std::end(consumerResult));

  ASSERT_THAT(expectedData, ::testing::ContainerEq(consumerResult)) << "hello"; 
}

TEST(BlockingQueueThreadingTests, GivenBlockingConsumer_WhenStopRequested_ThenStops)
{
  std::promise<std::vector<int>> dataPromise;
  auto fut = dataPromise.get_future();
  BlockingQueue<int> queue;
  std::vector<int> expectedData{1,2,3,4,5,6,7,8,9,10};

  auto th1 = std::jthread([&queue, promise = std::move(dataPromise)](std::stop_token token) mutable {ConsumeBlocking<int>(token, queue, std::move(promise));});
  auto th2 = std::jthread(&Produce<int>, std::ref(queue), expectedData);

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  th1.request_stop();

  auto consumerResult = fut.get();
  ASSERT_THAT(expectedData, ::testing::ContainerEq(consumerResult));
}
