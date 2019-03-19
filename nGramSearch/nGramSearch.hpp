// nGramSearch.cpp : Defines the exported functions for the DLL application.
//
#ifndef NGRAMSEARCH_HPP
#define NGRAMSEARCH_HPP

#include "nGramSearch.h"

void StringSearch::StringIndex::getGrams(size_t id)
{
	auto& str = stringLib[id];
	for (size_t i = 0; i < str.size() - 2; i++)
	{
		auto hash = gramHash(str, i);
		ngrams[hash].insert(id);
	}
}

std::vector<int32_t> StringSearch::StringIndex::getGrams(const std::string& str) const
{
	std::vector<int32_t> generatedGrams;
	generatedGrams.reserve(str.size() - 2);
	for (size_t i = 0; i < str.size() - 2; i++)
		generatedGrams.emplace_back(gramHash(str, i));
	return generatedGrams;
}

void StringSearch::StringIndex::buildGrams()
{
	for (auto& id : longLib)
		getGrams(id);
	indexed = true;
}

void StringSearch::StringIndex::init(std::unordered_map<std::string, std::vector<std::string>>& tempWordMap,
	std::unordered_map<std::string, std::unordered_map<std::string, float>>& tempWordWeight)
{
	//Centralize all strings in an array so that they do not take replicated spaces
	std::unordered_set<std::string> tempStrLib;
	for (auto& kp : tempWordMap)
	{
		tempStrLib.insert(kp.first);
		for (auto& str : kp.second)
			tempStrLib.insert(str);
	}
	stringLib = std::vector<std::string>(tempStrLib.begin(), tempStrLib.end());

	//to separate the long and short libs, since different algorithms will be applied upon searches
	std::unordered_map<std::string, size_t> stringIndex(stringLib.size());
	for (size_t i = 0; i < stringLib.size(); i++)
	{
		stringIndex[stringLib[i]] = i;
		if (stringLib[i].size() > longest)
			longest = stringLib[i].size();
	}
	for (auto& kp : tempWordMap)
	{
		auto& searchTerm = kp.first;
		auto pTargetIndex = stringIndex.find(searchTerm);
		if (pTargetIndex != stringIndex.end())
		{
			auto id = pTargetIndex->second;
			if (searchTerm.size() >= 6)
				longLib.push_back(id);
			else
				shortLib.push_back(id);
			std::vector<size_t> keysMapped;
			keysMapped.reserve(tempWordMap[searchTerm].size());
			for (auto& key : tempWordMap[searchTerm])
			{
				auto target = stringIndex.find(key);
				if (target != stringIndex.end())
					keysMapped.push_back(target->second);
			}
			wordMap[id] = move(keysMapped);

			for (auto& pair : tempWordWeight[searchTerm])
			{
				auto target = stringIndex.find(pair.first);
				if (target != stringIndex.end())
					wordWeight[id][target->second] = pair.second;	//target.second : pointer to keyword, &str: pointer to query, pair.second: weight
			}
		}
	}

	stringLib.shrink_to_fit();
	longLib.shrink_to_fit();
	shortLib.shrink_to_fit();

	//Each future involves an overhead of about 1-2 us, so I am limiting the number of futures to hardware concurrency
	auto nThrd = std::thread::hardware_concurrency();
	size_t sizeNeededShort = (size_t)ceil((float)shortLib.size() / nThrd);
	if (sizeNeededShort > sectionSizeShort)
		sectionSizeShort = sizeNeededShort;
	size_t sizeNeededLong = (size_t)ceil((float)longLib.size() / nThrd);
	if (sizeNeededLong > sectionSizeLong)
		sectionSizeLong = sizeNeededLong;
}

StringSearch::StringIndex::StringIndex(char** const words, const size_t size, const uint16_t rowSize, float* const weight)
{
	if (size < 2 || !words)
		return;
	std::unordered_map<std::string, std::vector<std::string>> tempWordMap(size);
	std::unordered_map<std::string, std::unordered_map<std::string, float>> tempWeightMap(size);
	for (size_t i = 0; i < size; i += rowSize)
	{
		//skip null entries
		if (!words[i])
			continue;
		std::string strKey(words[i]);
		trim(strKey);
		//skip empty entries
		if (strKey.size() == 0)
			continue;
		std::string upperKey(strKey);
		escapeBlank(upperKey, validChar);
		trim(upperKey);
		toUpper(upperKey);

		float currentWeight = 1.0f;
		if (weight)
			currentWeight = weight[i];
		if (currentWeight != 0.0f)
		{
			tempWordMap[upperKey].push_back(strKey);
			tempWeightMap[upperKey][strKey] = currentWeight;
		}

		for (size_t j = i + 1; j < i + rowSize; j++)
			if (words[j])
			{
				std::string strQuery(words[j]);
				escapeBlank(strQuery, validChar);
				trim(strQuery);
				toUpper(strQuery);
				if (strQuery.size() != 0)
				{
					currentWeight = 1.0f;
					if (weight)
						currentWeight = weight[j];
					if (currentWeight != 0.0f)
					{
						tempWordMap[strQuery].push_back(strKey);
						tempWeightMap[strQuery][strKey] = currentWeight;
					}
				}
			}
	}
	init(tempWordMap, tempWeightMap);
	buildGrams();
}

