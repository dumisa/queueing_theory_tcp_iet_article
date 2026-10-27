/*
 * Copyright (c) 2024 Dumisa Ngwenya <dumisa@crocs.co.za; ngwenyad@sentech.co.za>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Developed as part of PhD work on TCP Congestion Control
 * Dumisa W. Ngwenya - University of Pretoria - Department of Electrical, Electronic and Computer
 * Engineering
 *
 * Adapted from TCP Vegas
 *
 */

#include "tcp-qt-sdr.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/tcp-socket-state.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpQtSdr");
NS_OBJECT_ENSURE_REGISTERED(TcpQtSdr);

TypeId
TcpQtSdr::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TcpQtSdr")
            .SetParent<TcpNewReno>()
            .AddConstructor<TcpQtSdr>()
            .SetGroupName("Internet")
            .AddAttribute("targetRttAlpha",
                          "multiplication factor for Rtarget in proportion to Rmin",
                          DoubleValue(1.2),
                          MakeDoubleAccessor(&TcpQtSdr::SetTargetRttMultiplier),
                          MakeDoubleChecker<double>(1.0, std::numeric_limits<double>::max()))
            .AddAttribute("minRttEpsilon",
                          "Proportion of time RTT in Rmin before it starts to evolve",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&TcpQtSdr::SetMinRttEpsilon),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddTraceSource("predInflight",
                            "Predicted inflight packets from trajectory optimization.",
                            MakeTraceSourceAccessor(&TcpQtSdr::m_predInflight),
                            "ns3::TracedValueCallback::Uint32")
            .AddAttribute("RttWindowLength",
                          "Length of RTT windowed filter",
                          TimeValue(Seconds(10)),
                          MakeTimeAccessor(&TcpQtSdr::m_minRttFilterLen),
                          MakeTimeChecker())
            .AddTraceSource("MinRtt",
                            "Estimated two-way round-trip propagation delay of the path, estimated "
                            "from the windowed minimum recent round-trip delay sample",
                            MakeTraceSourceAccessor(&TcpQtSdr::m_minRtt),
                            "ns3::TracedValueCallback::Time");
    return tid;
}

TcpQtSdr::TcpQtSdr()
    : TcpNewReno()
{
    NS_LOG_FUNCTION(this);
}

TcpQtSdr::TcpQtSdr(const TcpQtSdr& sock)
    : TcpNewReno(sock),
      m_predInflight(sock.m_predInflight),
      m_minRtt(sock.m_minRtt),
      m_minRttExpired(sock.m_minRttExpired),
      m_minRttFilterLen(sock.m_minRttFilterLen),
      m_minRttStamp(sock.m_minRttStamp),
      m_targetRtt(sock.m_targetRtt),
      m_targetRttAlpha(sock.m_targetRttAlpha),
      m_minRttEpsilon(sock.m_minRttEpsilon),
      m_probeRttDuration(sock.m_probeRttDuration),
      m_probeRttPropStamp(sock.m_probeRttPropStamp),
      m_priorCwnd(sock.m_priorCwnd),
      m_cntRtt(sock.m_cntRtt),
      m_probeRtt(sock.m_probeRtt),
      m_usePriorCwnd(sock.m_usePriorCwnd),
      m_begSndNxt(sock.m_begSndNxt),
      m_curRtt(sock.m_curRtt),
      m_prevRtt(sock.m_prevRtt),
      m_curCwnd(sock.m_curCwnd),
      m_prevCwnd(sock.m_prevCwnd),
      m_persistentRttGrowthCnt(sock.m_persistentRttGrowthCnt),
      m_persistentRttGrowthCntThreshold(sock.m_persistentRttGrowthCntThreshold)
{
    NS_LOG_FUNCTION(this);
}

TcpQtSdr::~TcpQtSdr(void)
{
    NS_LOG_FUNCTION(this);
}

Time
TcpQtSdr::GetMinRtt() const
{
    return m_minRtt.Get();
}

Time
TcpQtSdr::GetTargetRtt() const
{
    return m_targetRtt;
}

double
TcpQtSdr::GetTargetRttAlpha() const
{
    return m_targetRttAlpha;
}

double
TcpQtSdr::GetMinRttEpsilon() const
{
    return m_minRttEpsilon;
}

bool
TcpQtSdr::GetUsePriorInFlight() const
{
    return m_usePriorCwnd;
}

uint32_t
TcpQtSdr::GetPriorInFlight() const
{
    return m_priorCwnd;
}

void
TcpQtSdr::SetUsePriorInFlight(bool usePriorInFlight)
{
    m_usePriorCwnd = usePriorInFlight;
}

void
TcpQtSdr::SetTargetRttMultiplier(double targetRttAlpha)
{
    NS_ABORT_MSG_IF(!std::isfinite(targetRttAlpha) || targetRttAlpha < 1.0,
                    "Target RTT multiplier must be finite and at least one");
    m_targetRttAlpha = targetRttAlpha;
    if (m_minRtt.Get() != Time::Max())
    {
        m_targetRtt = m_targetRttAlpha * m_minRtt.Get();
    }
}

