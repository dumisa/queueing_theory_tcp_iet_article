
// Include a header file from your module to test.
#include "ns3/my_maths_tools.h"

// An essential include is test.h
#include "ns3/test.h"

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;

// Add a doxygen group for tests.
// If you have more than one test, this should be in only one of them.
/**
 * \defgroup my_maths_tools-tests Tests for my_maths_tools
 * \ingroup my_maths_tools
 * \ingroup tests
 */

// This is an example TestCase.
/**
 * \ingroup my_maths_tools-tests
 * Test case for feature 1
 */
class My_maths_toolsTestCase1 : public TestCase
{
  public:
    My_maths_toolsTestCase1();
    virtual ~My_maths_toolsTestCase1();

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
My_maths_toolsTestCase1::My_maths_toolsTestCase1()
    : TestCase("My_maths_tools test case (does nothing)")
{
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
My_maths_toolsTestCase1::~My_maths_toolsTestCase1()
{
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
My_maths_toolsTestCase1::DoRun()
{
    // A wide variety of test macros are available in src/core/test.h
    NS_TEST_ASSERT_MSG_EQ(true, true, "true doesn't equal true for some reason");
    // Use this one for floating point comparisons
    NS_TEST_ASSERT_MSG_EQ_TOL(0.01, 0.01, 0.001, "Numbers are not equal within tolerance");
}

// The TestSuite class names the TestSuite, identifies what type of TestSuite,
// and enables the TestCases to be run.  Typically, only the constructor for
// this class must be defined

/**
 * \ingroup my_maths_tools-tests
 * TestSuite for module my_maths_tools
 */
class My_maths_toolsTestSuite : public TestSuite
{
  public:
    My_maths_toolsTestSuite();
};

My_maths_toolsTestSuite::My_maths_toolsTestSuite()
    : TestSuite("my_maths_tools", Type::UNIT)
{
    // TestDuration for TestCase can be QUICK, EXTENSIVE or TAKES_FOREVER
    AddTestCase(new My_maths_toolsTestCase1, TestCase::Duration::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
/**
 * \ingroup my_maths_tools-tests
 * Static variable for test initialization
 */
static My_maths_toolsTestSuite smy_maths_toolsTestSuite;