size_t StringSearch::StringIndex::stringMatch(const std::string& query, const std::string& source,
	std::vector<size_t>& row1, std::vector<size_t>& row2) const
{
	if (query.size() == 1)
	{
		for (auto ch : source)
			if (ch == query[0])
				return 1;
		return 0;
	}

	size_t qSize = query.size();
	size_t sSize = source.size();
	auto maxSize = std::max(qSize, sSize);

	fill(row1.begin(), row1.end(), 0);
	for (size_t q = 0; q < qSize; q++)
	{
		row2[0] = q + 1;
		for (size_t s = 0; s < sSize; s++)
		{
			int cost = 0;
			if (query[q] != source[s])
				cost = 1;
			row2[s + 1] =
				std::min(
					std::min(
						row1[s + 1] + 1,
						row2[s] + 1),
					row1[s] + cost);
		}
		swap(row1, row2);
		//memset(row2, 0, sizeof(unsigned) * (maxSize + 1));
		std::fill(row2.begin(), row2.end(), 0);
	}
	size_t misMatch = (std::numeric_limits<size_t>::max)();
	for (unsigned i = 0; i < sSize + 1; i++)
		if (row1[i] < misMatch)
			misMatch = row1[i];
	return qSize - misMatch;
}

void StringSearch::StringIndex::getMatchScore(const std::string& query, size_t first, std::vector<size_t>& targets,
	std::vector<float>& currentScore) const
{
	auto size = std::max(query.size() + 1, (size_t)6);
	if (query.size() <= 3)
		size = longest + 1;
	//allocate levenstein temporary containers
	std::vector<size_t> row1(size);
	std::vector<size_t> row2(size);
	for (size_t i = first * sectionSizeShort; i < first * sectionSizeShort + sectionSizeShort && i < shortLib.size(); i++)
	{
		auto& source = shortLib[i];
		auto match = stringMatch(query, stringLib[source], row1, row2);
		currentScore[i] = (float)match / query.size();
		targets[i] = source;
	}
	//search for all strings if n-gram does not work
	if (query.size() <= 3)
		for (size_t i = first * sectionSizeLong; i < first * sectionSizeLong + sectionSizeLong && i < longLib.size(); i++)
		{
			auto& source = longLib[i];
			auto match = stringMatch(query, stringLib[source], row1, row2);
			currentScore[i + shortLib.size()] = (float)match / query.size();
			targets[i + shortLib.size()] = source;
		}
}

void StringSearch::StringIndex::searchShort(std::string& query, std::unordered_map<size_t, float>& score) const
{
	auto len = query.size();
	auto dicSize = shortLib.size();
	if (query.size() <= 3)
		dicSize += longLib.size();
	std::vector<size_t> targets(dicSize);
	std::vector<float> currentScore(dicSize);
	std::vector<std::future<void>> futures;
	auto nThrd = std::thread::hardware_concurrency();
	for (size_t section = 0; section < nThrd; section++)
		futures.emplace_back(std::async(std::launch::async, &StringIndex::getMatchScore, this, std::cref(query), section, std::ref(targets), std::ref(currentScore)));
	for (auto& fu : futures)
		fu.get();
	for (size_t i = 0; i < dicSize; i++)
		score[targets[i]] += currentScore[i];
}

void StringSearch::StringIndex::searchLong(std::string& query, std::unordered_map<size_t, float>& score) const
{
	auto len = query.size();
	if (len < (size_t)3)
		return;

	auto generatedGrams = getGrams(query);
	if (generatedGrams.empty())
		return;
	std::unordered_map<size_t, size_t> rawScore(longLib.size());
	//may consider parallelsm here in the future
	for (auto& gram : generatedGrams)
	{
		auto found = ngrams.find(gram);
		if (found != ngrams.end())
		{
			auto& sourceSet = found->second;
			for (size_t match : sourceSet)
				rawScore[match]++;
		}
	}
	for (auto& kp : rawScore)
		score[kp.first] = (float)kp.second / generatedGrams.size();
}

