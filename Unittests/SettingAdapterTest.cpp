#include <boost/test/unit_test.hpp>
#include "L4/HashTable/Common/SettingAdapter.h"
#include "L4/HashTable/Common/Record.h"
#include "CheckedAllocator.h"

namespace L4
{
namespace UnitTests
{

using SharedHashTable = HashTable::SharedHashTable<HashTable::RecordBuffer, CheckedAllocator<>>;

BOOST_AUTO_TEST_SUITE(SettingAdapterTests)

BOOST_AUTO_TEST_CASE(SettingAdapterTestWithDefaultValues)
{
    HashTableConfig::Setting from{ 100U };
    const auto to = HashTable::SettingAdapter{}.Convert<SharedHashTable>(from);

    BOOST_CHECK_EQUAL(to.m_numBuckets, 100U);
    BOOST_CHECK_EQUAL(to.m_numBucketsPerMutex, 1U);
    BOOST_CHECK_EQUAL(to.m_fixedKeySize, 0U);
    BOOST_CHECK_EQUAL(to.m_fixedValueSize, 0U);
}


BOOST_AUTO_TEST_CASE(SettingAdapterTestWithNonDefaultValues)
{
    HashTableConfig::Setting from{ 100U, 10U, 5U, 20U };
    const auto to = HashTable::SettingAdapter{}.Convert<SharedHashTable>(from);

    BOOST_CHECK_EQUAL(to.m_numBuckets, 100U);
    BOOST_CHECK_EQUAL(to.m_numBucketsPerMutex, 10U);
    BOOST_CHECK_EQUAL(to.m_fixedKeySize, 5U);
    BOOST_CHECK_EQUAL(to.m_fixedValueSize, 20U);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace UnitTests
} // namespace L4
