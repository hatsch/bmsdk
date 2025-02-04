/* -LICENSE-START-
** Copyright (c) 2019 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation covered by
** this license (the "Software") to use, reproduce, display, distribute,
** execute, and transmit the Software, and to prepare derivative works of the
** Software, and to permit third-parties to whom the Software is furnished to
** do so, all subject to the following:
**
** The copyright notices in the Software and this entire statement, including
** the above license grant, this restriction and the following disclaimer,
** must be included in all copies of the Software, in whole or in part, and
** all derivative works of the Software, unless such copies or derivative
** works are solely in the form of machine-executable object code generated by
** a source language processor.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
** -LICENSE-END-
*/

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>


class DispatchQueue
{
	using DispatchFunction = std::function<void(void)>;

public:
	DispatchQueue(size_t numThreads);
	virtual ~DispatchQueue();

	template<class F, class... Args>
	void dispatch(F&& fn, Args&&... args);
	
private:
	std::vector<std::thread>		m_workerThreads;
	std::queue<DispatchFunction>	m_functionQueue;
	std::condition_variable			m_condition;
	std::mutex						m_mutex;

	bool							m_cancelWorkers;

	void workerThread(void);
};

DispatchQueue::DispatchQueue(size_t numThreads) :
	m_cancelWorkers(false)
{
	for (size_t i = 0; i < numThreads; i++)
	{
		m_workerThreads.emplace_back(&DispatchQueue::workerThread, this);
	}
}

DispatchQueue::~DispatchQueue()
{
	// Stop all threads once they have completed current job
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_cancelWorkers = true;
	}
	m_condition.notify_all();

	for (auto& worker : m_workerThreads)
	{
		worker.join();
	}
}

template<class F, class... Args>
void DispatchQueue::dispatch(F&& fn, Args&& ...args)
{
	using DispatchFunctionBinding = decltype(std::bind(std::declval<F>(), std::declval<Args>()...));
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_functionQueue.push(DispatchFunctionBinding(std::forward<F>(fn), std::forward<Args>(args)...));
	}
	m_condition.notify_one();
}

void DispatchQueue::workerThread()
{
	while (true)
	{
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_condition.wait(lock, [&] { return !m_functionQueue.empty() || m_cancelWorkers; });
		}

		while (true)
		{
			DispatchFunction func;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (m_functionQueue.empty())
					break;

				func = std::move(m_functionQueue.front());
				m_functionQueue.pop();
			}

			func();
		}

		if (m_cancelWorkers)
			// Exit thread
			break;
	}
}
