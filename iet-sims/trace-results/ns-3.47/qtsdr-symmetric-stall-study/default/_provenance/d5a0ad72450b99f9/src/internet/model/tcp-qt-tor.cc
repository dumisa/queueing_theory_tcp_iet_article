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
 * Dumisa W. Ngwenya - University of Pretoria - Department of Electrical, Electronic and Computer Engineering
 *
 * Adapted from TCP Vegas
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "tcp-qt-tor.h"
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/tcp-socket-state.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE ("TcpQtTor");
NS_OBJECT_ENSURE_REGISTERED (TcpQtTor);

TypeId
TcpQtTor::GetTypeId ()
{
    static TypeId tid = TypeId ("ns3::TcpQtTor")
                            .SetParent<TcpNewReno> ()
                            .AddConstructor<TcpQtTor> ()
                            .SetGroupName ("Internet")
                            .AddAttribute ("targetRttAlpha",
                                           "multiplication factor for Rtarget in proportion to R_min",
                                           DoubleValue (1.2),
                                           MakeDoubleAccessor (&TcpQtTor::SetTargetRttMultiplier),
                                           MakeDoubleChecker<double> (0.0))
                            .AddAttribute ("RttWindowLength",
                                           "Length of RTT windowed filter",
                                           TimeValue (Seconds (10)),
                                           MakeTimeAccessor (&TcpQtTor::m_minRttFilterLen),
                                           MakeTimeChecker ())
                            .AddAttribute ("MuSmoothingGain",
                                           "Exponential smoothing gain for the fixed service-rate estimate muHat",
                                           DoubleValue (0.1),
                                           MakeDoubleAccessor (&TcpQtTor::m_muRho),
                                           MakeDoubleChecker<double> (0.0, 1.0))
                            .AddAttribute ("MinServiceRate",
                                           "Lower bound for the service-rate estimate in packets per second",
                                           DoubleValue (1e-6),
                                           MakeDoubleAccessor (&TcpQtTor::m_muMin),
                                           MakeDoubleChecker<double> (0.0))
                            .AddAttribute ("MinControlScale",
                                           "Minimum normalisation scale for the control input in packets per second",
                                           DoubleValue (25.0),
                                           MakeDoubleAccessor (&TcpQtTor::m_minUScale),
                                           MakeDoubleChecker<double> (1e-9))
                            .AddAttribute ("ControlScaleFraction",
                                           "Fraction of the current congestion window per RTT used to form the control scale",
                                           DoubleValue (0.05),
                                           MakeDoubleAccessor (&TcpQtTor::m_uScaleFraction),
                                           MakeDoubleChecker<double> (0.0))
                            .AddTraceSource ("MinRtt",
                                             "Estimated two-way round-trip propagation delay of the path, estimated "
                                             "from the windowed minimum recent round-trip delay sample",
                                             MakeTraceSourceAccessor (&TcpQtTor::m_minRtt),
                                             "ns3::TracedValueCallback::Time")
                            .AddTraceSource ("predRtt",
                                             "Predicted RTT from trajectory optimization.",
                                             MakeTraceSourceAccessor (&TcpQtTor::m_predRtt),
                                             "ns3::TracedValueCallback::Double")
                            .AddTraceSource ("predInflight",
                                             "Predicted inflight packets from trajectory optimization.",
                                             MakeTraceSourceAccessor (&TcpQtTor::m_predInflight),
                                             "ns3::TracedValueCallback::Double")
                            .AddTraceSource ("controlInput",
                                             "Control input selected by trajectory optimization.",
                                             MakeTraceSourceAccessor (&TcpQtTor::m_control),
                                             "ns3::TracedValueCallback::Double")
                            .AddTraceSource ("cost",
                                             "Trajectory optimization cost.",
                                             MakeTraceSourceAccessor (&TcpQtTor::m_cost),
                                             "ns3::TracedValueCallback::Double");
    return tid;
}

TcpQtTor::TcpQtTor ()
    : TcpNewReno ()
{
    NS_LOG_FUNCTION (this);
}

