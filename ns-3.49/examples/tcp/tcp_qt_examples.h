#ifndef TCP_QT_EXAMPLES_H
#define TCP_QT_EXAMPLES_H

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/my-dumbbell.h"
#include "ns3/my_maths_tools.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/tcp-header.h"
#include "ns3/traffic-control-module.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace ns3
{
namespace tcp_qt_examples
{

struct SimulationConfig
{
    std::string transport = "TcpNewReno";
    std::string recovery = "TcpClassicRecovery";
    std::string queueDiscType = "PfifoFastQueueDisc";

    std::string bottleneckRate = "100Mbps";
    std::string bottleneckDelay = "20ms";
    std::string accessRate = "10Gbps";
    std::string accessDelay = "0.01ms";

    std::string simName = "default";
    double errorRate = 0.0;
    double simDuration = 200.0;
    double dataLoggingInterval = 0.01;

    uint32_t segmentSize = 1448;
    uint32_t queueSizeBytes = 2000000;
    uint32_t numFlows = 1;
    uint32_t initialCwnd = 10;
    uint32_t delayedAckCount = 2;

    double targetRttAlpha = 1.2;
    double minRttEpsilon = 1.0;

    bool enableTraces = true;
    bool enableLoggers = true;
    bool enableProgress = true;
};

struct OutputPaths
{
    std::string directory;
    std::string prefix;
};

class TraceState
{
  public:
    explicit TraceState(const SimulationConfig& config)
        : m_config(config),
          m_microInterval(std::max(config.dataLoggingInterval, 1.0e-6)),
          m_macroInterval(std::max(config.dataLoggingInterval, 1.0e-6))
    {
    }

    void SetOutputPaths(const OutputPaths& paths)
    {
        m_paths = paths;
    }

    void EnablePerFlowTraces(dumbbell& topology)
    {
        if (!m_config.enableTraces)
        {
            //return;
        }

        AsciiTraceHelper ascii;
        const Time initialRtt = 2 * Time(m_config.accessDelay) + 2 * Time(m_config.bottleneckDelay);

        for (uint32_t flow = 0; flow < m_config.numFlows; ++flow)
        {
            const uint32_t nodeId = topology.GetLeftLeaf(flow)->GetId();
            const std::string flowSuffix = "-flow" + std::to_string(flow);

            m_firstCwnd[nodeId] = true;
            m_firstSsthresh[nodeId] = true;
            m_firstRtt[nodeId] = true;
            m_firstRto[nodeId] = true;

            m_cwndStream[nodeId] = ascii.CreateFileStream(m_paths.directory + m_paths.prefix + flowSuffix + "-cwnd.data");
            m_ssthreshStream[nodeId] = ascii.CreateFileStream(m_paths.directory + m_paths.prefix + flowSuffix + "-ssthresh.data");
            m_rttStream[nodeId] = ascii.CreateFileStream(m_paths.directory + m_paths.prefix + flowSuffix + "-rtt.data");
            m_rtoStream[nodeId] = ascii.CreateFileStream(m_paths.directory + m_paths.prefix + flowSuffix + "-rto.data");
            m_inFlightStream[nodeId] = ascii.CreateFileStream(m_paths.directory + m_paths.prefix + flowSuffix + "-inflight.data");

            m_txBytes[nodeId] = 0;
            m_retransBytes[nodeId] = 0;
            m_lostBytes[nodeId] = 0;
            m_cwndValue[nodeId] = 0;
            m_ssthreshValue[nodeId] = 0;
            m_inFlightValue[nodeId] = 0;
            m_rttValue[nodeId] = initialRtt.GetSeconds();
            m_averageThroughput[nodeId] = 0.0;

            Simulator::Schedule(Seconds(0.01), &TraceState::ConnectSocketTraces, this, nodeId);
        }
    }

    void EnableAggregateLoggers()
    {
        if (!m_config.enableLoggers)
        {
            return;
        }

        AsciiTraceHelper ascii;
        m_microMetricsStream = ascii.CreateFileStream(m_paths.directory + m_paths.prefix + "-aggMicroMetrics.data");
        m_macroMetricsStream = ascii.CreateFileStream(m_paths.directory + m_paths.prefix + "-aggMacroMetrics.data");
        m_bottleneckRateStream = ascii.CreateFileStream(m_paths.directory + m_paths.prefix + "-changeBtlBW.data");
        m_propagationDelayStream = ascii.CreateFileStream(m_paths.directory + m_paths.prefix + "-changePropDelay.data");

        Simulator::Schedule(Seconds(0.001), &TraceState::LogMicroMetrics, this);
        Simulator::Schedule(Seconds(0.001), &TraceState::LogMacroMetrics, this);
    }

    void EnableProgressPrinter()
    {
        if (m_config.enableProgress)
        {
            Simulator::Schedule(Seconds(1.0), &TraceState::PrintProgress, this);
        }
    }

    void ChangeBottleneckRate(dumbbell topology, uint64_t bitRate)
    {
        PointToPointNetDevice* device = static_cast<PointToPointNetDevice*>(PeekPointer(topology.GetRouterDevices(0)));
        DataRateValue oldRate;
        device->GetAttribute("DataRate", oldRate);
        device->SetDataRate(DataRate(bitRate));

        if (m_bottleneckRateStream)
        {
            if (m_firstBottleneckRateEntry)
            {
                *m_bottleneckRateStream->GetStream() << 0.0 << " " << oldRate.Get().GetBitRate() * 1e-6 << std::endl;
                m_firstBottleneckRateEntry = false;
            }
            *m_bottleneckRateStream->GetStream() << Simulator::Now().GetSeconds() << " "
                                                  << oldRate.Get().GetBitRate() * 1e-6 << std::endl;
            *m_bottleneckRateStream->GetStream() << Simulator::Now().GetSeconds() << " "
                                                  << bitRate * 1e-6 << std::endl;
        }
    }

    void ChangePropagationDelay(dumbbell topology, Time delay)
    {
        PointToPointNetDevice* device = static_cast<PointToPointNetDevice*>(PeekPointer(topology.GetRouterDevices(0)));
        Ptr<PointToPointChannel> channel = DynamicCast<PointToPointChannel>(device->GetChannel());
        TimeValue oldDelay;
        channel->GetAttribute("Delay", oldDelay);
        channel->SetAttribute("Delay", TimeValue(delay));

        if (m_propagationDelayStream)
        {
            if (m_firstPropagationDelayEntry)
            {
                *m_propagationDelayStream->GetStream() << 0.0 << " " << oldDelay.Get().GetSeconds() << std::endl;
                m_firstPropagationDelayEntry = false;
            }
            *m_propagationDelayStream->GetStream() << Simulator::Now().GetSeconds() << " "
                                                   << oldDelay.Get().GetSeconds() << std::endl;
            *m_propagationDelayStream->GetStream() << Simulator::Now().GetSeconds() << " "
                                                   << delay.GetSeconds() << std::endl;
        }
    }

    void ScheduleStochasticBottleneckRate(dumbbell topology,
                                          uint64_t upperBitRate,
                                          Time interval = MilliSeconds(1),
                                          double tau = 0.5,
                                          double sigmaLog = 0.62)
    {
        PointToPointNetDevice* device = static_cast<PointToPointNetDevice*>(PeekPointer(topology.GetRouterDevices(0)));
        DataRateValue currentRate;
        device->GetAttribute("DataRate", currentRate);
        const uint64_t currentBps = currentRate.Get().GetBitRate();

        if (!m_ouInitialised)
        {
            m_ouMeanBps = currentBps;
            const double theta = 1.0 / tau;
            const double stationaryVar = sigmaLog * sigmaLog / (2.0 * theta);
            m_ouMeanLog = std::log(std::max<uint64_t>(m_ouMeanBps, 1)) - 0.5 * stationaryVar;
            m_ouStateLog = m_ouMeanLog;
            m_ouNormal = CreateObject<NormalRandomVariable>();
            m_ouNormal->SetAttribute("Mean", DoubleValue(0.0));
            m_ouNormal->SetAttribute("Variance", DoubleValue(1.0));
            m_ouNormal->SetStream(42);
            m_ouInitialised = true;
        }

        const double dt = interval.GetSeconds();
        const double theta = 1.0 / tau;
        const double z = m_ouNormal->GetValue();
        m_ouStateLog += theta * (m_ouMeanLog - m_ouStateLog) * dt + sigmaLog * std::sqrt(dt) * z;

        uint64_t newBps = static_cast<uint64_t>(std::exp(m_ouStateLog));
        newBps = std::max<uint64_t>(newBps, 1000);
        newBps = std::max<uint64_t>(newBps, 25 * m_ouMeanBps / 100);
        newBps = std::min<uint64_t>(newBps, 175 * m_ouMeanBps / 100);
        if (upperBitRate > 0)
        {
            newBps = std::min<uint64_t>(newBps, upperBitRate);
        }

        device->SetDataRate(DataRate(newBps));

        if (m_bottleneckRateStream)
        {
            if (m_firstBottleneckRateEntry)
            {
                *m_bottleneckRateStream->GetStream() << 0.0 << " " << currentBps * 1e-6 << " "
                                                      << m_ouMeanBps * 1e-6 << std::endl;
                m_firstBottleneckRateEntry = false;
            }
            *m_bottleneckRateStream->GetStream() << Simulator::Now().GetSeconds() << " "
                                                  << newBps * 1e-6 << " " << m_ouMeanBps * 1e-6 << std::endl;
        }

        Simulator::Schedule(interval,
                            &TraceState::ScheduleStochasticBottleneckRate,
                            this,
                            topology,
                            upperBitRate,
                            interval,
                            tau,
                            sigmaLog);
    }

  private:
    void ConnectSocketTraces(uint32_t nodeId)
    {
        const std::string socketPath = "/NodeList/" + std::to_string(nodeId) + "/$ns3::TcpL4Protocol/SocketList/0/";

        Config::ConnectWithoutContext(socketPath + "CongestionWindow", MakeBoundCallback(&TraceState::CwndTracer, this, nodeId));
        Config::ConnectWithoutContext(socketPath + "SlowStartThreshold", MakeBoundCallback(&TraceState::SsthreshTracer, this, nodeId));
        Config::ConnectWithoutContext(socketPath + "RTT", MakeBoundCallback(&TraceState::RttTracer, this, nodeId));
        Config::ConnectWithoutContext(socketPath + "RTO", MakeBoundCallback(&TraceState::RtoTracer, this, nodeId));
        Config::ConnectWithoutContext(socketPath + "BytesInFlight", MakeBoundCallback(&TraceState::InFlightTracer, this, nodeId));

        Config::ConnectWithoutContext("/NodeList/" + std::to_string(nodeId) + "/$ns3::TcpL4Protocol/SocketList/*/Tx",
                                      MakeBoundCallback(&TraceState::TcpTxDataTracer, this, nodeId));
    }

    static void CwndTracer(TraceState* state, uint32_t nodeId, uint32_t oldValue, uint32_t newValue)
    {
        if (state->m_config.enableTraces && state->m_cwndStream[nodeId])
        {
            if (state->m_firstCwnd[nodeId])
            {
                *state->m_cwndStream[nodeId]->GetStream() << "0.0 " << oldValue << std::endl;
                state->m_firstCwnd[nodeId] = false;
            }
            *state->m_cwndStream[nodeId]->GetStream() << Simulator::Now().GetSeconds() << " " << newValue << std::endl;
        }
        state->m_cwndValue[nodeId] = newValue;
    }

    static void SsthreshTracer(TraceState* state, uint32_t nodeId, uint32_t oldValue, uint32_t newValue)
    {
        if (state->m_config.enableTraces && state->m_ssthreshStream[nodeId])
        {
            if (state->m_firstSsthresh[nodeId])
            {
                *state->m_ssthreshStream[nodeId]->GetStream() << "0.0 " << oldValue << std::endl;
                state->m_firstSsthresh[nodeId] = false;
            }
            *state->m_ssthreshStream[nodeId]->GetStream() << Simulator::Now().GetSeconds() << " " << newValue << std::endl;
        }
        state->m_ssthreshValue[nodeId] = newValue;
    }

    static void RttTracer(TraceState* state, uint32_t nodeId, Time oldValue, Time newValue)
    {
        if (state->m_config.enableTraces && state->m_rttStream[nodeId])
        {
            if (state->m_firstRtt[nodeId])
            {
                *state->m_rttStream[nodeId]->GetStream() << "0.0 " << oldValue.GetSeconds() << std::endl;
                state->m_firstRtt[nodeId] = false;
            }
            *state->m_rttStream[nodeId]->GetStream() << Simulator::Now().GetSeconds() << " " << newValue.GetSeconds() << std::endl;
        }
        state->m_rttValue[nodeId] = newValue.GetSeconds();
    }

    static void RtoTracer(TraceState* state, uint32_t nodeId, Time oldValue, Time newValue)
    {
        if (state->m_config.enableTraces && state->m_rtoStream[nodeId])
        {
            if (state->m_firstRto[nodeId])
            {
                *state->m_rtoStream[nodeId]->GetStream() << "0.0 " << oldValue.GetSeconds() << std::endl;
                state->m_firstRto[nodeId] = false;
            }
            *state->m_rtoStream[nodeId]->GetStream() << Simulator::Now().GetSeconds() << " " << newValue.GetSeconds() << std::endl;
        }
    }

    static void InFlightTracer(TraceState* state, uint32_t nodeId, uint32_t, uint32_t inFlight)
    {
        if (state->m_config.enableTraces && state->m_inFlightStream[nodeId])
        {
            *state->m_inFlightStream[nodeId]->GetStream() << Simulator::Now().GetSeconds() << " " << inFlight << std::endl;
        }
        state->m_inFlightValue[nodeId] = inFlight;

        const double now = Simulator::Now().GetSeconds();
        state->m_averageThroughput[nodeId] = (now > 0.0) ? state->m_txBytes[nodeId] * 8.0e-6 / now : 0.0;
    }

    static void TcpTxDataTracer(TraceState* state,
                                uint32_t nodeId,
                                Ptr<const Packet> packet,
                                const TcpHeader& header,
                                Ptr<const TcpSocketBase> socket)
    {
        const uint32_t packetSize = packet->GetSize();
        state->m_aggregateTxBytes += packetSize;
        state->m_txBytes[nodeId] += packetSize;

        if (socket->GetTxBuffer()->IsHeadRetransmitted())
        {
            state->m_aggregateRetransBytes += packetSize;
            state->m_retransBytes[nodeId] += packetSize;
        }

        if (socket->GetTxBuffer()->IsLost(header.GetSequenceNumber()))
        {
            state->m_aggregateLostBytes += packetSize;
            state->m_lostBytes[nodeId] += packetSize;
        }

        state->m_aggregateAckBytes = state->m_aggregateTxBytes - state->m_aggregateRetransBytes;
    }

    void PrintProgress()
    {
        const AggregateSnapshot snapshot = BuildAggregateSnapshot();
        std::cout << Simulator::Now().GetSeconds()
                  << " cwndPkts=" << snapshot.cwndPackets
                  << " inflightPkts=" << snapshot.inFlightPackets
                  << " avgRttMs=" << snapshot.averageRttMs << std::endl;

        Simulator::Schedule(Seconds(0.5), &TraceState::PrintProgress, this);
    }

    struct AggregateSnapshot
    {
        uint32_t cwndPackets = 0;
        uint32_t inFlightPackets = 0;
        uint32_t averageRttMs = 0;
    };

    AggregateSnapshot BuildAggregateSnapshot() const
    {
        uint64_t cwndBytes = 0;
        uint64_t inFlightBytes = 0;
        double rttSum = 0.0;

        for (const auto& [nodeId, value] : m_cwndValue)
        {
            cwndBytes += value;
        }
        for (const auto& [nodeId, value] : m_inFlightValue)
        {
            inFlightBytes += value;
        }
        for (const auto& [nodeId, value] : m_rttValue)
        {
            rttSum += value;
        }

        AggregateSnapshot snapshot;
        snapshot.cwndPackets = static_cast<uint32_t>(cwndBytes / m_config.segmentSize);
        snapshot.inFlightPackets = static_cast<uint32_t>(inFlightBytes / m_config.segmentSize);
        snapshot.averageRttMs = m_rttValue.empty() ? 0 : static_cast<uint32_t>((rttSum / m_rttValue.size()) * 1000.0);
        return snapshot;
    }

    void LogMicroMetrics()
    {
        if (!m_microMetricsStream)
        {
            return;
        }

        if (m_firstMicroEntry)
        {
            *m_microMetricsStream->GetStream() << "time_s cwnd_pkts inflight_pkts avg_rtt_ms" << std::endl;
            m_firstMicroEntry = false;
        }

        const AggregateSnapshot snapshot = BuildAggregateSnapshot();
        *m_microMetricsStream->GetStream() << Simulator::Now().GetSeconds() << " "
                                           << snapshot.cwndPackets << " "
                                           << snapshot.inFlightPackets << " "
                                           << snapshot.averageRttMs << std::endl;

        Simulator::Schedule(Seconds(m_microInterval), &TraceState::LogMicroMetrics, this);
    }

    void LogMacroMetrics()
    {
        if (!m_macroMetricsStream)
        {
            return;
        }

        const double now = Simulator::Now().GetSeconds();
        const double dt = std::max(now - m_lastMacroTime, std::numeric_limits<double>::epsilon());

        const double intervalThroughputMbps = (m_aggregateTxBytes - m_lastAggregateTxBytes) * 8.0e-6 / dt;
        const double averageThroughputMbps = now > 0.0 ? m_aggregateTxBytes * 8.0e-6 / now : 0.0;
        const double intervalGoodputMbps = (m_aggregateAckBytes - m_lastAggregateAckBytes) * 8.0e-6 / dt;
        const double averageGoodputMbps = now > 0.0 ? m_aggregateAckBytes * 8.0e-6 / now : 0.0;
        const double fairness = JainFairnessIndex();

        if (m_firstMacroEntry)
        {
            *m_macroMetricsStream->GetStream()
                << "time_s interval_throughput_kbps average_throughput_kbps interval_goodput_kbps average_goodput_kbps "
                << "tx_pkts ack_pkts retrans_pkts lost_pkts fairness" << std::endl;
            m_firstMacroEntry = false;
        }

        *m_macroMetricsStream->GetStream() << now << " "
                                           << static_cast<uint32_t>(intervalThroughputMbps * 1000.0) << " "
                                           << static_cast<uint32_t>(averageThroughputMbps * 1000.0) << " "
                                           << static_cast<uint32_t>(intervalGoodputMbps * 1000.0) << " "
                                           << static_cast<uint32_t>(averageGoodputMbps * 1000.0) << " "
                                           << m_aggregateTxBytes / m_config.segmentSize << " "
                                           << m_aggregateAckBytes / m_config.segmentSize << " "
                                           << m_aggregateRetransBytes / m_config.segmentSize << " "
                                           << m_aggregateLostBytes / m_config.segmentSize << " "
                                           << fairness << std::endl;

        m_lastMacroTime = now;
        m_lastAggregateTxBytes = m_aggregateTxBytes;
        m_lastAggregateAckBytes = m_aggregateAckBytes;

        Simulator::Schedule(Seconds(m_macroInterval), &TraceState::LogMacroMetrics, this);
    }

    double JainFairnessIndex() const
    {
        double sum = 0.0;
        double sumSquares = 0.0;
        for (const auto& [nodeId, value] : m_averageThroughput)
        {
            sum += value;
            sumSquares += value * value;
        }
        if (m_averageThroughput.empty() || sumSquares <= 0.0)
        {
            return 0.0;
        }
        return (sum * sum) / (static_cast<double>(m_averageThroughput.size()) * sumSquares);
    }

    const SimulationConfig& m_config;
    OutputPaths m_paths;

    double m_microInterval;
    double m_macroInterval;

    std::map<uint32_t, bool> m_firstCwnd;
    std::map<uint32_t, bool> m_firstSsthresh;
    std::map<uint32_t, bool> m_firstRtt;
    std::map<uint32_t, bool> m_firstRto;

    std::map<uint32_t, Ptr<OutputStreamWrapper>> m_cwndStream;
    std::map<uint32_t, Ptr<OutputStreamWrapper>> m_ssthreshStream;
    std::map<uint32_t, Ptr<OutputStreamWrapper>> m_rttStream;
    std::map<uint32_t, Ptr<OutputStreamWrapper>> m_rtoStream;
    std::map<uint32_t, Ptr<OutputStreamWrapper>> m_inFlightStream;

    Ptr<OutputStreamWrapper> m_microMetricsStream;
    Ptr<OutputStreamWrapper> m_macroMetricsStream;
    Ptr<OutputStreamWrapper> m_bottleneckRateStream;
    Ptr<OutputStreamWrapper> m_propagationDelayStream;

    std::map<uint32_t, uint32_t> m_cwndValue;
    std::map<uint32_t, uint32_t> m_ssthreshValue;
    std::map<uint32_t, uint32_t> m_inFlightValue;
    std::map<uint32_t, double> m_rttValue;
    std::map<uint32_t, double> m_averageThroughput;

    std::map<uint32_t, uint64_t> m_txBytes;
    std::map<uint32_t, uint64_t> m_retransBytes;
    std::map<uint32_t, uint64_t> m_lostBytes;

    uint64_t m_aggregateTxBytes = 0;
    uint64_t m_aggregateAckBytes = 0;
    uint64_t m_aggregateRetransBytes = 0;
    uint64_t m_aggregateLostBytes = 0;

    double m_lastMacroTime = 0.0;
    uint64_t m_lastAggregateTxBytes = 0;
    uint64_t m_lastAggregateAckBytes = 0;

    bool m_firstMicroEntry = true;
    bool m_firstMacroEntry = true;
    bool m_firstBottleneckRateEntry = true;
    bool m_firstPropagationDelayEntry = true;

    bool m_ouInitialised = false;
    uint64_t m_ouMeanBps = 0;
    double m_ouMeanLog = 0.0;
    double m_ouStateLog = 0.0;
    Ptr<NormalRandomVariable> m_ouNormal;
};

inline bool IsTcpQtController(const std::string& transport)
{
    return transport.rfind("TcpQt", 0) == 0;
}

inline void ConfigureTcpDefaults(const SimulationConfig& config)
{
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("1p"));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + config.transport));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1073741824));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1073741824));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(config.initialCwnd));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(config.delayedAckCount));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(config.segmentSize));

    if (IsTcpQtController(config.transport))
    {
        Config::SetDefault("ns3::" + config.transport + "::targetRttAlpha", DoubleValue(config.targetRttAlpha));
        if (config.transport == "TcpQtSdr")
        {
            Config::SetDefault("ns3::TcpQtSdr::minRttEpsilon", DoubleValue(config.minRttEpsilon));
        }
    }
}

