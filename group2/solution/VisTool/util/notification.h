#pragma once

// Implements a simplified Observer pattern

#include <list>
#include <functional>
#include <algorithm>

class Observer;
class Observable;

class Observer
{
public:
	virtual void notify(Observable * subject) {};
};

class Observable
{
public:
	void RegisterObserver(Observer * observer)	{
		m_observers.push_back(observer);
	};

	void UnregisterObserver(Observer * observer)
	{
		m_observers.remove(observer);
	};

	void NotifyAll(void)
	{
		std::for_each(m_observers.begin(), m_observers.end(), [this] (Observer * observer) {
			observer->notify(this);
		});
	};

	bool HasObservers(void)
	{
		return !m_observers.empty();
	}

private:
	std::list<Observer *> m_observers;
};