TcpQtTor::TcpQtTor (const TcpQtTor& sock)
    : TcpNewReno (sock),

      // Trace variables: declared before private members
      m_predInflight (sock.m_predInflight),
      m_predRtt (sock.m_predRtt),
      m_control (sock.m_control),
      m_cost (sock.m_cost),

      // RTT estimation and ProbeRTT
      m_minRtt (sock.m_minRtt),
      m_minRttExpired (sock.m_minRttExpired),
      m_minRttFilterLen (sock.m_minRttFilterLen),
      m_minRttStamp (sock.m_minRttStamp),
      m_myminRttExpired (sock.m_myminRttExpired),
      m_targetRtt (sock.m_targetRtt),
      m_targetRttAlpha (sock.m_targetRttAlpha),
      m_probeRttDuration (sock.m_probeRttDuration),
      m_probeRttPropStamp (sock.m_probeRttPropStamp),

      // Congestion-control state
      m_priorInFlight (sock.m_priorInFlight),
      m_cntRtt (sock.m_cntRtt),
      m_rttOvershootCnt (sock.m_rttOvershootCnt),
      m_last_loss_cWnd (sock.m_last_loss_cWnd),
      m_TxBytesTotal (sock.m_TxBytesTotal),
      m_AckBytesTotal (sock.m_AckBytesTotal),
      m_loss_ratio (sock.m_loss_ratio),
      m_cwndChangeStamp (sock.m_cwndChangeStamp),
      m_probeRtt (sock.m_probeRtt),
      m_usePriorInFlight (sock.m_usePriorInFlight),
      m_begSndNxt (sock.m_begSndNxt),

      // Optimal-control state history
      m_Rtt (sock.m_Rtt),
      m_pktsInFlight (sock.m_pktsInFlight),
      m_cwndInPkts (sock.m_cwndInPkts),
      m_time (sock.m_time),
      m_prevRtt (sock.m_prevRtt),
      m_prevPktsInFlight (sock.m_prevPktsInFlight),
      m_prevCwndInPkts (sock.m_prevCwndInPkts),
      m_prevTime (sock.m_prevTime),
      m_minRttChanged (sock.m_minRttChanged),

      // Service-rate estimator
      m_muHat (sock.m_muHat),
      m_muRho (sock.m_muRho),
      m_muMin (sock.m_muMin),

      // Adaptive control normalisation
      m_minUScale (sock.m_minUScale),
      m_uScaleFraction (sock.m_uScaleFraction),

      // Finite-horizon optimisation
      m_state (sock.m_state),
      m_u (sock.m_u),
      m_wR (sock.m_wR),
      m_wu (sock.m_wu),
      m_wDeltaU (sock.m_wDeltaU),
      m_qL (sock.m_qL),
      m_qRdot (sock.m_qRdot),
      m_prevControl (sock.m_prevControl),
      m_consecutiveDelayReductions (sock.m_consecutiveDelayReductions)
{
    NS_LOG_FUNCTION (this);
}

TcpQtTor::~TcpQtTor ()
{
    NS_LOG_FUNCTION (this);
}

Time
TcpQtTor::GetMinRtt ()
{
    return m_minRtt;
}

Time
TcpQtTor::GetTargetRtt ()
{
    return m_targetRtt;
}

double
TcpQtTor::GetTargetRttAlpha ()
{
    return m_targetRttAlpha;
}

bool
TcpQtTor::GetUsePriorInFlight ()
{
    return m_usePriorInFlight;
}

uint32_t
TcpQtTor::GetPriorInFlight ()
{
    return m_priorInFlight;
}

void
TcpQtTor::SetUsePriorInFlight (bool usePriorInFlight)
{
    m_usePriorInFlight = usePriorInFlight;
}

void
TcpQtTor::SetTargetRttMultiplier (double targetRttAlpha)
{
    m_targetRttAlpha = targetRttAlpha;
}

void
TcpQtTor::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt)
{
    NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);

    if (rtt.IsZero ())
    {
        return;
    }

    m_minRtt = std::min (m_minRtt.Get (), rtt);
    m_targetRtt = m_targetRttAlpha * m_minRtt;

    NS_LOG_DEBUG ("Updated m_minRtt = " << m_minRtt);

    // Update RTT counter.
    m_cntRtt++;
    NS_LOG_DEBUG ("Updated m_cntRtt = " << m_cntRtt);
}

void
TcpQtTor::UpdateRttProp (Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION (this << tcb);

    if (tcb->m_lastRtt >= Seconds (0) &&
        (tcb->m_lastRtt <= m_minRtt || m_minRttExpired || m_minRtt == Seconds (0) ||
         m_minRtt == Time::Max ()))
    {
        m_minRtt = tcb->m_lastRtt;
        m_targetRtt = m_targetRttAlpha * m_minRtt;
        m_minRttStamp = Simulator::Now ();
    }
}

