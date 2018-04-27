/********************************************************************

   BagOValues.h

   Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License.

********************************************************************/

#include <map>
#include <mutex>
#include <vector>
#include <algorithm>
#include <string_view>

#include "spinlock.h"


template <class TValue>
class BagOValues
{
	typedef std::pair<std::wstring, TValue> TPair;
	typedef std::vector<TPair> TVector;
	typedef typename TVector::const_iterator TItr;

	SpinLock m_spinlock;
	TVector m_Values;
	std::wstring m_lastStr;
	TItr m_LastItr;

public:
	BagOValues()
	{
	}

	// copies the value, but doesn't assume any memory management needs be done
	void Add(std::wstring key, TValue value)
	{
		std::lock_guard<SpinLock> guard(this->m_spinlock);
		std::wstring lowered;
		lowered.resize(key.size());
		std::transform(std::begin(key), std::end(key), std::begin(lowered), ::tolower);
		m_Values.emplace_back(make_pair(std::move(lowered), value));

		m_lastStr.resize(0);	// clear this after new data added
	}

	void Sort()
	{
		std::lock_guard<SpinLock> guard(this->m_spinlock);
		std::sort(m_Values.begin(), m_Values.end());
	}

	// Retrieve with fPrefix = true means return values for the tree at the point of the query matched; 
	//      we must consume the whole query for anything to be returned
	// fPrefix = false means that we only return values when an entire key matches and we match substrings of the query
	//
	// NOTE: returns a newly allocated vector; must delete it
	auto Retrieve(const std::wstring_view query, bool fPrefix = true, unsigned maxResults = ULONG_MAX)
	{
		std::wstring lowered;
		lowered.resize(query.size());
		std::transform(std::cbegin(query), std::cend(query), std::begin(lowered), ::tolower);

		std::vector<TValue> results;
		TValue val = TValue();
		TPair laspair = make_pair(lowered, val);

		std::lock_guard<SpinLock> guard(this->m_spinlock);

		// if last saved string/iterator is a prefix of the new string, start there
		TItr itr;
		if (m_lastStr.size() != 0 && lowered.compare(0, m_lastStr.size(), m_lastStr) == 0)
			itr = m_LastItr;
		else
		{
			itr = lower_bound(m_Values.begin(), m_Values.end(), laspair, CompareFirst);

			m_lastStr = lowered;
			m_LastItr = itr;
		}

		for (; itr != m_Values.end(); itr++)
		{
			const auto& key = itr->first;
			int cmp = key.compare(0, lowered.size(), lowered);
			if (cmp == 0)
			{
				if (!fPrefix && key.size() != lowered.size())
				{
					// need exact match (not just prefix); skip
					continue;
				}

				if (results.size() >= maxResults)
					break;

				results.push_back(itr->second);
			}
			else if (cmp > 0)
			{
				// iterated past the strings which match on the prefix
				break;
			}
		}

		return results;
	}

private:
	static bool CompareFirst(const TPair& a, const TPair& b)
	{
		return a.first < b.first;
	}
};

