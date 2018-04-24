// nGramSearch.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "main.h"
using namespace std;

vector<const string*> longLib;
vector<const string*> shortLib;
unordered_map<string, string> wordMap;
unordered_map<string, vector<const string*>> ngrams;
vector<const wstring*> longLibW;
vector<const wstring*> shortLibW;
unordered_map<wstring, wstring> wordMapW;
unordered_map<wstring, vector<const wstring*>> ngramsW;
int16_t gramSize = 3;

void getGrams(const string* str)
{
	for (int i = 0; i < str->size() - gramSize + 1; i++)
	{
		auto gram = str->substr(i, gramSize);
		ngrams[gram].push_back(str);
	}
}

void getGramsW(const wstring* str)
{
	for (int i = 0; i < str->size() - gramSize + 1; i++)
	{
		auto gram = str->substr(i, gramSize);
		ngramsW[gram].push_back(str);
	}
}	

void getGrams(const string& str, vector<string>& generatedGrams)
{
	for (int i = 0; i < str.size() - gramSize + 1; i++)
	{
		auto gram = str.substr(i, gramSize);
		generatedGrams.push_back(gram);
	}
}

void getGramsW(const wstring& str, vector<wstring>& generatedGrams)
{
	for (int i = 0; i < str.size() - gramSize + 1; i++)
	{
		auto gram = str.substr(i, gramSize);
		generatedGrams.push_back(gram);
	}
}

void getGrams(const char* str, const int size, vector<string>& generatedGrams)
{
	for (int i = 0; i < size - gramSize + 1; i++)
	{
		string gram;
		for (int j = i; j < i + gramSize; j++)
			gram.push_back(str[j]);
		generatedGrams.push_back(gram);
	}
}

void getGramsW(const wchar_t* str, const int size, vector<wstring>& generatedGrams)
{
	for (int i = 0; i < size - gramSize + 1; i++)
	{
		wstring gram;
		for (int j = i; j < i + gramSize; j++)
			gram.push_back(str[j]);
		generatedGrams.push_back(gram);
	}
}

//void getGramsBatch(const size_t base, const size_t len, const int interval,
//	concurrency::concurrent_unordered_map<string, vector<const string*>>* generatedGrams)
//{
//	for (size_t i = base; i < len && i < base + interval; i++)
//		getGrams(longLib[i]);
//}

void buildGrams()
{
	for (auto& str : longLib)
		getGrams(str);
	/*const int interval = 1000;
	const size_t len = longLib.size();
	const int runs = (const int)ceil(len / interval);
	for (int r = 0; r < runs; r++)
	{
		size_t base = interval * r;
		getGramsBatch( base, len, interval);
	}
	vector<future<void>> futures;
	for (int r = 0; r < runs; r++)
	{
		size_t base = interval * r;
		futures.emplace_back(async(launch::async, getGramsBatch, base, len, interval, &generatedGrams));
	}
	for (auto& fu : futures)		
		fu.get();
	for (auto& keyPair : generatedGrams)
	{
		auto& target = ngrams[keyPair.first];
		auto& source = keyPair.second;
		target.insert(end(target), begin(source), end(source));
	}*/
}

void buildGramsW()
{
	for (auto& str : longLibW)
		getGramsW(str);
}

int stringMatch(const string& query, const string& source)
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

int stringMatchW(const wstring& query, const wstring& source)
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

pair<const string*, int> getMatchScore(const string& query, const string* source)
{
	int match = stringMatch(query, *source);
	return make_pair(source, match);
}

pair<const wstring*, int> getMatchScoreW(const wstring& query, const wstring* source)
{
	int match = stringMatchW(query, *source);
	return make_pair(source, match);
}

void searchShort(string& query, unordered_map<const string*, float, StringPointerHash, StringPointerEqual>& score)
{
	unsigned len = (unsigned)query.size();
	vector<future<pair<const string*, int>>> futures;
	for (const string* str : shortLib)
		futures.emplace_back(async(std::launch::async, getMatchScore, query, str));
	for (int i = 0; i < futures.size(); i++)
	{
		auto res = futures[i].get();
		if (score.find(res.first) == score.end())
			score[res.first] = (float)res.second / len;
		else
			score[res.first] += (float)res.second / len;
	}
}

void searchShortW(wstring& query, unordered_map<const wstring*, float, StringPointerHashW, StringPointerEqualW>& score)
{
	unsigned len = (unsigned)query.size();
	vector<future<pair<const wstring*, int>>> futures;
	for (const wstring* str : shortLibW)
		futures.emplace_back(async(std::launch::async, getMatchScoreW, query, str));
	for (int i = 0; i < futures.size(); i++)
	{
		auto res = futures[i].get();
		if (score.find(res.first) == score.end())
			score[res.first] = (float)res.second / len;
		else
			score[res.first] += (float)res.second / len;
	}
}