void
TcpQtTor::MinRttChangeDetection (Ptr<TcpSocketState> tcb)
{
    NS_LOG_FUNCTION (this << tcb);

    // Retain the observable history without calculating unused ratios.
    // In particular, the old expression could divide by zero.
    if (tcb->m_segmentSize == 0)
    {
        return;
    }

    m_prevCwndInPkts = static_cast<double> (tcb->m_cWnd) / tcb->m_segmentSize;
    m_prevRtt = tcb->m_lastRtt.Get ().GetSeconds ();
    m_prevPktsInFlight = static_cast<double> (tcb->m_bytesInFlight) / tcb->m_segmentSize;
    m_prevTime = Simulator::Now ().GetSeconds ();
}

void
TcpQtTor::UpdateProbeRttState (Ptr<TcpSocketState> tcb)
{
    m_minRttExpired = Simulator::Now () > (m_minRttStamp + m_minRttFilterLen);
    m_myminRttExpired = m_minRttExpired;

    // Preserve legacy behaviour: ProbeRTT entry is currently disabled.
    // This should be revisited separately rather than silently changing simulations.
    m_minRttExpired = false;

    if (m_minRttExpired && !m_probeRtt)
    {
        NS_LOG_INFO ("Starting Probe RTT phase.");
        m_probeRtt = true;
        m_probeRttPropStamp = Simulator::Now ();
        m_priorInFlight = tcb->m_bytesInFlight;
        tcb->m_cWnd = 4 * tcb->m_segmentSize;
        m_minRtt = tcb->m_lastRtt;
        m_minRttStamp = Simulator::Now ();
    }

    UpdateRttProp (tcb);

    if (Simulator::Now () - m_probeRttPropStamp >= m_probeRttDuration && m_probeRtt &&
        tcb->m_bytesInFlight <= 4 * tcb->m_segmentSize)
    {
        m_probeRtt = false;
        m_usePriorInFlight = true;
    }
}

void
TcpQtTor::ApplyRttWindowControl (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    if (m_probeRtt)
    {
        return;
    }

    if (tcb->m_lastAckedSeq >= m_begSndNxt)
    {
        NS_LOG_INFO ("A full RTT cycle completed, adjusting congestion window.");
        if (m_cntRtt <= 2)
        {
            NS_LOG_INFO ("Insufficient RTT samples, behaving like Reno.");
            return;
        }

        m_begSndNxt = tcb->m_nextTxSequence;
        const double minRtt = m_minRtt.Get ().GetSeconds ();
        const double currentRtt = tcb->m_lastRtt.Get ().GetSeconds ();

        if (currentRtt > 1.01 * minRtt)
        {
            tcb->m_ssThresh = std::max (std::min (tcb->m_ssThresh.Get (),
                                                  9 * tcb->m_cWnd.Get () / 10),
                                        2 * tcb->m_segmentSize);
        }

        tcb->m_cWnd = ComputeCwnd (tcb);
        m_cntRtt = 0;
    }
    else if (tcb->m_cWnd < tcb->m_ssThresh &&
             tcb->m_lastRtt < (m_targetRtt + m_minRtt) / 2)
    {
        NS_LOG_INFO ("Entering Slow Start phase.");
        TcpNewReno::SlowStart (tcb, segmentsAcked);
        m_begSndNxt = tcb->m_nextTxSequence;
    }
}

void
TcpQtTor::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION (this << tcb << segmentsAcked);
    UpdateProbeRttState (tcb);
    ApplyRttWindowControl (tcb, segmentsAcked);
}

uint32_t
TcpQtTor::ApplyWindowGuards (uint32_t proposedWindow, double currentWindow,
                             double currentRtt, double targetRtt)
{
    if (proposedWindow == static_cast<uint32_t> (currentWindow))
    {
        return proposedWindow + 1;
    }

    if (currentRtt >= targetRtt &&
        (proposedWindow >= currentWindow || m_consecutiveDelayReductions > 2))
    {
        m_consecutiveDelayReductions = 0;
        m_minRttStamp = Simulator::Now ();
        return static_cast<uint32_t> (std::floor (0.98 * currentWindow));
    }

    if (currentRtt >= targetRtt)
    {
        ++m_consecutiveDelayReductions;
    }
    return proposedWindow;
}

