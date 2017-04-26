#include <boost/test/unit_test.hpp>
#include <utility>
#include <vector>
#include "Mocks.h"
#include "Utils.h"
#include "L4/LocalMemory/HashTableService.h"

namespace L4
{
    namespace UnitTests
    {

        BOOST_AUTO_TEST_CASE(HashTableServiceTest)
        {
            std::vector<std::pair<std::string, std::string>> dataSet;
            for (std::uint16_t i = 0U; i < 100; ++i)
            {
                dataSet.emplace_back("key" + std::to_string(i), "value" + std::to_string(i));
            }

            LocalMemory::HashTableService htService;
            htService.AddHashTable(
                HashTableConfig("Table1", HashTableConfig::Setting{ 100U }));
            htService.AddHashTable(
                HashTableConfig(
                    "Table2",
                    HashTableConfig::Setting{ 1000U },
                    HashTableConfig::Cache{ 1024, std::chrono::seconds{ 1U }, false }));

            for (const auto& data : dataSet)
            {
                htService.GetContext()["Table1"].Add(
                    Utils::ConvertFromString<IReadOnlyHashTable::Key>(data.first.c_str()),
                    Utils::ConvertFromString<IReadOnlyHashTable::Value>(data.second.c_str()));
            }

            // Smoke tests for looking up the data .
            {
                auto context = htService.GetContext();
                for (const auto& data : dataSet)
                {
                    IReadOnlyHashTable::Value val;
                    BOOST_CHECK(context["Table1"].Get(
                        Utils::ConvertFromString<IReadOnlyHashTable::Key>(data.first.c_str()),
                        val));
                    BOOST_CHECK(Utils::ConvertToString(val) == data.second);
                }
            }
        }

    } // namespace UnitTests
} // namespace L4