void searchLong(string& query, unordered_map<const string*, float, StringPointerHash, StringPointerEqual>& score)
{
	auto len = query.size();
	if (len < gramSize)
		return;

	vector<string> generatedGrams;
	getGrams(query, generatedGrams);
	if (generatedGrams.size() == 0)
		return;
	unordered_map<const string*, size_t, StringPointerHash, StringPointerEqual> rawScore;
	for (auto& str : generatedGrams)
	{
		const auto& sourceSet = ngrams[str];
		for (auto match : sourceSet)
			rawScore[match] ++;
	}
	for (auto& keyPair : rawScore)
		score[keyPair.first] += (float)keyPair.second / generatedGrams.size();
}

void searchLongW(wstring& query, unordered_map<const wstring*, float, StringPointerHashW, StringPointerEqualW>& score)
{
	auto len = query.size();
	if (len < gramSize)
		return;

	vector<wstring> generatedGrams;
	getGramsW(query, generatedGrams);
	if (generatedGrams.size() == 0)
		return;
	unordered_map<const wstring*, size_t, StringPointerHashW, StringPointerEqualW> rawScore;
	for (auto& str : generatedGrams)
	{
		const auto& sourceSet = ngramsW[str];
		for (auto match : sourceSet)
			rawScore[match] ++;
	}
	for (auto& keyPair : rawScore)
		score[keyPair.first] += (float)keyPair.second / generatedGrams.size();
}

void insert(vector<string>& key, vector<string>* additional, const int16_t gSize)
{
	clearW();
	if (gSize < 2 || key.size() < 2)
		return;
	for (size_t i = 0; i < key.size(); i++)
	{
		string strKey(key[i]);
		trim(strKey);
		string upperKey(strKey);
		toUpper(upperKey);
		wordMap[upperKey] = strKey;

		if (additional)
		{
			string strQuery((*additional)[i]);
			trim(strQuery);
			toUpper(strQuery);
			wordMap[strQuery] = strKey;
		}
	}
	gramSize = gSize;
	for (auto& keyPair : wordMap)
	{
		auto& str = keyPair.first;
		if (str.size() >= gramSize * 2)
			longLib.push_back(&str);
		else
			shortLib.push_back(&str);
	}
	buildGrams();
}

void insertW(vector<wstring>& key, vector<wstring>* additional, const int16_t gSize)
{
	clear();
	if (gSize < 2 || key.size() < 2)
		return;
	for (size_t i = 0; i < key.size(); i++)
	{
		wstring strKey(key[i]);
		trimW(strKey);
		wstring upperKey(strKey);
		toUpperW(upperKey);
		wordMapW[upperKey] = strKey;

		if (additional)
		{
			wstring strQuery((*additional)[i]);
			trimW(strQuery);
			toUpperW(strQuery);
			wordMapW[strQuery] = strKey;
		}
	}
	gramSize = gSize;
	for (auto& keyPair : wordMap)
	{
		auto& str = keyPair.first;
		if (str.size() >= gramSize * 2)
			longLib.push_back(&str);
		else
			shortLib.push_back(&str);
	}
	buildGramsW();
}

DLLEXP bool insert(char** const key, const size_t size, char** const additional, const uint16_t gSize)
{  	
	if (gSize < 2 || size < 2)
		return false;
	for (size_t i = 0; i < size; i++)
	{
		string strKey(key[i]);
		trim(strKey);
		string upperKey(strKey);
		toUpper(upperKey);
		wordMap[upperKey] = strKey;

		if (additional)
		{
			string strQuery(additional[i]);
			trim(strQuery);
			toUpper(strQuery);
			wordMap[strQuery] = strKey;
		}
	}
	gramSize = gSize;
	for (auto& keyPair : wordMap)
	{
		auto& str = keyPair.first;
		if (str.size() >= gramSize * 2)
			longLib.push_back(&str);
		else
			shortLib.push_back(&str);
	}
	buildGrams();
	return true;
}

DLLEXP bool insertW(wchar_t** const key, const size_t size, wchar_t** const additional, const uint16_t gSize)
{
	if (gSize < 2 || size < 2)
		return false;
	for (size_t i = 0; i < size; i++)
	{
		wstring strKey(key[i]);
		trimW(strKey);
		wstring upperKey(strKey);
		toUpperW(upperKey);
		wordMapW[upperKey] = strKey;

		if (additional)
		{
			wstring strQuery(additional[i]);
			trimW(strQuery);
			toUpperW(strQuery);
			wordMapW[strQuery] = strKey;
		}
	}
	gramSize = gSize;
	for (auto& keyPair : wordMap)
	{
		auto& str = keyPair.first;
		if (str.size() >= gramSize * 2)
			longLib.push_back(&str);
		else
			shortLib.push_back(&str);
	}
	buildGramsW();
	return true;
}

