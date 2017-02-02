#include "stdafx.h"
#include "Utils.h"
#include "Mocks.h"
#include "L4/HashTable/Config.h"
#include "L4/HashTable/IHashTable.h"
#include "L4/LocalMemory/HashTableManager.h"

namespace L4
{
namespace UnitTests
{

template <typename Store>
static void ValidateRecord(
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

BOOST_AUTO_TEST_CASE(HashTableManagerTest)
{
    MockEpochManager epochManager;
    PerfData perfData;

    LocalMemory::HashTableManager htManager;
    const auto ht1Index = htManager.Add(
        HashTableConfig("HashTable1", HashTableConfig::Setting(100U)),
        epochManager,
        std::allocator<void>());
    const auto ht2Index = htManager.Add(
        HashTableConfig("HashTable2", HashTableConfig::Setting(200U)),
        epochManager,
        std::allocator<void>());

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

} // namespace UnitTests
} // namespace L4