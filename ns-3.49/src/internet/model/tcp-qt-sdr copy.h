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
 * Adapted from TcpVegas but inherited from TcpNewReno
 *
 */

#ifndef TCP_QTSDR_H
#define TCP_QTSDR_H

#include "ns3/nstime.h"
#include "ns3/sequence-number.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/traced-value.h"

#include <cstdint>
#include <string>

namespace ns3
{

class TcpSocketState;

/** @ingroup tcp
 * @brief RTT-targeted congestion control using predicted in-flight packets.
 */
class TcpQtSdr : public TcpNewReno
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    TcpQtSdr();

    /**
     * \brief Copy constructor.
     * \param sock object to copy.
     */
    TcpQtSdr(const TcpQtSdr& sock);

    ~TcpQtSdr() override;

    std::string GetName() const override;

    void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) override;
    void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;
    uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;
    void SetTargetRttMultiplier(double rttTarget_alpha);
    void SetMinRttEpsilon(double minRttEpsilon);
    virtual uint32_t ComputeCwnd(Ptr<TcpSocketState> tcb);
    void UpdateRttProp(Ptr<TcpSocketState> tcb);
    virtual Ptr<TcpCongestionOps> Fork() override;

    Time GetMinRtt() const;
    Time GetTargetRtt() const;
    double GetTargetRttAlpha() const;
    double GetMinRttEpsilon() const;
    bool GetUsePriorInFlight() const;
    void SetUsePriorInFlight(bool usePriorInFlight);
    uint32_t GetPriorInFlight() const;

    TracedValue<uint32_t> m_predInflight{0};

  private:
    /** @brief Incorporate a valid ACK RTT sample. @param rtt Measured RTT. */
    void UpdateRttSample(Time rtt);
    /**
     * @brief Convert a packet window to bounded bytes without overflow.
     * @param segments Proposed window in packets.
     * @param segmentSize TCP segment size in bytes.
     * @return A representable byte window of at least two segments when possible.
     */
    static uint32_t WindowFromSegments(double segments, uint32_t segmentSize);

    TracedValue<Time> m_minRtt{
        Time::Max()}; //!< Estimated two-way round-trip propagation delay of the path, estimated
                      //!< from the windowed minimum recent round-trip delay sample.
    bool m_minRttExpired{false};         //!< A boolean recording whether the BBR.RTprop has expired
                                         //!< filter window, default 10 secs.
    Time m_minRttFilterLen{Seconds(10)}; //!< A constant specifying the length of the RTProp min
    Time m_minRttStamp{Seconds(0)};
    Time m_targetRtt{Seconds(0)};
    double m_targetRttAlpha{1.2};
    double m_minRttEpsilon{1.0};
    Time m_probeRttDuration{Seconds(0.2)};
    Time m_probeRttPropStamp{Seconds(0)};
    uint32_t m_priorCwnd{5000};
    uint32_t m_cntRtt{0}; //!< Number of RTT measurements during last RTT
    bool m_probeRtt{false};
    bool m_usePriorCwnd{false};
    SequenceNumber32 m_begSndNxt{0}; //!< Right edge during last RTT
    Time m_curRtt{Seconds(0)};
    Time m_prevRtt{Seconds(0)};
    uint32_t m_curCwnd{5000};
    uint32_t m_prevCwnd{5000};
    uint32_t m_persistentRttGrowthCnt{0};
    uint32_t m_persistentRttGrowthCntThreshold{5};
};

} // end namespace ns3

#endif /* TCP_QTSDR_H */
