#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "BlockingQueue.hpp"
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <sstream>
#include <algorithm>

class ConstructionTest{
  public:
    struct Calls
    {
    unsigned int destructorCalls = 0;
    unsigned int copyConstructorCalls = 0;
    unsigned int copyAssignmentCalls = 0;
    unsigned int moveConstructorCalls = 0;
    unsigned int moveAssignmentCalls = 0;
    };

    Calls m_counter;

    ConstructionTest(Calls calls)
    {
      m_counter = calls;
    }
  ~ConstructionTest() // I. destructor
  {
    ++m_counter.destructorCalls;
  }

  ConstructionTest(const ConstructionTest& other) // II. copy constructor
                                                  : ConstructionTest(other.m_counter)
  {
    ++m_counter.copyConstructorCalls;
  }

  ConstructionTest& operator=(const ConstructionTest& other) // III. copy assignment
  {
    if (this == &other)
      return *this;

    ConstructionTest temp(other.m_counter); // use the copy constructor
    std::swap(m_counter, temp.m_counter); // exchange the underlying resource
    ++m_counter.copyAssignmentCalls;

    return *this;
  }

  ConstructionTest(ConstructionTest&& other) noexcept // IV. move constructor
    : cstring(std::exchange(other.cstring, nullptr))
    {
    }

  ConstructionTest& operator=(ConstructionTest&& other) noexcept // V. move assignment
  {
    ConstructionTest temp(std::move(other));
    std::swap(cstring, temp.cstring);
    return *this;
  } ~ConstructionTest() // I. destructor
  {
    delete[] cstring; // deallocate
  }

  ConstructionTest(const ConstructionTest& other) // II. copy constructor
    : ConstructionTest(other.cstring)
  {
  }

  ConstructionTest& operator=(const ConstructionTest& other) // III. copy assignment
  {
    if (this == &other)
      return *this;

    ConstructionTest temp(other); // use the copy constructor
    std::swap(cstring, temp.cstring); // exchange the underlying resource

    return *this;
  }

  ConstructionTest(ConstructionTest&& other) noexcept // IV. move constructor
    : cstring(std::exchange(other.cstring, nullptr))
    {
    }

  ConstructionTest& operator=(ConstructionTest&& other) noexcept // V. move assignment
  {
    ConstructionTest temp(std::move(other));
    std::swap(cstring, temp.cstring);
    return *this;
  }
}

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

TEST(BlockingQueueTests, GivenMultipleProducerAndSingleConsumer_WhenBlockAndPop_CorrectItemsTakenFromQueue)
{
  std::promise<std::vector<int>> dataPromise;
  auto fut = dataPromise.get_future();
  BlockingQueue<int> queue;
  std::vector<int> expectedData{1,2,3,4,5,6,7,8,9,10};

  auto th1 = std::thread(&Consume<int>, std::ref(queue), std::move(dataPromise), 10);
  auto th2 = std::thread(&Produce<int>, std::ref(queue), std::vector<int>{1,2,3});
  auto th3 = std::thread(&Produce<int>, std::ref(queue), std::vector<int>{4,5,6});
  auto th4 = std::thread(&Produce<int>, std::ref(queue), std::vector<int>{7,8});
  auto th5 = std::thread(&Produce<int>, std::ref(queue), std::vector<int>{9,10});

  auto consumerResult = fut.get();
  std::sort(std::begin(consumerResult), std::end(consumerResult));
  th1.join();
  th2.join();
  th3.join();
  th4.join();
  th5.join();

  ASSERT_THAT(expectedData, ::testing::ContainerEq(consumerResult)) << "hello"; 
}
