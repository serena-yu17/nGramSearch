// nGramSearch.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "main.h"
using namespace std;


void StringIndex::getGrams(string* str)
{
	for (int i = 0; i < str->size() - gramSize + 1; i++)
	{
		auto gram = str->substr(i, gramSize);
		ngrams[gram].push_back(str);
	}
}

void StringIndex::getGrams(const string& str, vector<string>& generatedGrams)
{
	for (int i = 0; i < str.size() - gramSize + 1; i++)
	{
		auto gram = str.substr(i, gramSize);
		generatedGrams.push_back(gram);
	}
}

void StringIndex::getGrams(const char* str, const int size, vector<string>& generatedGrams)
{
	for (int i = 0; i < size - gramSize + 1; i++)
	{
		string gram;
		for (int j = i; j < i + gramSize; j++)
			gram.push_back(str[j]);
		generatedGrams.push_back(gram);
	}
}

void StringIndex::buildGrams()
{
	for (string& str : longLib)
		getGrams(&str); 
}

int StringIndex::stringMatch(const string& query, const string& source)
{
	if (query.size() == 1)
	{
		for (auto ch : source)
			if (ch == query[0])
				return 1;
		return 0;
	}

	unsigned qSize = (unsigned)query.size();
	unsigned sSize = (unsigned)source.size();
	auto maxSize = max(qSize, sSize);
	vector<unsigned> row1(maxSize + 1, 0);
	vector<unsigned> row2(maxSize + 1, 0);
	for (unsigned q = 0; q < qSize; q++)
	{
		row2[0] = q + 1;
		for (unsigned s = 0; s < sSize; s++)
		{
			int cost = 0;
			if (query[q] != source[s])
				cost = 1;
			row2[s + 1] =
				min(
					min(
						row1[s + 1] + 1,
						row2[s] + 1),
					row1[s] + cost);
		}
		swap(row1, row2);
		//memset(row2, 0, sizeof(unsigned) * (maxSize + 1));
		fill(row2.begin(), row2.end(), 0);
	}
	unsigned misMatch = (numeric_limits<unsigned>::max)();
	for (unsigned i = 0; i < sSize + 1; i++)
		if (row1[i] < misMatch)
			misMatch = row1[i];
	return qSize - misMatch;
}

pair<string*, int> StringIndex::getMatchScore(const std::string& query, std::string* source)
{
	int match = stringMatch(query, *source);
	return make_pair(source, match);
}

void StringIndex::searchShort(string& query, unordered_map<string, float>& score)
{
	unsigned len = (unsigned)query.size();
	vector<future<pair<string*, int>>> futures;
	for (auto str : shortLib)
		futures.emplace_back(async(std::launch::async, &StringIndex::getMatchScore, this, query, &str));
	for (int i = 0; i < futures.size(); i++)
	{
		auto res = futures[i].get();
		std::lock_guard<mutex> lock(mutScore);
		if (score.find(*res.first) == score.end())
			score[*res.first] = (float)res.second / len;
		else
			score[*res.first] += (float)res.second / len;
	}
}

void StringIndex::searchLong(string& query, unordered_map<string, float>& score)
{
	auto len = query.size();
	if (len < gramSize)
		return;

	vector<string> generatedGrams;
	getGrams(query, generatedGrams);
	if (generatedGrams.size() == 0)
		return;
	unordered_map<string, size_t> rawScore;
	for (auto& str : generatedGrams)
	{
		const auto& sourceSet = ngrams[str];
		for (auto match : sourceSet)
			rawScore[*match] ++;
	}
	for (auto& keyPair : rawScore)
	{
		std::lock_guard<mutex> lock(mutScore);
		score[keyPair.first] += (float)keyPair.second / generatedGrams.size();
	}
}

void StringIndex::insert(vector<string>& key, vector<string>* additional, const int16_t gSize)
{
	std::unordered_map<std::string, std::string> tempWordMap;
	if (gSize < 2 || key.size() < 2)
		return;
	for (size_t i = 0; i < key.size(); i++)
	{
		string strKey(key[i]);
		trim(strKey);
		string upperKey(strKey);
		toUpper(upperKey);
		tempWordMap[upperKey] = strKey;

		if (additional)
		{
			string strQuery((*additional)[i]);
			trim(strQuery);
			toUpper(strQuery);
			tempWordMap[strQuery] = strKey;
		}
	}
	gramSize = gSize;
	for (auto& keyPair : tempWordMap)
	{
		auto& str = keyPair.first;
		if (str.size() >= gramSize * 2)
		{
			longLib.push_back(str);
			wordMap[&longLib.back()] = keyPair.second;
		}
		else
		{
			shortLib.push_back(str);
			wordMap[&shortLib.back()] = keyPair.second;
		}
	}
	buildGrams();
}

void StringIndex::insert(char** const key, const size_t size, char** const additional, const uint16_t gSize)
{  	
	std::unordered_map<std::string, std::string> tempWordMap;
	if (gSize < 2 || size < 2)
		return;
	for (size_t i = 0; i < size; i++)
	{
		string strKey(key[i]);
		trim(strKey);
		string upperKey(strKey);
		toUpper(upperKey);
		tempWordMap[upperKey] = strKey;

		if (additional)
		{
			string strQuery(additional[i]);
			trim(strQuery);
			toUpper(strQuery);
			tempWordMap[strQuery] = strKey;
		}
	}
	gramSize = gSize;
	for (auto& keyPair : tempWordMap)
	{
		auto& str = keyPair.first;
		if (str.size() >= gramSize * 2)
		{
			longLib.push_back(str);
			wordMap[&longLib.back()] = keyPair.second;
		}
		else
		{
			shortLib.push_back(str);
			wordMap[&shortLib.back()] = keyPair.second;
		}
	}
	buildGrams();
}

void StringIndex::search(const char* query, const float threshold, const uint32_t limit, vector<string>& result)
{
	string queryStr(query);
	toUpper(queryStr);
	unordered_map<string, float> score;
	vector<future<void>> futures;
	futures.emplace_back(
		std::async(std::launch::async, &StringIndex::searchShort, this, std::ref(queryStr), ref(score))
	);
	futures.emplace_back(
		std::async(std::launch::async, &StringIndex::searchLong, this, std::ref(queryStr), ref(score))
	);
	for (auto& fu : futures)
		fu.get();

	//merge
	/*for (auto& keyPair : scoreLong)
		scoreShort[keyPair.first] += keyPair.second;*/
	vector<pair<string, float>> scoreElems(score.begin(), score.end());	 
	sort(scoreElems.begin(), scoreElems.end(), compareScores);

	for (size_t i = 0; i < scoreElems.size() && i < limit && scoreElems[i].second >= threshold; i++)
		result.push_back(scoreElems[i].first);
}

void StringIndex::search(const char* query, char*** results, uint32_t* nStrings, const float threshold, uint32_t limit)
{
	if (wordMap.size() == 0 || gramSize == 0)
		return;

	if (limit == 0)
		limit = (numeric_limits<int32_t>::max)();

	vector<string> result;

	search(query, threshold, limit, result);

	//transform to C ABI using pointers
	*nStrings = (uint32_t)result.size();
	*results = new char*[*nStrings];
	for (uint32_t i = 0; i < *nStrings; i++)
	{
		auto item = result[i];
		auto resStr = item.c_str();
		auto len = item.size();
		(*results)[i] = new char[len + 1]();
		memcpy((*results)[i], resStr, len);
	}
}

void StringIndex::release(char*** results, size_t nStrings)
{
	if (*results)
	{
		for (int i = 0; i < nStrings; i++)
			delete[](*results)[i];
		delete[](*results);
	}
}