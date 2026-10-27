/*
 * Copyright (c) 2024 Dumisa Ngwenya <dumisa@crocs.co.za; ngwenyad@sentech.co.za>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
 *
 * Developed as part of PhD work on TCP Congestion Control
 * Dumisa W. Ngwenya - University of Pretoria - Department of Electrical, Electronic and Computer Engineering
 *
 * Adapted from TcpVegas but inherited from TcpNewReno
 */

#ifndef TCP_QT_TOR_H
#define TCP_QT_TOR_H

#include <cstdint>
#include <string>
#include <vector>

#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-recovery-ops.h"
#include "tcp-socket-base.h"
#include "ns3/sequence-number.h"
#include "ns3/traced-value.h"
#include "ns3/event-id.h"

namespace ns3
{

class TcpSocketState;

/**
 * \brief Represents the finite-horizon QtTOR-V2 prediction state.
 *
 * W is the congestion window in packets, L is in-flight data in packets,
 * and R is the predicted RTT in seconds.
 */
struct QtTorV2State
{
    double W; //!< Congestion window in packets
    double L; //!< In-flight data in packets
    double R; //!< Round-trip time in seconds
};

/**
 * \brief Represents the control input.
 */
struct QtTorV2Control
{
    double u; //!< Control input, interpreted as dW/dt in packets/s
};

/**
 * \ingroup tcp
 * \brief TCP QtTOR congestion-control algorithm.
 *
 * TcpQtTor replaces the artificial RTT-relaxation model used in the
 * original finite-horizon predictor with a queueing-consistent algebraic
 * RTT estimate. At the beginning of each optimisation step, the service-rate
 * estimate is computed from the previous in-flight data and RTT, optionally
 * smoothed, and then held constant throughout the inner optimisation loop.
 */
class TcpQtTor : public TcpNewReno
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId ();

    TcpQtTor ();

    /**
     * \brief Copy constructor.
     * \param sock object to copy.
     */
    TcpQtTor (const TcpQtTor& sock);

    ~TcpQtTor () override;

    std::string GetName () const override;

