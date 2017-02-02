#include "stdafx.h"
#include "L4/Utils/Math.h"
#include <array>

namespace L4
{
namespace UnitTests
{

using namespace Utils;

BOOST_AUTO_TEST_CASE(MathTest)
{
    // RoundUp tests.
    BOOST_CHECK_EQUAL(Math::RoundUp(5, 10), 10);
    BOOST_CHECK_EQUAL(Math::RoundUp(10, 10), 10);
    BOOST_CHECK_EQUAL(Math::RoundUp(11, 10), 20);
    BOOST_CHECK_EQUAL(Math::RoundUp(5, 0), 5);

    // RoundDown tests.
    BOOST_CHECK_EQUAL(Math::RoundDown(5, 10), 0);
    BOOST_CHECK_EQUAL(Math::RoundDown(10, 10), 10);
    BOOST_CHECK_EQUAL(Math::RoundDown(11, 10), 10);
    BOOST_CHECK_EQUAL(Math::RoundDown(5, 0), 5);

    // IsPowerOfTwo tests.
    BOOST_CHECK(Math::IsPowerOfTwo(2));
    BOOST_CHECK(Math::IsPowerOfTwo(4));
    BOOST_CHECK(!Math::IsPowerOfTwo(3));
    BOOST_CHECK(!Math::IsPowerOfTwo(0));

    // NextHighestPowerOfTwo tests.
    BOOST_CHECK_EQUAL(Math::NextHighestPowerOfTwo(0), 0U);
    BOOST_CHECK_EQUAL(Math::NextHighestPowerOfTwo(1), 1U);
    BOOST_CHECK_EQUAL(Math::NextHighestPowerOfTwo(2), 2U);
    BOOST_CHECK_EQUAL(Math::NextHighestPowerOfTwo(3), 4U);
    BOOST_CHECK_EQUAL(Math::NextHighestPowerOfTwo(4), 4U);
    BOOST_CHECK_EQUAL(Math::NextHighestPowerOfTwo(5), 8U);
    BOOST_CHECK_EQUAL(Math::NextHighestPowerOfTwo(200), 256U);
}


BOOST_AUTO_TEST_CASE(PointerArithmeticTest)
{
    std::array<int, 3> elements;
    
    BOOST_CHECK(reinterpret_cast<int*>(Math::PointerArithmetic::Add(&elements[0], sizeof(int))) == &elements[1]);
    BOOST_CHECK(reinterpret_cast<int*>(Math::PointerArithmetic::Subtract(&elements[1], sizeof(int))) == &elements[0]);
    BOOST_CHECK(Math::PointerArithmetic::Distance(&elements[2], &elements[0]) == sizeof(int) * 2U);
    BOOST_CHECK(Math::PointerArithmetic::Distance(&elements[0], &elements[2]) == sizeof(int) * 2U);
}

} // namespace UnitTests
} // namespace L4
