/* SPDX-License-Identifier: GPL-2.0-only */
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/tcp-qt-sdr.h"
#include "ns3/tcp-qt-tor.h"
#include "ns3/tcp-socket-state.h"
#include "ns3/test.h"

#include <cmath>
#include <limits>

using namespace ns3;

/** @ingroup internet-test @brief Regression checks for Qt TCP safety and control. */
class TcpQtTest : public TestCase
{
  public:
    TcpQtTest()
        : TestCase("Qt TCP RTT samples, fallback, bounds, copying and optimiser")
    {
    }

  private:
    /**
     * @brief Exercise shared congestion-controller invariants.
     * @tparam T Congestion-control class.
     */
    template <typename T>
    void CheckController()
    {
        auto algorithm = CreateObject<T>();
        NS_TEST_ASSERT_MSG_EQ(algorithm->SetAttributeFailSafe("targetRttAlpha", DoubleValue(0.0)),
                              false,
                              "Zero target RTT multiplier must be rejected");
        auto state = CreateObject<TcpSocketState>();
        state->m_segmentSize = 1000;
        state->m_cWnd = 10000;
        state->m_ssThresh = 100000;
        state->m_nextTxSequence = SequenceNumber32(10000);
        NS_TEST_ASSERT_MSG_EQ(algorithm->ComputeCwnd(state),
                              10000,
                              "No valid RTT must preserve the current window");
        algorithm->PktsAcked(state, 1, MilliSeconds(-1));
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetMinRtt(),
                              Time::Max(),
                              "Negative RTT samples must be ignored");
        // The ACK argument is valid even when the socket's last RTT is still zero.
        algorithm->PktsAcked(state, 1, MilliSeconds(100));
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetMinRtt(),
                              MilliSeconds(100),
                              "Must use the supplied ACK RTT");
        algorithm->PktsAcked(state, 1, Time(0));
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetMinRtt(),
                              MilliSeconds(100),
                              "Zero RTT must not erase the minimum");
        state->m_lastRtt = MilliSeconds(100);
        algorithm->IncreaseWindow(state, 1);
        NS_TEST_ASSERT_MSG_GT(state->m_cWnd.Get(),
                              10000,
                              "Sparse samples must use Reno rather than stall");
        const auto previous = state->m_cWnd.Get();
        algorithm->IncreaseWindow(state, 0);
        NS_TEST_ASSERT_MSG_EQ(state->m_cWnd.Get(), previous, "No ACK means no increase");
        state->m_congState = TcpSocketState::CA_RECOVERY;
        algorithm->IncreaseWindow(state, 1);
        NS_TEST_ASSERT_MSG_EQ(state->m_cWnd.Get(),
                              previous,
                              "Delay control must not undo loss recovery");
        state->m_congState = TcpSocketState::CA_OPEN;
        state->m_segmentSize = 0;
        NS_TEST_ASSERT_MSG_EQ(algorithm->ComputeCwnd(state),
                              previous,
                              "Zero MSS must not divide by zero");
        state->m_segmentSize = 1000;
        const uint32_t inflight = 4000000000U;
        NS_TEST_ASSERT_MSG_EQ(algorithm->GetSsThresh(state, inflight),
                              3800000000U,
                              "Loss reduction must not overflow");
        auto clone = DynamicCast<T>(algorithm->Fork());
        NS_TEST_ASSERT_MSG_EQ(clone->GetMinRtt(),
                              algorithm->GetMinRtt(),
                              "Fork must preserve RTT state");
        NS_TEST_ASSERT_MSG_EQ(clone->GetTargetRttAlpha(),
                              algorithm->GetTargetRttAlpha(),
                              "Fork must preserve configuration");
    }

    void DoRun() override
    {
        CheckController<TcpQtSdr>();
        CheckController<TcpQtTor>();
        auto sdr = CreateObject<TcpQtSdr>();
        auto state = CreateObject<TcpSocketState>();
        state->m_segmentSize = 1000;
        state->m_cWnd = 100000;
        state->m_bytesInFlight = 100000000;
        sdr->PktsAcked(state, 1, MilliSeconds(100));
        state->m_lastRtt = Seconds(1);
        sdr->SetMinRttEpsilon(0.0);
        NS_TEST_ASSERT_MSG_EQ(sdr->ComputeCwnd(state),
                              2000,
                              "Negative predicted window must clamp to two MSS");
        sdr->SetMinRttEpsilon(1.0);
        sdr->SetTargetRttMultiplier(1.0);
        state->m_cWnd = std::numeric_limits<uint32_t>::max();
        state->m_bytesInFlight = std::numeric_limits<uint32_t>::max();
        state->m_lastRtt = MilliSeconds(100);
        const auto boundedWindow = sdr->ComputeCwnd(state);
        NS_TEST_ASSERT_MSG_GT(boundedWindow,
                              4000000000U,
                              "Maximum window must saturate rather than wrap");
        auto copiedSdr = DynamicCast<TcpQtSdr>(sdr->Fork());
        NS_TEST_ASSERT_MSG_EQ(copiedSdr->m_predInflight.Get(),
                              sdr->m_predInflight.Get(),
                              "Fork must copy predicted in-flight trace value");

        auto tor = CreateObject<TcpQtTor>();
        tor->PktsAcked(state, 1, MilliSeconds(100));
        tor->UpdateServiceRateEstimate(100.0, 0.1);
        const QtTorV2State initial{100.0, 100.0, 0.1};
        const double nominal = (100.0 - 100.0 / 1.2) / 0.1;
        std::vector<double> baseline(5, nominal);
        const auto initialCost =
            tor->ComputeCost(tor->Integrate(initial, baseline), baseline, nominal);
        const auto controls = tor->OptimizeControl(initial);
        const auto finalCost =
            tor->ComputeCost(tor->Integrate(initial, controls), controls, nominal);
        NS_TEST_ASSERT_MSG_EQ(std::isfinite(finalCost), true, "Optimisation cost must be finite");
        NS_TEST_ASSERT_MSG_LT_OR_EQ(finalCost,
                                    initialCost,
                                    "Accepted optimiser steps must not increase cost");
        NS_TEST_ASSERT_MSG_EQ(tor->ComputeNextW(100.0, {}, 0.1),
                              100.0,
                              "Empty controls must preserve the current window");
        NS_TEST_ASSERT_MSG_EQ(std::isinf(tor->ComputeCost({}, {1.0}, 1.0)),
                              true,
                              "Mismatched trajectory must be rejected");
        const std::vector<double> shortControls{0.0};
        NS_TEST_ASSERT_MSG_EQ(
            std::isfinite(
                tor->ComputeCost(tor->Integrate(initial, shortControls), shortControls, 1.0)),
            true,
            "Short horizons must not read out of bounds");
        NS_TEST_ASSERT_MSG_EQ(tor->dRdt(0.0, 10.0, 1.0, 1.0),
                              0.0,
                              "Zero RTT must not produce a NaN derivative");
        NS_TEST_ASSERT_MSG_EQ(tor->dR_du_model(0.1, 100.0, 1.0, 0.0),
                              0.0,
                              "Zero time interval must be rejected");
        const double derivative = tor->central_diff_dR_du(0.1, 100.0, 100.0, 1.0, 0.01, 0.001);
        const double terminalDerivative =
            tor->propagate_terminal_dRdu({0.1}, {100.0}, {1.0}, {100.0}, 0, 0.01);
        NS_TEST_ASSERT_MSG_EQ_TOL(derivative,
                                  terminalDerivative,
                                  1e-6,
                                  "One-step and terminal sensitivity must agree");
        NS_TEST_ASSERT_MSG_EQ(tor->propagate_terminal_dRdu({}, {}, {}, {}, 0, 0.1),
                              0.0,
                              "Empty sensitivity input must be safe");
        Simulator::Schedule(Seconds(11), [this, tor, state]() {
            tor->PktsAcked(state, 1, MilliSeconds(150));
            NS_TEST_ASSERT_MSG_EQ(tor->GetMinRtt(),
                                  MilliSeconds(150),
                                  "Expired minimum RTT must adapt to a longer path");
        });
        Simulator::Run();
        Simulator::Destroy();
    }
};

/** @ingroup internet-test @brief Qt congestion-control regression suite. */
class TcpQtTestSuite : public TestSuite
{
  public:
    TcpQtTestSuite()
        : TestSuite("tcp-qt", Type::UNIT)
    {
        AddTestCase(new TcpQtTest, TestCase::Duration::QUICK);
    }
};

static TcpQtTestSuite g_tcpQtTestSuite; //!< Register the Qt TCP regression tests.