uint32_t
TcpQtTor::ComputeCwnd (Ptr<TcpSocketState> tcb)
{
    double R_min = m_minRtt.Get ().GetSeconds ();
    double R_target = m_targetRttAlpha * R_min;

    double R_now = tcb->m_lastRtt.Get ().GetSeconds ();
    double L_now = static_cast<double> (tcb->m_bytesInFlight) / tcb->m_segmentSize;
    double W_now = static_cast<double> (tcb->GetCwndInSegments ());

    if (m_usePriorInFlight)
    {
        m_usePriorInFlight = false;
        L_now = std::max (0.90 * static_cast<double> (m_priorInFlight) / tcb->m_segmentSize, 4.0);
        W_now = L_now;
    }

    // Estimate muHat once at the beginning of the optimisation step, using the
    // previous in-flight data and RTT. The estimate is then held fixed during
    // all inner gradient-descent iterations and horizon integrations.
    double prevInflight = (m_prevPktsInFlight > 0.0) ? m_prevPktsInFlight : L_now;
    double prevRtt = (m_prevRtt > 0.0) ? m_prevRtt : R_now;
    UpdateServiceRateEstimate (prevInflight, prevRtt);

    QtTorV2State initial = {W_now, L_now, std::max (R_min, R_now)};
    std::vector<double> U = OptimizeControl (initial);

    double u_applied = U[0];
    QtTorV2State S_next = rk4_step (initial, u_applied);
    m_prevControl = u_applied;

    const uint32_t proposedWindow =
        static_cast<uint32_t> (std::ceil (std::max (2.0, S_next.W)));
    const uint32_t guardedWindow = ApplyWindowGuards (proposedWindow, W_now, R_now, R_target);

    m_predRtt = S_next.R;
    m_predInflight = S_next.L;
    m_control = u_applied;

    // Store current measurements for the next optimisation interval's causal
    // service-rate estimate.
    m_prevRtt = R_now;
    m_prevPktsInFlight = L_now;
    m_prevCwndInPkts = W_now;
    m_prevTime = Simulator::Now ().GetSeconds ();

    return std::max (guardedWindow * tcb->m_segmentSize, 2 * tcb->m_segmentSize);
}

//--------------------------------------------------
// Queueing-model helpers
//--------------------------------------------------

void
TcpQtTor::UpdateServiceRateEstimate (double prevInflight, double prevRtt)
{
    if (!std::isfinite (prevInflight) || !std::isfinite (prevRtt) || prevInflight <= 0.0 ||
        prevRtt <= 0.0)
    {
        return;
    }

    double muSample = prevInflight / prevRtt; // packets/s
    if (!std::isfinite (muSample) || muSample <= 0.0)
    {
        return;
    }

    muSample = std::max (muSample, m_muMin);
    double rho = std::clamp (m_muRho, 0.0, 1.0);

    if (!std::isfinite (m_muHat) || m_muHat <= m_muMin)
    {
        m_muHat = muSample;
    }
    else
    {
        m_muHat = (1.0 - rho) * m_muHat + rho * muSample;
    }

    m_muHat = std::max (m_muHat, m_muMin);
}

double
TcpQtTor::QueueingRtt (double inflight) const
{
    double R_min = m_minRtt.Get ().GetSeconds ();
    double safeRmin = std::max (R_min, 1e-9);
    double safeMu = std::max (m_muHat, m_muMin);

    if (!std::isfinite (inflight))
    {
        return safeRmin;
    }

    double baseInflight = safeMu * safeRmin;
    double queueInflight = std::max (0.0, inflight - baseInflight);
    double rtt = safeRmin + queueInflight / safeMu;

    if (!std::isfinite (rtt))
    {
        return safeRmin;
    }

    return std::max (safeRmin, rtt);
}

//--------------------------------------------------
// Helper functions
//--------------------------------------------------

// dR/dt based on dynamics. Retained for compatibility; the V2 horizon model
// obtains RTT algebraically from L and fixed muHat instead of propagating dR/dt.
double
TcpQtTor::dRdt (double R, double W, double u, double dLdt)
{
    double R_min = m_minRtt.Get ().GetSeconds ();
    double scaled_W = std::max (W * (1.0 - R_min / R), 1.0);
    return (R / scaled_W) * (u - dLdt);
}

// dW/dt.
double
TcpQtTor::dWdt (double u)
{
    return u;
}

// dL/dt model using the fixed service-rate estimate.
double
TcpQtTor::dLdt_model (double L, double W, double R, double u, double delta_t)
{
    (void) L;
    (void) u;
    (void) delta_t;
    double safeR = std::max (R, m_minRtt.Get ().GetSeconds ());
    safeR = std::max (safeR, 1e-9);
    double safeMu = std::max (m_muHat, m_muMin);
    return W / safeR - safeMu;
}

