#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "BlockingQueue.hpp"
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <sstream>

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

  auto th1 = std::thread(&Consume<int>, std::ref(queue), std::move(dataPromise), 10);
  auto th2 = std::thread(&Produce<int>, std::ref(queue), expectedData);

  auto consumerResult = fut.get();
  std::ostringstream stream;
  for(auto item: consumerResult)
  {
    stream << item << std::endl;
  }
  th1.join();
  th2.join();

  ASSERT_THAT(expectedData, ::testing::ContainerEq(consumerResult));
}