void
TcpQtSdr::SetMinRttEpsilon(double minRttEpsilon)
{
    NS_ABORT_MSG_IF(!std::isfinite(minRttEpsilon) || minRttEpsilon < 0.0 || minRttEpsilon > 1.0,
                    "RTT epsilon must be in [0, 1]");
    m_minRttEpsilon = minRttEpsilon;
}

void
TcpQtSdr::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);

    if (rtt <= Time(0) || rtt == Time::Max())
    {
        return;
    }

    UpdateRttSample(rtt);

    NS_LOG_DEBUG("Updated m_minRtt = " << m_minRtt);

    // Update RTT counter
    if (m_cntRtt < std::numeric_limits<uint32_t>::max())
    {
        ++m_cntRtt;
    }
    NS_LOG_DEBUG("Updated m_cntRtt = " << m_cntRtt);
}

void
TcpQtSdr::UpdateRttProp(Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION(this << tcb);

    UpdateRttSample(tcb->m_lastRtt.Get());
}

void
TcpQtSdr::UpdateRttSample(Time rtt)
{
    if (rtt <= Time(0) || rtt == Time::Max())
    {
        return;
    }
    if (rtt < m_minRtt.Get())
    {
        m_minRtt = rtt;
        m_targetRtt = m_targetRttAlpha * rtt;
        m_minRttStamp = Simulator::Now();
    }
}

void
TcpQtSdr::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (segmentsAcked == 0 || tcb->m_segmentSize == 0 ||
        tcb->m_congState != TcpSocketState::CA_OPEN)
    {
        return;
    }
    if (m_minRtt.Get() == Time::Max())
    {
        TcpNewReno::IncreaseWindow(tcb, segmentsAcked);
        return;
    }
    m_minRttExpired = Simulator::Now() > (m_minRttStamp + m_minRttFilterLen);
    m_persistentRttGrowthCnt=0;
    if ((m_minRttExpired || m_persistentRttGrowthCnt > m_persistentRttGrowthCntThreshold) &&
        !m_probeRtt)
    {
        m_probeRtt = true;
        m_probeRttPropStamp = Simulator::Now();
        m_priorCwnd = tcb->m_cWnd;
        tcb->m_cWnd = 4 * tcb->m_segmentSize;
        m_minRtt = tcb->m_lastRtt.Get();
        m_targetRtt = m_targetRttAlpha * m_minRtt.Get();
        m_minRttStamp = Simulator::Now();
        m_persistentRttGrowthCnt = 0;
    }

    if (Simulator::Now() - m_probeRttPropStamp >= m_probeRttDuration && m_probeRtt &&
        tcb->m_bytesInFlight.Get() <= 10 * tcb->m_segmentSize)
    {
        m_probeRtt = false;
        m_usePriorCwnd = true;
    }

    if (!m_probeRtt)
    {
        if (tcb->m_lastAckedSeq >= m_begSndNxt)
        { // as in Vegas, we do QtSDR cwnd adjustment every RTT.
            NS_LOG_LOGIC("A cycle has finished, we adjust cwnd once per RTT.");

            /*
             * as in Vegas, we perform QtSDR calculations only if we got enough RTT samples to
             * insure that at least 1 of those samples wasn't from a delayed ACK.
             */
            if (m_cntRtt <= 1)
            { // We do not have enough RTT samples, so we should behave like Reno
                NS_LOG_LOGIC("We do not have enough RTT samples");
                TcpNewReno::IncreaseWindow(tcb, segmentsAcked);
                m_begSndNxt = tcb->m_nextTxSequence;
                m_cntRtt = 0;
            }
            else
            {
                // Save the current right edge for next cycle
                m_begSndNxt = tcb->m_nextTxSequence;

                const double Rmin = m_minRtt.Get().GetSeconds();
                const double R = tcb->m_lastRtt.Get().GetSeconds();
                const double Rtarget = m_targetRtt.GetSeconds();

                // If the measured RTT is already above the midpoint between the
                // propagation RTT and target RTT, cap ssthresh to avoid an
                // aggressive slow-start overshoot on the next round.
                if (tcb->m_cWnd.Get() < tcb->m_ssThresh.Get() && Rmin > 0.0 && Rtarget > Rmin &&
                    R > 0.5 * (Rmin + Rtarget) && tcb->m_cWnd.Get() > 2 * tcb->m_segmentSize)
                {
                    const uint32_t reducedSsThresh = std::max(
                        2 * tcb->m_segmentSize,
                        static_cast<uint32_t>(0.9 * static_cast<double>(tcb->m_cWnd.Get())));
                    tcb->m_ssThresh = std::min(tcb->m_ssThresh.Get(), reducedSsThresh);
                }

                tcb->m_cWnd = ComputeCwnd(tcb);
                m_cntRtt = 0;
            }

            // Check increasing minRTT
            m_curRtt = tcb->m_lastRtt.Get();
            m_curCwnd = tcb->m_cWnd;
            const bool havePreviousRtt = !m_prevRtt.IsZero();
            const bool persistentAboveTarget = havePreviousRtt && m_curRtt > 1.05 * m_targetRtt &&
                                               m_prevRtt > 1.05 * m_targetRtt &&
                                               m_curRtt >= m_prevRtt;

            if (persistentAboveTarget)
            {
                ++m_persistentRttGrowthCnt;

                tcb->m_cWnd = std::min(tcb->m_cWnd.Get(), m_prevCwnd);
                m_curCwnd = tcb->m_cWnd.Get();

                NS_LOG_DEBUG("Persistent RTT growth above target: cur="
                             << m_curRtt << " prev=" << m_prevRtt << " target=" << m_targetRtt
                             << " count=" << m_persistentRttGrowthCnt);
            }
            else
            {
                m_persistentRttGrowthCnt = 0;
            }

            m_prevRtt = m_curRtt;
            m_prevCwnd = m_curCwnd;
            // end check m_minRtt increase
        }
        else if (tcb->m_cWnd.Get() < tcb->m_ssThresh.Get())
        {
            const double Rmin = m_minRtt.Get().GetSeconds();
            const double Rtarget = m_targetRtt.GetSeconds();
            const Time slowStartRttThreshold = Seconds(0.5 * (Rmin + Rtarget));

            if (!slowStartRttThreshold.IsZero() && tcb->m_lastRtt.Get() < slowStartRttThreshold)
            {
                NS_LOG_INFO("Entering Slow Start phase.");
                TcpNewReno::SlowStart(tcb, segmentsAcked);
            }
        }
    }
}