// Estimate dR/du. Kept for compatibility; not used by the V2 optimiser.
double
TcpQtTor::dR_du_model (double R, double W, double u, double delta_t)
{
    double R_min = m_minRtt.Get ().GetSeconds ();
    double R_target = m_targetRttAlpha * R_min;
    double den = u * R - W * (R - R_target) / delta_t;

    if (!std::isfinite (den) || std::abs (den) < 1e-12)
    {
        return 0.0;
    }

    return R * R / den;
}

// Runge-Kutta 4th order integrator for one time step. The state propagated by
// RK4 is (W,L); R is evaluated algebraically from L using the fixed muHat.
QtTorV2State
TcpQtTor::rk4_step (const QtTorV2State& s, double u)
{
    double R_min = m_minRtt.Get ().GetSeconds ();
    double R_target = m_targetRttAlpha * R_min;
    double dt = std::max (R_target, 1e-9);
    double safeMu = std::max (m_muHat, m_muMin);

    auto derivative = [&] (const QtTorV2State& state) -> QtTorV2State {
        double R = QueueingRtt (state.L);
        double safeR = std::max (R, 1e-9);
        double safeW = std::max (state.W, 2.0);
        double lambda = safeW / safeR;
        double dLdt = lambda - safeMu;

        return {u, dLdt, 0.0};
    };

    QtTorV2State k1 = derivative (s);
    QtTorV2State s1 = {s.W + 0.5 * dt * k1.W,
                       s.L + 0.5 * dt * k1.L,
                       QueueingRtt (s.L + 0.5 * dt * k1.L)};

    QtTorV2State k2 = derivative (s1);
    QtTorV2State s2 = {s.W + 0.5 * dt * k2.W,
                       s.L + 0.5 * dt * k2.L,
                       QueueingRtt (s.L + 0.5 * dt * k2.L)};

    QtTorV2State k3 = derivative (s2);
    QtTorV2State s3 = {s.W + dt * k3.W,
                       s.L + dt * k3.L,
                       QueueingRtt (s.L + dt * k3.L)};

    QtTorV2State k4 = derivative (s3);

    QtTorV2State next;
    next.W = std::max (2.0, s.W + dt / 6.0 * (k1.W + 2.0 * k2.W + 2.0 * k3.W + k4.W));
    next.L = std::max (2.0, s.L + dt / 6.0 * (k1.L + 2.0 * k2.L + 2.0 * k3.L + k4.L));
    next.R = QueueingRtt (next.L);

    return next;
}

// Simulate the system for a sequence of control inputs.
std::vector<QtTorV2State>
TcpQtTor::Integrate (const QtTorV2State& initial, const std::vector<double>& U)
{
    std::vector<QtTorV2State> trajectory;
    trajectory.reserve (U.size () + 1);

    QtTorV2State s = initial;
    s.R = QueueingRtt (s.L);
    trajectory.push_back (s);

    for (const double& u : U)
    {
        s = rk4_step (s, u);
        trajectory.push_back (s);
    }

    return trajectory;
}

// Compute the cost for a control sequence.
double
TcpQtTor::ComputeCost (const std::vector<QtTorV2State>& trajectory,
                         const std::vector<double>& U,
                         double uScale)
{
    const double R_min = m_minRtt.Get ().GetSeconds ();
    const double R_target = m_targetRttAlpha * R_min;
    const double safeRTarget = std::max (R_target, 1e-9);
    const double safeUScale = std::max (uScale, 1e-9);

    double cost = 0.0;

    for (size_t i = 0; i < m_horizon; ++i)
    {
        const QtTorV2State& s_next = trajectory[i + 1];
        const double u = U[i];
        const double uPrev = (i == 0) ? m_prevControl : U[i - 1];

        // Use dimensionless errors so that the cost weights express relative
        // design priorities rather than compensating for incompatible units.
        const double R_err = (s_next.R - R_target) / safeRTarget;
        const double uNorm = u / safeUScale;
        const double dUNorm = (u - uPrev) / safeUScale;

        if (!std::isfinite (R_err) || !std::isfinite (uNorm) ||
            !std::isfinite (dUNorm) || !std::isfinite (s_next.L) ||
            !std::isfinite (s_next.W))
        {
            return 1e12; // Large penalty for invalid paths.
        }


        cost += m_wR * std::pow (R_err, 2) +       // Normalised RTT error.
                m_wu * std::pow (uNorm, 2) +       // Normalised control effort.
                m_wDeltaU * std::pow (dUNorm, 2);  // Normalised control variation.
    }

    return cost;
}