    void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                    const Time& rtt) override;
    void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;
    uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;

    void SetTargetRttMultiplier (double rttTargetAlpha);
    uint32_t ComputeCwnd (Ptr<TcpSocketState> tcb);

    void UpdateRttProp (Ptr<TcpSocketState> tcb);
    Ptr<TcpCongestionOps> Fork () override;
    TracedValue<uint32_t> m_predictedBytesInFlight {0};
    void MinRttChangeDetection (Ptr<TcpSocketState> tcb);

    Time GetMinRtt ();
    Time GetTargetRtt ();
    double GetTargetRttAlpha ();
    bool GetUsePriorInFlight ();
    void SetUsePriorInFlight (bool usePriorInFlight);
    uint32_t GetPriorInFlight ();

    // Update the fixed service-rate estimate used during the optimisation step.
    void UpdateServiceRateEstimate (double prevInflight, double prevRtt);

    // Queueing-consistent RTT prediction for fixed m_muHat.
    double QueueingRtt (double inflight) const;

    // Runge-Kutta 4th order integrator for one time step.
    QtTorV2State rk4_step (const QtTorV2State& s, double u);

    // Simulate the system for a sequence of control inputs.
    std::vector<QtTorV2State> Integrate (const QtTorV2State& initial,
                                         const std::vector<double>& U);

    // Compute gradient using central difference.
    std::vector<double> ComputeGradient (const QtTorV2State& initial,
                                         const std::vector<double>& U,
                                         double uScale);

    // Compute the dimensionless cost for a control sequence.
    double ComputeCost (const std::vector<QtTorV2State>& trajectory,
                        const std::vector<double>& U,
                        double uScale);

    // Optimize the control input sequence u.
    std::vector<double> OptimizeControl (const QtTorV2State& initial);

    // Compute next best W using optimized u[0].
    double ComputeNextW (double W, const std::vector<double>& U, double dt);

    //--------------------------------------------------
    // Helper functions kept for compatibility/testing
    //--------------------------------------------------

    // dR/dt based on dynamics.
    double dRdt (double R, double W, double u, double dLdt);

    // dW/dt.
    double dWdt (double u);

    // dL/dt model.
    double dLdt_model (double L, double W, double R, double u, double delta_t);

    // Estimate dR/du.
    double dR_du_model (double R, double W, double u, double delta_t);

    // Central difference approximation of dR/du.
    double central_diff_dR_du (double R_now, double W_now, double L_now,
                               double u_now, double delta_t, double epsilon);

    // Sensitivity of terminal R_N to control u[n].
    double propagate_terminal_dRdu (const std::vector<double>& R,
                                    const std::vector<double>& W,
                                    const std::vector<double>& u,
                                    const std::vector<double>& L,
                                    int n, double delta_t);

    // Exposed trace values (preserved for existing tracing integrations).
    TracedValue<double> m_predInflight;
    TracedValue<double> m_predRtt;
    TracedValue<double> m_control;
    TracedValue<double> m_cost;

  private:
    // Per-ACK / per-RTT control orchestration.
    void UpdateProbeRttState (Ptr<TcpSocketState> tcb);
    void ApplyRttWindowControl (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
    uint32_t ApplyWindowGuards (uint32_t proposedWindow, double currentWindow,
                                double currentRtt, double targetRtt);

    TracedValue<Time> m_minRtt {
        Time::Max ()}; //!< Estimated two-way round-trip propagation delay of the path.
    bool m_minRttExpired {false};
    Time m_minRttFilterLen {Seconds (10)};
    Time m_minRttStamp;
    bool m_myminRttExpired {false};
    Time m_targetRtt;
    double m_targetRttAlpha {1.2};
    Time m_probeRttDuration {Seconds (0.2)};
    Time m_probeRttPropStamp;
    uint32_t m_priorInFlight {1024};
    uint32_t m_cntRtt {0};
    uint32_t m_rttOvershootCnt {0};
    uint32_t m_last_loss_cWnd {0};
    uint32_t m_TxBytesTotal {0};
    uint32_t m_AckBytesTotal {0};
    double m_loss_ratio {0};
    Time m_cwndChangeStamp {0};
    bool m_probeRtt {false};
    bool m_usePriorInFlight {false};
    SequenceNumber32 m_begSndNxt;

    // Optimal-control state history.
    double m_Rtt {0.0};
    double m_pktsInFlight {0.0};
    double m_cwndInPkts {0.0};
    double m_time {0.0};
    double m_prevRtt {0.0};
    double m_prevPktsInFlight {0.0};
    double m_prevCwndInPkts {0.0};
    double m_prevTime {0.0};
    bool m_minRttChanged {false};

    // Service-rate estimator used by the finite-horizon queueing model.
    double m_muHat {0.0};      //!< Smoothed service-rate estimate in packets/s.
    double m_muRho {0.1};      //!< Smoothing gain for service-rate estimate.
    double m_muMin {1e-6};     //!< Lower bound for service-rate estimate.

    // Adaptive control normalisation. The scale is recomputed at each
    // optimisation interval from the nominal control and the observable W/R.
    double m_minUScale {25.0};       //!< Minimum u scale in packets/s.
    double m_uScaleFraction {0.05};  //!< Fraction of cwnd per RTT.

    const uint32_t m_maxIter {10};
    const uint32_t m_horizon {5};
    const double m_learning_rate {0.05};
    std::vector<QtTorV2State> m_state;
    double m_u {90.0};
    double m_wR {1.0};
    double m_wu {1};
    double m_wDeltaU {1};
    double m_qL {0.0};
    double m_qRdot {0.0};
    double m_prevControl {0.0};

    // Guard state must be per TCP connection, not shared across instances.
    uint32_t m_consecutiveDelayReductions {0};
};

} // namespace ns3

#endif /* TCP_QT_TOR_H */
