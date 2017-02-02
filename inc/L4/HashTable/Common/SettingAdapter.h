#pragma once

#include <cstdint>
#include "HashTable/Common/SharedHashTable.h"
#include "HashTable/Config.h"

namespace L4
{
namespace HashTable
{

// SettingAdapter class provides a functionality to convert a HashTableConfig::Setting object
// to a SharedHashTable::Setting object.
class SettingAdapter
{
public:
    template <typename SharedHashTable>
    typename SharedHashTable::Setting Convert(const HashTableConfig::Setting& from) const
    {
        typename SharedHashTable::Setting to;

        to.m_numBuckets = from.m_numBuckets;
        to.m_numBucketsPerMutex = (std::max)(from.m_numBucketsPerMutex.get_value_or(1U), 1U);
        to.m_fixedKeySize = from.m_fixedKeySize.get_value_or(0U);
        to.m_fixedValueSize = from.m_fixedValueSize.get_value_or(0U);

        return to;
    }
};

} // namespace HashTable
} // namespace L4