void search(const char* query, const float threshold, const uint32_t limit, vector<const string*>& result)
{
	string queryStr(query);
	toUpper(queryStr);
	unordered_map<const string*, float, StringPointerHash, StringPointerEqual> scoreShort, scoreLong;
	vector<future<void>> futures;
	futures.emplace_back(
		async(launch::async, searchShort, ref(queryStr), ref(scoreShort))
	);
	futures.emplace_back(
		async(launch::async, searchLong, ref(queryStr), ref(scoreLong))
	);
	for (auto& fu : futures)
		fu.get();

	//merge
	for (auto& keyPair : scoreLong)
		scoreShort[keyPair.first] += keyPair.second;
	vector<pair<const string*, float>> scoreElems(scoreShort.begin(), scoreShort.end());

	sort(scoreElems.begin(), scoreElems.end(), compareScores);
	for (size_t i = 0; i < scoreElems.size() && i < limit && scoreElems[i].second >= threshold; i++)
		result.push_back(scoreElems[i].first);
}

void searchW(const wchar_t* query, const float threshold, const uint32_t limit, vector<const wstring*>& result)
{
	wstring queryStr(query);
	toUpperW(queryStr);
	unordered_map<const wstring*, float, StringPointerHashW, StringPointerEqualW> scoreShort, scoreLong;
	vector<future<void>> futures;
	futures.emplace_back(
		async(launch::async, searchShortW, ref(queryStr), ref(scoreShort))
	);
	futures.emplace_back(
		async(launch::async, searchLongW, ref(queryStr), ref(scoreLong))
	);
	for (auto& fu : futures)
		fu.get();

	//merge
	for (auto& keyPair : scoreLong)
		scoreShort[keyPair.first] += keyPair.second;
	vector<pair<const wstring*, float>> scoreElems(scoreShort.begin(), scoreShort.end());

	sort(scoreElems.begin(), scoreElems.end(), compareScoresW);
	for (size_t i = 0; i < scoreElems.size() && i < limit && scoreElems[i].second >= threshold; i++)
		result.push_back(scoreElems[i].first);
}

DLLEXP bool search(const char* query, char*** results, uint32_t* nStrings, const float threshold, uint32_t limit)
{
	if (wordMap.size() == 0 || gramSize == 0)
		return false;

	if (limit == 0)
		limit = (numeric_limits<int32_t>::max)();

	vector<const string*> result;

	search(query, threshold, limit, result);

	//transform to C ABI using pointers
	*nStrings = (uint32_t)result.size();
	*results = new char*[*nStrings];
	for (uint32_t i = 0; i < *nStrings; i++)
	{
		auto item = result[i];
		auto resStr = item->c_str();
		auto len = item->size();
		(*results)[i] = new char[len + 1]();
		memcpy((*results)[i], resStr, len);
	}
	return true;
}

DLLEXP bool searchW(const wchar_t* query, wchar_t*** results, uint32_t* nStrings, const float threshold, uint32_t limit)
{
	if (wordMap.size() == 0 || gramSize == 0)
		return false;

	if (limit == 0)
		limit = (numeric_limits<int32_t>::max)();

	vector<const wstring*> result;

	searchW(query, threshold, limit, result);

	//transform to C ABI using pointers
	*nStrings = (uint32_t)result.size();
	*results = new wchar_t*[*nStrings];
	for (uint32_t i = 0; i < *nStrings; i++)
	{
		auto item = result[i];
		auto resStr = item->c_str();
		auto len = item->size();
		(*results)[i] = new wchar_t[len + 1]();
		memcpy((*results)[i], resStr, len);
	}
	return true;
}

DLLEXP void release(char*** results, size_t nStrings)
{
	if (*results)
	{
		for (int i = 0; i < nStrings; i++)
			delete[](*results)[i];
		delete[](*results);
	}
}

DLLEXP void releaseW(wchar_t*** results, size_t nStrings)
{
	if (*results)
	{
		for (int i = 0; i < nStrings; i++)
			delete[](*results)[i];
		delete[](*results);
	}
}

DLLEXP void clear()
{
	longLib.clear();
	shortLib.clear();
	wordMap.clear();
	ngrams.clear();
}

DLLEXP void clearW()
{
	longLibW.clear();
	shortLibW.clear();
	wordMapW.clear();
	ngramsW.clear();
}
