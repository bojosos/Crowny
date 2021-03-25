#include <iostream>
#include <thread>
#include <queue>
#include <functional>

std::queue<std::function<void(void)>> queue;

void loop()
{
  while (true)
  {
    if (!queue.empty())
    {
      queue.front()();
      queue.pop();
    }
  }
}

int main ()
{

  std::thread thread(loop);
  queue.push([&]() {std::cout << "Hey coming from main" << std::endl; });
  thread.join();
  return 0;
}