// Compute gradient using central difference.
std::vector<double>
TcpQtTor::ComputeGradient (const QtTorV2State& initial,
                             const std::vector<double>& U,
                             double uScale)
{
    const double safeUScale = std::max (uScale, 1e-9);
    const double epsilonNorm = 0.05;
    const double epsilon = epsilonNorm * safeUScale;
    std::vector<double> grad (U.size ());

    for (size_t i = 0; i < U.size (); ++i)
    {
        std::vector<double> u_plus = U;
        std::vector<double> u_minus = U;

        u_plus[i] += epsilon;
        u_minus[i] -= epsilon;

        auto traj_plus = Integrate (initial, u_plus);
        auto traj_minus = Integrate (initial, u_minus);

        const double J_plus = ComputeCost (traj_plus, u_plus, safeUScale);
        const double J_minus = ComputeCost (traj_minus, u_minus, safeUScale);

        // Divide by the dimensionless perturbation. The returned gradient is
        // therefore with respect to v = u/uScale, which improves conditioning
        // when the path BDP and operating congestion window are unknown.
        const double g = (J_plus - J_minus) / (2.0 * epsilonNorm);
        grad[i] = std::clamp (g, -1e3, 1e3);
    }

    return grad;
}

// Optimize the control input sequence u.
std::vector<double>
TcpQtTor::OptimizeControl (const QtTorV2State& initial)
{
    double R_min = m_minRtt.Get ().GetSeconds ();
    double R_target = m_targetRttAlpha * R_min;
    double dt = std::max (R_min, 1e-9);

    std::vector<double> U (m_horizon, m_u);

    double safeR = std::max (initial.R, R_min);
    double L_predict = initial.L + initial.W - (initial.R / R_target) * initial.W;

    L_predict = std::max (L_predict, 4.0);
    if (L_predict == initial.L)
    {
        L_predict = initial.L + 1.0;
    }

    double W_predict = initial.W + (R_min / safeR) * (L_predict - initial.L);
    double u_init = (W_predict - initial.W) / dt;

    if (!std::isfinite (u_init))
    {
        u_init = m_u;
    }

    const double rateBasedScale =
        m_uScaleFraction * initial.W / std::max (initial.R, 1.0e-6);

    const double uScale = std::max ({m_minUScale,
                                     std::abs (u_init),
                                     rateBasedScale});

    NS_LOG_DEBUG ("uInit=" << u_init << ", rateBasedScale=" << rateBasedScale
                            << ", uScale=" << uScale);

    std::fill (U.begin (), U.end (), u_init);

    for (int iter = 0; iter < static_cast<int> (m_maxIter); ++iter)
    {
        std::vector<double> grad = ComputeGradient (initial, U, uScale);

        double grad_norm = 0.0;
        for (double g : grad)
        {
            grad_norm += g * g;
        }
        grad_norm = std::sqrt (grad_norm);

        if (grad_norm < 1e-3)
        {
            NS_LOG_INFO ("Gradient descent converged at iteration " << iter);
            break;
        }

        for (size_t i = 0; i < U.size (); ++i)
        {
            // The gradient is with respect to v = u/uScale. Convert the
            // dimensionless update back to packets/s before applying it.
            U[i] -= m_learning_rate * grad[i] * uScale;
            U[i] = std::clamp (U[i], -10.0 * uScale, 10.0 * uScale);
        }
    }

    auto trajectory = Integrate (initial, U);
    m_cost = ComputeCost (trajectory, U, uScale);

    return U;
}

// Compute next best W using optimized u[0].
double
TcpQtTor::ComputeNextW (double W, const std::vector<double>& U, double dt)
{
    return std::max (2.0, W + U[0] * dt);
}

std::string
TcpQtTor::GetName () const
{
    return "TcpQtTor";
}

uint32_t
TcpQtTor::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
    NS_LOG_FUNCTION (this << tcb << bytesInFlight);

    return std::max (2 * tcb->m_segmentSize, 95 * bytesInFlight / 100);
}

Ptr<TcpCongestionOps>
TcpQtTor::Fork ()
{
    return CopyObject<TcpQtTor> (this);
}

} // namespace ns3