uint32_t
TcpQtSdr::ComputeCwnd(Ptr<TcpSocketState> tcb)
{
    if (tcb->m_segmentSize == 0 || m_minRtt.Get() == Time::Max() || m_minRtt.Get() <= Time(0) ||
        tcb->m_lastRtt.Get() <= Time(0) || tcb->m_lastRtt.Get() == Time::Max())
    {
        return tcb->m_cWnd.Get();
    }

    const double Rmin = m_minRtt.Get().GetSeconds();
    if (Rmin <= 0.0 || !std::isfinite(Rmin))
    {
        return std::max(tcb->m_cWnd.Get(), 4 * tcb->m_segmentSize);
    }

    const double R_target = m_targetRttAlpha * Rmin;
    m_targetRtt = Seconds(R_target);

    // measured RTT, PacketsInFlight and CwndInSegments
    const double R = tcb->m_lastRtt.Get().GetSeconds();
    const double L = (tcb->m_segmentSize > 0)
                         ? static_cast<double>(tcb->m_bytesInFlight.Get()) / tcb->m_segmentSize
                         : 0.0;
    const double W = static_cast<double>(tcb->GetCwndInSegments()); // cwnd in packets

    if (R <= 0.0 || !std::isfinite(R))
    {
        return std::max(tcb->m_cWnd.Get(), 4 * tcb->m_segmentSize);
    }

    // predict desired PacketsInFlight
    double Lpred = L + W - W * R / R_target;

    if (m_usePriorCwnd)
    {
        Lpred = (tcb->m_segmentSize > 0)
                    ? 0.9 * static_cast<double>(m_priorCwnd) / tcb->m_segmentSize
                    : L;
        m_usePriorCwnd = false;
    }

    Lpred = std::max(Lpred, 4.0);
    if (Lpred == L && R <= R_target)
    {
        Lpred = L + 1;
    }

    // compute desired cwnd - minimum cwnd allowed is 4 packets
    double k_r = (1 - m_minRttEpsilon) + m_minRttEpsilon * Rmin / R;
    double nextWindow = std::ceil(W + k_r * (Lpred - L));
    if (nextWindow == W && R <= R_target)
    {
        nextWindow += 1.0;
    }
    if (nextWindow == W && R > R_target)
    {
        nextWindow -= 1.0;
    }

    m_predInflight = static_cast<uint32_t>(
        std::clamp(Lpred, 0.0, static_cast<double>(std::numeric_limits<uint32_t>::max())));
    return WindowFromSegments(nextWindow, tcb->m_segmentSize);
}

std::string
TcpQtSdr::GetName() const
{
    return "TcpQtSdr";
}

uint32_t
TcpQtSdr::GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
    NS_LOG_FUNCTION(this << tcb << bytesInFlight);
    m_usePriorCwnd = false;
    m_probeRtt = false;

    return std::max(2 * tcb->m_segmentSize, static_cast<uint32_t>(95ULL * bytesInFlight / 100));
}

Ptr<TcpCongestionOps>
TcpQtSdr::Fork()
{
    return CopyObject<TcpQtSdr>(this);
}

} // namespace ns3

namespace ns3
{
uint32_t
TcpQtSdr::WindowFromSegments(double segments, uint32_t segmentSize)
{
    if (segmentSize == 0)
    {
        return 0;
    }
    const double maxSegments = std::numeric_limits<uint32_t>::max() / segmentSize;
    const double minSegments = std::min(2.0, maxSegments);
    if (!std::isfinite(segments))
    {
        segments = minSegments;
    }
    return static_cast<uint32_t>(std::clamp(std::ceil(segments), minSegments, maxSegments)) *
           segmentSize;
}
} // namespace ns3
