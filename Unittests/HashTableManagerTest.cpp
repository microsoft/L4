#include <boost/test/unit_test.hpp>
#include "Utils.h"
#include "Mocks.h"
#include "L4/HashTable/Config.h"
#include "L4/HashTable/IHashTable.h"
#include "L4/LocalMemory/HashTableManager.h"

namespace L4
{
    namespace UnitTests
    {

        class HashTableManagerTestsFixture
        {
        protected:
            template <typename Store>
            void ValidateRecord(
                const Store& store,
                const char* expectedKeyStr,
                const char* expectedValueStr)
            {
                IReadOnlyHashTable::Value actualValue;
                auto expectedValue = Utils::ConvertFromString<IReadOnlyHashTable::Value>(expectedValueStr);
                BOOST_CHECK(store.Get(Utils::ConvertFromString<IReadOnlyHashTable::Key>(expectedKeyStr), actualValue));
                BOOST_CHECK(actualValue.m_size == expectedValue.m_size);
                BOOST_CHECK(!memcmp(actualValue.m_data, expectedValue.m_data, expectedValue.m_size));
            }

            MockEpochManager m_epochManager;
            std::allocator<void> m_allocator;
        };

        BOOST_FIXTURE_TEST_SUITE(HashTableManagerTests, HashTableManagerTestsFixture)

            BOOST_AUTO_TEST_CASE(HashTableManagerTest)
        {
            LocalMemory::HashTableManager htManager;
            const auto ht1Index = htManager.Add(
                HashTableConfig("HashTable1", HashTableConfig::Setting(100U)),
                m_epochManager,
                m_allocator);
            const auto ht2Index = htManager.Add(
                HashTableConfig("HashTable2", HashTableConfig::Setting(200U)),
                m_epochManager,
                m_allocator);

            {
                auto& hashTable1 = htManager.GetHashTable("HashTable1");
                hashTable1.Add(
                    Utils::ConvertFromString<IReadOnlyHashTable::Key>("HashTable1Key"),
                    Utils::ConvertFromString<IReadOnlyHashTable::Value>("HashTable1Value"));

                auto& hashTable2 = htManager.GetHashTable("HashTable2");
                hashTable2.Add(
                    Utils::ConvertFromString<IReadOnlyHashTable::Key>("HashTable2Key"),
                    Utils::ConvertFromString<IReadOnlyHashTable::Value>("HashTable2Value"));
            }

            ValidateRecord(
                htManager.GetHashTable(ht1Index),
                "HashTable1Key",
                "HashTable1Value");

            ValidateRecord(
                htManager.GetHashTable(ht2Index),
                "HashTable2Key",
                "HashTable2Value");
        }


        BOOST_AUTO_TEST_CASE(HashTableManagerTestForSerialzation)
        {
            HashTableConfig htConfig{ "HashTable1", HashTableConfig::Setting(100U) };
            std::ostringstream outStream;

            std::vector<std::pair<std::string, std::string>> testData;
            for (std::int32_t i = 0; i < 10; ++i)
            {
                testData.emplace_back(
                    "key" + std::to_string(i),
                    "val" + std::to_string(i));
            }

            // Serialize a hash table.
            {
                LocalMemory::HashTableManager htManager;
                const auto ht1Index = htManager.Add(htConfig, m_epochManager, m_allocator);

                auto& hashTable1 = htManager.GetHashTable("HashTable1");

                for (const auto& kvPair : testData)
                {
                    hashTable1.Add(
                        Utils::ConvertFromString<IReadOnlyHashTable::Key>(kvPair.first.c_str()),
                        Utils::ConvertFromString<IReadOnlyHashTable::Value>(kvPair.second.c_str()));
                }

                auto serializer = hashTable1.GetSerializer();
                serializer->Serialize(outStream, {});
            }

            // Deserialize the hash table.
            {
                htConfig.m_serializer.emplace(
                    std::make_shared<std::istringstream>(outStream.str()));

                LocalMemory::HashTableManager htManager;
                const auto ht1Index = htManager.Add(htConfig, m_epochManager, m_allocator);

                auto& hashTable1 = htManager.GetHashTable("HashTable1");
                BOOST_CHECK_EQUAL(
                    hashTable1.GetPerfData().Get(HashTablePerfCounter::RecordsCount),
                    testData.size());

                for (const auto& kvPair : testData)
                {
                    ValidateRecord(
                        hashTable1,
                        kvPair.first.c_str(),
                        kvPair.second.c_str());
                }
            }
        }

        BOOST_AUTO_TEST_SUITE_END()

    } // namespace UnitTests
} // namespace L4