inline std::string FormatQtParameterSuffix(const SimulationConfig& config)
{
    std::ostringstream suffix;
    if (IsTcpQtController(config.transport) && std::abs(config.targetRttAlpha - 1.2) > 1.0e-9)
    {
        suffix << "_alpha" << static_cast<uint32_t>(std::lround(config.targetRttAlpha * 100.0));
    }
    if (config.transport == "TcpQtSdr" && std::abs(config.minRttEpsilon - 1.0) > 1.0e-9)
    {
        suffix << "_eps" << static_cast<uint32_t>(std::lround(config.minRttEpsilon * 100.0));
    }
    return suffix.str();
}

inline OutputPaths BuildOutputPaths(const SimulationConfig& config, uint32_t queueSizePackets)
{
    const std::string transportLabel = config.transport + FormatQtParameterSuffix(config);

    OutputPaths paths;
    paths.prefix = transportLabel + "-" + std::to_string(config.numFlows) + "-flows-" +
                   config.bottleneckRate + "-" + config.bottleneckDelay + "-" +
                   std::to_string(queueSizePackets) + "p";

    paths.directory = config.simName + "/" + transportLabel + "/" +
                      std::to_string(config.numFlows) + "-flows/" +
                      config.bottleneckRate + "-" + config.bottleneckDelay + "/" +
                      std::to_string(queueSizePackets) + "p-btlqueue/";

    std::filesystem::create_directories(paths.directory);
    return paths;
}

inline void PrintSimulationSummary(const SimulationConfig& config, uint32_t queueSizePackets, const OutputPaths& paths)
{
    std::cout << "Simulation configuration" << std::endl
              << "  transport        : " << config.transport << std::endl
              << "  flows            : " << config.numFlows << std::endl
              << "  bottleneck       : " << config.bottleneckRate << ", " << config.bottleneckDelay << std::endl
              << "  access           : " << config.accessRate << ", " << config.accessDelay << std::endl
              << "  queue            : " << queueSizePackets << " packets" << std::endl
              << "  errorRate        : " << config.errorRate << std::endl
              << "  duration         : " << config.simDuration << " s" << std::endl
              << "  output directory : " << paths.directory << std::endl;

    if (IsTcpQtController(config.transport))
    {
        std::cout << "  targetRttAlpha   : " << config.targetRttAlpha << std::endl;
        if (config.transport == "TcpQtSdr")
        {
            std::cout << "  minRttEpsilon    : " << config.minRttEpsilon << std::endl;
        }
    }
}

} // namespace tcp_qt_examples
} // namespace ns3

#endif // TCP_QT_EXAMPLES_H
