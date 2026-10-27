/* SPDX-License-Identifier: GPL-2.0-only */

#include "ns3/tcp-qt-sdr.h"
#include "ns3/tcp-qt-tor.h"
#include "ns3/tcp-socket-state.h"
#include "ns3/test.h"

using namespace ns3;

/** @brief Regression checks for the default QtSDR rule copied from ns-3.47. */
class TcpQtDefaultRuleTest : public TestCase
{
  public:
    TcpQtDefaultRuleTest()
        : TestCase("QtSDR default rule, RTT tracking, and loss response")
    {
    }

  private:
    void DoRun() override
    {
        auto algorithm = CreateObject<TcpQtSdr>();
        auto state = CreateObject<TcpSocketState>();
        state->m_segmentSize = 1000;
        state->m_cWnd = 100000;
        state->m_bytesInFlight = 100000;
        state->m_lastRtt = MilliSeconds(40);
        algorithm->PktsAcked(state, 1, MilliSeconds(40));
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetMinRtt(), MilliSeconds(40), "Propagation sample");
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetTargetRtt(), MilliSeconds(48), "Default alpha=1.2");
        state->m_lastRtt = MilliSeconds(48);
        NS_TEST_ASSERT_MSG_EQ(algorithm->ComputeCwnd(state), 101000, "One-packet excitation at target");
        state->m_lastRtt = MilliSeconds(47);
        NS_TEST_ASSERT_MSG_EQ(algorithm->ComputeCwnd(state), 102000, "Below-target correction");
        state->m_lastRtt = MilliSeconds(49);
        NS_TEST_ASSERT_MSG_EQ(algorithm->ComputeCwnd(state), 99000, "Above-target correction");
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetSsThresh(state, 100000), 95000, "Loss factor 0.95");
        auto clone = DynamicCast<TcpQtSdr>(algorithm->Fork());
        NS_TEST_ASSERT_MSG_EQ(clone->GetMinRtt(), algorithm->GetMinRtt(), "Fork preserves RTT");
        NS_TEST_ASSERT_MSG_EQ(clone->GetTargetRttAlpha(), algorithm->GetTargetRttAlpha(), "Fork preserves alpha");
    }
};

/** @brief Regression checks for the QtTOR configuration copied from ns-3.47. */
class TcpQtTorPortTest : public TestCase
{
  public:
    TcpQtTorPortTest()
        : TestCase("QtTOR RTT tracking, loss response, and clone state")
    {
    }

  private:
    void DoRun() override
    {
        auto algorithm = CreateObject<TcpQtTor>();
        auto state = CreateObject<TcpSocketState>();
        state->m_segmentSize = 1000;
        state->m_cWnd = 100000;
        state->m_bytesInFlight = 100000;
        state->m_lastRtt = MilliSeconds(40);
        algorithm->PktsAcked(state, 1, MilliSeconds(40));
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetMinRtt(), MilliSeconds(40), "Initial RTT sample");
        algorithm->PktsAcked(state, 1, MilliSeconds(35));
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetMinRtt(), MilliSeconds(35), "Lower sample updates baseline");
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetTargetRtt(), MilliSeconds(42), "Default target multiplier");
        algorithm->PktsAcked(state, 1, Time(0));
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetMinRtt(), MilliSeconds(35), "Zero sample is ignored");
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetSsThresh(state, 100000), 95000, "Loss factor 0.95");
        auto clone = DynamicCast<TcpQtTor>(algorithm->Fork());
        NS_TEST_ASSERT_MSG_EQ(clone->GetMinRtt(), algorithm->GetMinRtt(), "Fork preserves RTT");
        NS_TEST_ASSERT_MSG_EQ(clone->GetTargetRttAlpha(), algorithm->GetTargetRttAlpha(), "Fork preserves alpha");
    }
};

/** @brief Tests for the current default Qt model port. */
class TcpQtTestSuite : public TestSuite
{
  public:
    TcpQtTestSuite()
        : TestSuite("tcp-qt", Type::UNIT)
    {
        AddTestCase(new TcpQtDefaultRuleTest, TestCase::Duration::QUICK);
        AddTestCase(new TcpQtTorPortTest, TestCase::Duration::QUICK);
    }
};

static TcpQtTestSuite g_tcpQtTestSuite; ///< Register the Qt regression suite.
