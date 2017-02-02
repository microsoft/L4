#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "LocalMemory/HashTableService.h"

using namespace L4;

int main()
{
    EpochManagerConfig epochConfig{ 1000, std::chrono::milliseconds(100), 1 };
    LocalMemory::HashTableService service{ epochConfig };

    auto hashTableIndex = service.AddHashTable(
        HashTableConfig("Table1", HashTableConfig::Setting{ 1000000 }));

    std::vector<std::pair<std::string, std::string>> keyValuePairs =
    {
        { "key1", "value1"},
        { "key2", "value2" },
        { "key3", "value3" },
        { "key4", "value4" },
        { "key5", "value5" },
    };

    // Write data.
    {
        auto context = service.GetContext();
        auto& hashTable = context[hashTableIndex];

        for (const auto& keyValuePair : keyValuePairs)
        {
            const auto& keyStr = keyValuePair.first;
            const auto& valStr = keyValuePair.second;

            IWritableHashTable::Key key;
            key.m_data = reinterpret_cast<const std::uint8_t*>(keyStr.c_str());
            key.m_size = keyStr.size();

            IWritableHashTable::Value val;
            val.m_data = reinterpret_cast<const std::uint8_t*>(valStr.c_str());
            val.m_size = valStr.size();

            
            hashTable.Add(key, val);
        }
    }

    // Read data.
    {
        auto context = service.GetContext();
        auto& hashTable = context[hashTableIndex];

        for (const auto& keyValuePair : keyValuePairs)
        {
            const auto& keyStr = keyValuePair.first;

            IWritableHashTable::Key key;
            key.m_data = reinterpret_cast<const std::uint8_t*>(keyStr.c_str());
            key.m_size = keyStr.size();

            IWritableHashTable::Value val;
            hashTable.Get(key, val);

            std::cout << std::string(reinterpret_cast<const char*>(val.m_data), val.m_size) << std::endl;
        }
    }

    return 0;
}