void StringSearch::StringIndex::calcScore(std::string& query, std::unordered_map<size_t, float>& entryScore,
	std::unordered_map<size_t, float>& scoreList, const float threshold) const
{
	for (auto& scorePair : scoreList)
	{
		if (scorePair.second < threshold)
			continue;
		auto& searchWord = scorePair.first;
		auto weightDicPair = wordWeight.find(searchWord);
		auto mapped = wordMap.find(searchWord);
		if (mapped != wordMap.end() && weightDicPair != wordWeight.end())
			for (auto& keyWord : mapped->second)
			{
				auto weightPair = weightDicPair->second.find(keyWord);
				if (weightPair != weightDicPair->second.end())
				{
					auto score = std::max(weightPair->second * scorePair.second, entryScore[keyWord]);
					//the score is considered perfect greater than 0.999
					if (scorePair.second > 0.999)
					{
						std::string libStr(stringLib[keyWord]);
						escapeBlank(libStr, validChar);
						trim(libStr);
						//On exact match, promote to top
						if (libStr == query)
							score = 100;
					}
					entryScore[keyWord] = score;
				}
			}
	}
}

std::vector<std::pair<size_t, float>> StringSearch::StringIndex::_search(const char* query, const float threshold, const uint32_t limit) const
{
	std::string queryStr(query);
	std::unordered_map<size_t, float> entryScore;

	//wildcard
	if (queryStr.size() == 0 || (queryStr.size() == 1 && queryStr[0] == '*'))
	{
		for (auto& kp : wordMap)
			for (auto& w : kp.second)
				if (wordWeight.find(w) != wordWeight.end() && wordWeight.find(w)->second.find(w) != wordWeight.find(w)->second.end())
					entryScore[w] = wordWeight.find(w)->second.find(w)->second;
	}
	else
	{
		escapeBlank(queryStr, validChar);
		trim(queryStr);
		if (queryStr.size() == 0)
			return std::vector<std::pair<size_t, float>>();
		toUpper(queryStr);
		std::unordered_map<size_t, float> scoreShort(shortLib.size());
		std::unordered_map<size_t, float> scoreLong(longLib.size());
		std::vector<std::future<void>> futures;
		//if the query is long, there is no need to search for short sequences.
		if (queryStr.size() < 9)
			futures.emplace_back(
				std::async(std::launch::async, &StringIndex::searchShort, this, std::ref(queryStr), ref(scoreShort))
			);
		futures.emplace_back(
			std::async(std::launch::async, &StringIndex::searchLong, this, std::ref(queryStr), ref(scoreLong))
		);
		for (auto& fu : futures)
			fu.get();

		//merge scores to entryScore
		entryScore.reserve(scoreShort.size() + scoreLong.size());
		calcScore(queryStr, entryScore, scoreShort, threshold);
		calcScore(queryStr, entryScore, scoreLong, threshold);
	}

	std::vector<std::pair<size_t, float>> scoreElems(entryScore.begin(), entryScore.end());
	auto endIt = scoreElems.end();
	if (scoreElems.size() > limit)
		endIt = scoreElems.begin() + limit;
	std::partial_sort(scoreElems.begin(), endIt, scoreElems.end(), ScoreComparer(*this));

	return scoreElems;
}

uint32_t StringSearch::StringIndex::score(const char* query, char*** results, float** scores, const float threshold, uint32_t limit) const
{
	if (!indexed)
		return 0;

	if (limit == 0)
		limit = (std::numeric_limits<int32_t>::max)();

	auto result = _search(query, threshold, limit);

	uint32_t size = std::min((uint32_t)result.size(), limit);

	//transform to C ABI using pointers
	*scores = new float[size];
	*results = new char*[size];
	for (uint32_t i = 0; i < size; i++)
	{
		auto item = result[i].first;
		auto resStr = stringLib[item].c_str();
		(*results)[i] = const_cast<char*>(resStr);
		(*scores)[i] = result[i].second;
	}
	return size;
}

uint32_t StringSearch::StringIndex::search(const char* query, char*** results, const float threshold, uint32_t limit) const
{
	if (!indexed)
		return 0;

	if (limit == 0)
		limit = (std::numeric_limits<int32_t>::max)();

	auto result = _search(query, threshold, limit);

	uint32_t size = std::min((uint32_t)result.size(), limit);

	//transform to C ABI using pointers
	*results = new char*[size];
	for (uint32_t i = 0; i < size; i++)
	{
		auto item = result[i].first;
		auto resStr = stringLib[item].c_str();
		(*results)[i] = const_cast<char*>(resStr);
	}
	return size;
}

void StringSearch::StringIndex::release(char** results, float* scores) const
{
	if (results)
		delete[] results;
	if (scores)
		delete[] scores;
}

uint64_t StringSearch::StringIndex::size() const
{
	return wordMap.size();
}

uint64_t StringSearch::StringIndex::libSize() const
{
	return ngrams.size();
}

void StringSearch::StringIndex::setValidChar(std::unordered_set<char>& newValidChar)
{
	validChar = std::move(newValidChar);
}

#endif