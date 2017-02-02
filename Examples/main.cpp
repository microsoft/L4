#include "Log/IPerfLogger.h"
#include "LocalMemory/HashTableService.h"
#include <string>
#include <iostream>
#include <fstream>
#include <tchar.h>
#include <stdarg.h> 
#include <memory>


using namespace L4;

class ConsolePerfLogger : public IPerfLogger
{
    virtual void Log(const IData& perfData) override
    {
        for (auto i = 0; i < static_cast<std::uint16_t>(ServerPerfCounter::Count); ++i)
        {
            std::cout << c_serverPerfCounterNames[i] << ": " 
                << perfData.GetServerPerfData().Get(static_cast<ServerPerfCounter>(i)) << std::endl;
        }
        
        const auto& hashTablesPerfData = perfData.GetHashTablesPerfData();

        for (const auto& entry : hashTablesPerfData)
        {
            std::cout << "Hash table '" << entry.first << "'" << std::endl;

            for (auto j = 0; j < static_cast<std::uint16_t>(HashTablePerfCounter::Count); ++j)
            {
                std::cout << c_hashTablePerfCounterNames[j] << ": " 
                    << entry.second.get().Get(static_cast<HashTablePerfCounter>(j)) << std::endl;
            }
        }

        std::cout << std::endl;
    }
};



int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    LocalMemory::HashTableService service;

    auto index = service.AddHashTable(
        HashTableConfig("Table1", HashTableConfig::Setting{ 1000000 }));

    static constexpr int keySize = 100;
    static constexpr int valSize = 2000;

    char bufKey[keySize];
    char bufVal[valSize];

    IWritableHashTable::Key key;
    key.m_data = reinterpret_cast<std::uint8_t*>(bufKey);
    IWritableHashTable::Value val;
    val.m_data = reinterpret_cast<std::uint8_t*>(bufVal);

    std::ifstream file;
    file.open(argv[1], std::ifstream::in);
    std::cout << "Opening " << argv[1] << std::endl;
    static const int BufferLength = 4096;

    char buffer[BufferLength];

    auto totalTime = 0U;
    int numLines = 0;
    while (file.getline(buffer, BufferLength))
    {
        auto context = service.GetContext();

        auto& hashTable = context[index];

        char* nextToken = nullptr;
        const char* keyStr = strtok_s(buffer, "\t", &nextToken);
        const char* valStr = strtok_s(nullptr, "\t", &nextToken);

        key.m_data = reinterpret_cast<const std::uint8_t*>(keyStr);
        key.m_size = static_cast<std::uint16_t>(strlen(keyStr));

        val.m_data = reinterpret_cast<const std::uint8_t*>(valStr);
        val.m_size = static_cast<std::uint32_t>(strlen(valStr));

        hashTable.Add(key, val);

        ++numLines;
    }

    std::cout<< "Total Add() time" << totalTime << std::endl;
    std::cout << "Added " << numLines << " lines." << std::endl;

    return 0;
}

