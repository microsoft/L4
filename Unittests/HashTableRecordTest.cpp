#include <boost/test/unit_test.hpp>
#include <boost/optional.hpp>
#include <string>
#include <vector>
#include "L4/HashTable/Common/Record.h"
#include "Utils.h"

namespace L4
{
    namespace UnitTests
    {

        using namespace HashTable;

        class HashTableRecordTestFixture
        {
        protected:
            void Run(bool isFixedKey, bool isFixedValue, bool useMetaValue)
            {
                BOOST_TEST_MESSAGE(
                    "Running with isFixedKey=" << isFixedKey
                    << ", isFixedValue=" << isFixedValue
                    << ", useMetatValue=" << useMetaValue);

                const std::string key = "TestKey";
                const std::string value = "TestValue";
                const std::string metaValue = "TestMetavalue";

                const auto recordOverhead = (isFixedKey ? 0U : c_keyTypeSize) + (isFixedValue ? 0U : c_valueTypeSize);

                Validate(
                    RecordSerializer{
                    isFixedKey ? static_cast<RecordSerializer::KeySize>(key.size()) : std::uint16_t(0),
                    isFixedValue ? static_cast<RecordSerializer::ValueSize>(value.size()) : 0U,
                    useMetaValue ? static_cast<RecordSerializer::ValueSize>(metaValue.size()) : 0U },
                    key,
                    value,
                    recordOverhead + key.size() + value.size() + (useMetaValue ? metaValue.size() : 0U),
                    recordOverhead,
                    useMetaValue ? boost::optional<const std::string&>{ metaValue } : boost::none);
            }

        private:
            void Validate(
                const RecordSerializer& serializer,
                const std::string& keyStr,
                const std::string& valueStr,
                std::size_t expectedBufferSize,
                std::size_t expectedRecordOverheadSize,
                boost::optional<const std::string&> metadataStr = boost::none)
            {
                BOOST_CHECK_EQUAL(serializer.CalculateRecordOverhead(), expectedRecordOverheadSize);

                const auto key = Utils::ConvertFromString<Record::Key>(keyStr.c_str());
                const auto value = Utils::ConvertFromString<Record::Value>(valueStr.c_str());

                const auto bufferSize = serializer.CalculateBufferSize(key, value);

                BOOST_REQUIRE_EQUAL(bufferSize, expectedBufferSize);
                std::vector<std::uint8_t> buffer(bufferSize);

                RecordBuffer* recordBuffer = nullptr;

                if (metadataStr)
                {
                    auto metaValue = Utils::ConvertFromString<Record::Value>(metadataStr->c_str());
                    recordBuffer = serializer.Serialize(key, value, metaValue, buffer.data(), bufferSize);
                }
                else
                {
                    recordBuffer = serializer.Serialize(key, value, buffer.data(), bufferSize);
                }

                const auto record = serializer.Deserialize(*recordBuffer);

                // Make sure the data serialized is in different memory location.
                BOOST_CHECK(record.m_key.m_data != key.m_data);
                BOOST_CHECK(record.m_value.m_data != value.m_data);

                BOOST_CHECK(record.m_key == key);
                if (metadataStr)
                {
                    const std::string newValueStr = *metadataStr + valueStr;
                    const auto newValue = Utils::ConvertFromString<Record::Value>(newValueStr.c_str());
                    BOOST_CHECK(record.m_value == newValue);
                }
                else
                {
                    BOOST_CHECK(record.m_value == value);
                }
            }

            static constexpr std::size_t c_keyTypeSize = sizeof(Record::Key::size_type);
            static constexpr std::size_t c_valueTypeSize = sizeof(Record::Value::size_type);
        };

        BOOST_FIXTURE_TEST_SUITE(HashTableRecordTests, HashTableRecordTestFixture)

            BOOST_AUTO_TEST_CASE(RunAll)
        {
            // Run all permutations for Run(), which takes three booleans.
            for (int i = 0; i < 8; ++i)
            {
                Run(
                    !!((i >> 2) & 1),
                    !!((i >> 1) & 1),
                    !!((i) & 1));
            }
        }


        BOOST_AUTO_TEST_CASE(InvalidSizeTest)
        {
            std::vector<std::uint8_t> buffer(100U);

            RecordSerializer serializer{ 4, 5 };

            const std::string keyStr = "1234";
            const std::string invalidStr = "999999";
            const std::string valueStr = "12345";

            const auto key = Utils::ConvertFromString<Record::Key>(keyStr.c_str());
            const auto value = Utils::ConvertFromString<Record::Value>(valueStr.c_str());

            const auto invalidKey = Utils::ConvertFromString<Record::Key>(invalidStr.c_str());
            const auto invalidValue = Utils::ConvertFromString<Record::Value>(invalidStr.c_str());

            CHECK_EXCEPTION_THROWN_WITH_MESSAGE(
                serializer.Serialize(invalidKey, value, buffer.data(), buffer.size()),
                "Invalid key or value sizes are given.");

            CHECK_EXCEPTION_THROWN_WITH_MESSAGE(
                serializer.Serialize(key, invalidValue, buffer.data(), buffer.size()),
                "Invalid key or value sizes are given.");

            CHECK_EXCEPTION_THROWN_WITH_MESSAGE(
                serializer.Serialize(invalidKey, invalidValue, buffer.data(), buffer.size()),
                "Invalid key or value sizes are given.");

            // Normal case shouldn't thrown an exception.
            serializer.Serialize(key, value, buffer.data(), buffer.size());

            RecordSerializer serializerWithMetaValue{ 4, 5, 2 };
            std::uint16_t metadata = 0;

            Record::Value metaValue{
                reinterpret_cast<std::uint8_t*>(&metadata),
                sizeof(metadata) };

            // Normal case shouldn't thrown an exception.
            serializerWithMetaValue.Serialize(key, value, metaValue, buffer.data(), buffer.size());

            // Mismatching size is given.
            metaValue.m_size = 1;
            CHECK_EXCEPTION_THROWN_WITH_MESSAGE(
                serializerWithMetaValue.Serialize(key, value, metaValue, buffer.data(), buffer.size()),
                "Invalid meta value size is given.");
        }

        BOOST_AUTO_TEST_SUITE_END()

    } // namespace UnitTests
} // namespace L4
