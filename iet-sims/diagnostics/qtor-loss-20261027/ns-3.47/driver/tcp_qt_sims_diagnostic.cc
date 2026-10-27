/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * IET Queueing-Dynamic Predictor-Corrector TCP experiments.
 * Reference: supplied IET_Networks_TcpOptimal_clean.pdf, Sections 6.1-6.2,
 * Tables 2-4 and Appendix C. One invocation runs one resolved configuration.
 *
 * Examples:
 * ./ns3 run 'tcp_qt_sims --scenario=static --transport_prot=TcpQtSdr'
 * ./ns3 run 'tcp_qt_sims --scenario=buffer --queuePackets=483'
 * ./ns3 run 'tcp_qt_sims --scenario=loss --error_p=1e-4'
 * ./ns3 run 'tcp_qt_sims --scenario=stochastic --regime=high'
 * ./ns3 run 'tcp_qt_sims --PrintHelp'
 *
 * The step-delay scenario is an extension from the supplied examples, not an
 * experiment reported in the IET article. Access-link values and TCP startup
 * settings omitted by the article are explicitly recorded implementation choices.
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ns3;

namespace
{
/** @brief Resolved experiment settings; each configurable field has a CLI option. */
struct Settings
{
    std::string scenario{"static"};             ///< Experiment class.
    std::string regime{"low"};                  ///< Table 4 stochastic regime.
    std::string transport{"TcpQtSdr"};          ///< Sender congestion-control type.
    std::string recovery{"TcpClassicRecovery"}; ///< Loss-recovery implementation.
    std::string bottleneckRate{"100Mbps"};      ///< Nominal physical bottleneck capacity.
    std::string bottleneckDelay{"20ms"};        ///< One-way bottleneck propagation delay.
    std::string accessRate{"10Gbps"};  ///< Overprovisioned access rate, from supplied code.
    std::string accessDelay{"0.01ms"}; ///< Per-access-link delay, from supplied code.
    std::string outputRoot{
        "/Users/dumisangwenya/Development/ns3-install/iet-sims/trace-results"}; ///< Root output
                                                                                    ///< directory.
    uint32_t flows{5};           ///< Saturated, homogeneous sender/receiver pairs.
    uint32_t mss{1448};          ///< TCP maximum segment size in bytes.
    uint32_t mtu{1500};          ///< Point-to-point MTU in bytes.
    uint32_t queuePackets{1381}; ///< Exact waiting-packet capacity per bottleneck direction.
    uint32_t queueBytes{0};      ///< Optional legacy byte budget; zero selects packet capacity.
    uint32_t accessQueuePackets{10000}; ///< Access device queue capacity.
    uint32_t socketBuffer{1073741824};  ///< Send and receive buffers in bytes.
    uint32_t initialCwnd{10};           ///< Initial congestion window in segments.
    uint32_t delayedAckCount{2};        ///< Delayed ACK ratio.
    double duration{100.0};             ///< Simulation stop time in seconds.
    double appStart{0.00001};           ///< Common sender start time: 10 microseconds.
    double warmup{0.00001};             ///< Measurement begins with the senders.
    double samplingPhaseSeconds{0.001}; ///< Absolute sampling phase in seconds.
    double interval{0.1};               ///< Evaluation interval, independent of controller updates.
    double errorRate{0.0};              ///< Independent forward-link packet error probability.
    bool reverseErrors{false};          ///< Also apply errors to reverse bottleneck traffic.
    double alpha{1.2};                  ///< Target RTT / minimum RTT.
    double epsilon{1.0};                ///< QtSDR corrector mixing parameter.
    double muGain{0.1};                 ///< QtTOR service-rate smoothing gain.
    double muMin{1e-6};                 ///< QtTOR service-rate floor in packets/s.
    double minControlScale{25.0};       ///< QtTOR control-scale floor in packets/s.
    double controlFraction{0.05};       ///< QtTOR rate-based control scale fraction.
    double ouStart{0.0};                ///< First stochastic update time.
    double ouInterval{0.001};           ///< Appendix C Euler-Maruyama integration interval.
    double ouTau{8.0};                  ///< OU correlation time in seconds.
    double ouSigma{0.15};               ///< OU log-domain diffusion coefficient.
    double ouLower{0.25};               ///< Capacity lower bound relative to nominal.
    double ouUpper{1.75};               ///< Capacity upper bound relative to nominal.
    bool bidirectionalOu{false}; ///< Vary reverse capacity too; legacy code varies forward only.
    uint32_t seed{12345};        ///< Common ns-3 seed.
    uint64_t run{1};             ///< Common ns-3 run number.
    int64_t ouStream{42};        ///< Dedicated OU random stream, independent of TCP activity.
    int64_t tcpStream{100};      ///< Internet-stack stream allocation start.
    int64_t errorStream{10000};  ///< Forward and reverse packet-error streams.
    std::string delayTimes{"15,25,35"};        ///< Extension's delay-change times in seconds.
    std::string delayValues{"20ms,40ms,10ms"}; ///< Extension's successive one-way delays.
    bool sack{true};                           ///< Enable TCP SACK.
    bool timestamps{true};                     ///< Enable TCP timestamps.
    bool tracing{false};                       ///< Optional high-resolution socket event file.
    bool pcap{false};                          ///< Optional bottleneck packet capture.
    bool progress{false};                      ///< Print progress at integer seconds.
    bool overwrite{false}; ///< Explicit permission to reuse the same resolved output directory.
    bool dryRun{false};    ///< Validate and print settings without creating the network.
};

/**
 * @brief Read a scenario selector before declaring scenario-specific CLI defaults.
 * @param argc Number of arguments.
 * @param argv Argument strings.
 * @param name Selector name.
 * @param fallback Value when not supplied.
 * @return Last --name=value selector, matching CommandLine precedence.
 */
std::string
Selector(int argc, char* argv[], const std::string& name, std::string fallback)
{
    for (int i = 1; i < argc; ++i)
    {
        for (const auto& prefix : {"--" + name + "=", "-" + name + "="})
        {
            const std::string arg(argv[i]);
            if (arg.rfind(prefix, 0) == 0)
            {
                fallback = arg.substr(prefix.size());
            }
        }
    }
    return fallback;
}

/**
 * @brief Parse a comma-delimited list without silently accepting empty entries.
 * @param text Input list.
 * @return Nonempty tokens.
 */
std::vector<std::string>
Split(const std::string& text)
{
    std::vector<std::string> result;
    std::istringstream input(text);
    std::string token;
    while (std::getline(input, token, ','))
    {
        if (token.empty())
        {
            throw std::runtime_error("Empty item in comma-separated list");
        }
        result.push_back(token);
    }
    if (result.empty() || text.back() == ',')
    {
        throw std::runtime_error("Empty comma-separated list");
    }
    return result;
}

/**
 * @brief Register runtime overrides, retaining paper/legacy naming where practical.
 * @param c Mutable settings.
 * @param argc Argument count.
 * @param argv Argument values.
 */
void
ParseSettings(Settings& c, int argc, char* argv[])
{
    c.scenario = Selector(argc, argv, "scenario", c.scenario);
    c.regime = Selector(argc, argv, "regime", c.regime);
    if (c.scenario == "loss")
    {
        c.errorRate = 1e-6;
    }
    if (c.regime == "moderate")
    {
        c.ouTau = 2.0;
        c.ouSigma = 0.45;
    }
    else if (c.regime == "high")
    {
        c.ouTau = 0.5;
        c.ouSigma = 1.0;
    }
    CommandLine cmd(__FILE__);
    cmd.Usage(
        "One IET experiment per invocation; all options use --name=value. "
        "Presets are resolved before explicit overrides. See --dryRun=1 for the final settings.");
    cmd.AddValue("scenario",
                 "static, buffer, loss, stochastic, or step-delay (extension)",
                 c.scenario);
    cmd.AddValue("regime", "Table 4 OU regime: low, moderate, high", c.regime);
    cmd.AddValue("transport_prot", "TcpQtSdr, TcpQtTor, TcpNewReno, TcpCubic, TcpBbr", c.transport);
    cmd.AddValue("recovery", "ns-3 TCP recovery type", c.recovery);
    cmd.AddValue("btlBW",
                 "Nominal bottleneck rate; OU stationary mean before clipping",
                 c.bottleneckRate);
    cmd.AddValue("btlDelay", "One-way bottleneck delay", c.bottleneckDelay);
    cmd.AddValue("accessBW", "Access-link capacity (implementation choice)", c.accessRate);
    cmd.AddValue("accessDelay", "One-way delay of EACH of two access links", c.accessDelay);
    cmd.AddValue("num_flows", "Number of long-lived bulk TCP flows", c.flows);
    cmd.AddValue("segmentSize", "TCP MSS in bytes", c.mss);
    cmd.AddValue("mtu", "Point-to-point MTU in bytes", c.mtu);
    cmd.AddValue("queuePackets",
                 "DropTail bottleneck capacity: article uses 483, 690, 1381",
                 c.queuePackets);
    cmd.AddValue("queueSize_bytes",
                 "Legacy budget, floor(bytes/MSS); use 0 for queuePackets",
                 c.queueBytes);
    cmd.AddValue("accessQueuePackets", "Access-link DropTail queue capacity", c.accessQueuePackets);
    cmd.AddValue("socketBuffer", "Host send/receive buffer size in bytes", c.socketBuffer);
    cmd.AddValue("initialCwnd", "Initial congestion window in segments", c.initialCwnd);
    cmd.AddValue("delayedAckCount", "ACK every this many data segments", c.delayedAckCount);
    cmd.AddValue("sim_duration", "Simulation stop time in seconds", c.duration);
    cmd.AddValue("appStart", "Common start time of all senders in seconds", c.appStart);
    cmd.AddValue("warmup", "Measurement start time in seconds; 0 includes startup", c.warmup);
    cmd.AddValue("samplingPhase", "Absolute sample phase in seconds; samples follow phase + k*interval", c.samplingPhaseSeconds);
    cmd.AddValue("data_logging_interval", "Metric sampling interval in seconds", c.interval);
    cmd.AddValue("error_p",
                 "Independent bottleneck packet error probability (not bit error rate)",
                 c.errorRate);
    cmd.AddValue("reverseErrors",
                 "Also corrupt reverse bottleneck packets on a separate stream",
                 c.reverseErrors);
    cmd.AddValue("targetRttAlpha", "Qt controller RTT target multiplier", c.alpha);
    cmd.AddValue("minRttEpsilon", "QtSDR corrector mixing parameter [0,1]", c.epsilon);
    cmd.AddValue("muGain", "QtTOR service-rate smoothing gain [0,1]", c.muGain);
    cmd.AddValue("muMin", "QtTOR minimum service-rate estimate, packets/s", c.muMin);
    cmd.AddValue("minControlScale", "QtTOR minimum control scale, packets/s", c.minControlScale);
    cmd.AddValue("controlFraction", "QtTOR rate-scale factor", c.controlFraction);
    cmd.AddValue("stoch_start", "First OU update time in seconds", c.ouStart);
    cmd.AddValue("stoch_interval", "Euler-Maruyama step in seconds", c.ouInterval);
    cmd.AddValue("stoch_tau", "OU correlation time; overrides selected regime", c.ouTau);
    cmd.AddValue("stoch_sigmaLog",
                 "OU diffusion coefficient; overrides selected regime",
                 c.ouSigma);
    cmd.AddValue("stoch_lowerFactor",
                 "Lower clipping factor; physical floor also 1000 bps",
                 c.ouLower);
    cmd.AddValue("stoch_upperFactor", "Upper clipping factor", c.ouUpper);
    cmd.AddValue("stoch_bidirectional",
                 "Apply same OU capacity to reverse link too",
                 c.bidirectionalOu);
    cmd.AddValue("rngSeed", "Common RNG seed", c.seed);
    cmd.AddValue("rngRun", "Common RNG run number", c.run);
    cmd.AddValue("ouStream", "Dedicated OU normal stream", c.ouStream);
    cmd.AddValue("tcpStream", "Starting Internet-stack stream", c.tcpStream);
    cmd.AddValue("errorStream", "First of two independent packet-error streams", c.errorStream);
    cmd.AddValue("delayTimes",
                 "Step-delay extension: comma-separated change times, seconds",
                 c.delayTimes);
    cmd.AddValue("delayValues",
                 "Step-delay extension: matching one-way delay values",
                 c.delayValues);
    cmd.AddValue("sack", "Enable TCP SACK", c.sack);
    cmd.AddValue("timestamps", "Enable TCP timestamps", c.timestamps);
    cmd.AddValue("tracing", "Write optional high-resolution per-flow socket events", c.tracing);
    cmd.AddValue("pcap", "Capture both bottleneck directions", c.pcap);
    cmd.AddValue("progress", "Print progress once per second", c.progress);
    // Preserve whitespace in filesystem paths instead of using stream extraction.
    cmd.AddValue("sim_name",
                 "Results root; each configuration gets a separate directory",
                 Callback<bool, const std::string&>([&c](const std::string& value) {
                     c.outputRoot = value;
                     return true;
                 }),
                 c.outputRoot);
    cmd.AddValue("overwrite",
                 "Explicitly replace outputs for an existing configuration",
                 c.overwrite);
    cmd.AddValue("dryRun", "Print resolved settings and exit without simulating", c.dryRun);
    cmd.Parse(argc, argv);
    if (c.transport.rfind("ns3::", 0) == 0)
    {
        c.transport.erase(0, 5);
    }
    if (c.recovery.rfind("ns3::", 0) == 0)
    {
        c.recovery.erase(0, 5);
    }
    if (c.queueBytes)
    {
        if (Selector(argc, argv, "queuePackets", "").size())
        {
            throw std::runtime_error("Choose queuePackets OR queueSize_bytes, not both");
        }
        if (c.mss == 0 || c.queueBytes < c.mss)
        {
            throw std::runtime_error("queueSize_bytes must contain at least one MSS");
        }
        c.queuePackets = c.queueBytes / c.mss;
    }
}

/**
 * @brief Reject invalid settings before creating outputs or simulation objects.
 * @param c Resolved settings.
 */
void
Validate(const Settings& c)
{
    auto require = [](bool condition, const std::string& message) {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    };
    const std::vector<std::string> scenarios{"static",
                                             "buffer",
                                             "loss",
                                             "stochastic",
                                             "step-delay"};
    require(std::find(scenarios.begin(), scenarios.end(), c.scenario) != scenarios.end(),
            "Unknown scenario");
    require(c.regime == "low" || c.regime == "moderate" || c.regime == "high", "Unknown OU regime");
    require(c.flows > 0 && c.flows <= 4096, "num_flows must be in [1,4096]");
    require(c.mss > 0 && c.mtu >= static_cast<uint64_t>(c.mss) + 40 + (c.timestamps ? 12 : 0),
            "MTU must accommodate MSS plus IPv4/TCP headers and timestamp options");
    require(c.queuePackets > 0 && c.accessQueuePackets > 0, "Queues must be nonempty");
    require(c.socketBuffer >= c.mss && c.initialCwnd > 0 && c.delayedAckCount > 0,
            "Invalid TCP buffer/startup settings");
    require(std::isfinite(c.duration) && c.duration > 0 && std::isfinite(c.appStart) &&
                c.appStart >= 0 && c.appStart < c.duration,
            "Require 0 <= appStart < sim_duration");
    require(std::isfinite(c.warmup) && c.warmup >= c.appStart && c.warmup < c.duration,
            "Require appStart <= warmup < sim_duration");
    require(Seconds(c.duration) > Seconds(c.warmup) && Seconds(c.duration) > Seconds(c.appStart),
            "Duration must exceed start times at the simulator time resolution");
    require(std::isfinite(c.interval) && c.interval > 0 && Seconds(c.interval) > Time(0),
            "Invalid sampling interval");
    require(std::isfinite(c.samplingPhaseSeconds) && c.samplingPhaseSeconds >= 0 &&
                c.samplingPhaseSeconds < c.duration,
            "Require 0 <= samplingPhase < sim_duration");
    require(std::isfinite(c.errorRate) && c.errorRate >= 0 && c.errorRate <= 1,
            "error_p must be in [0,1]");
    require(std::isfinite(c.alpha) && c.alpha >= 1 && std::isfinite(c.epsilon) && c.epsilon >= 0 &&
                c.epsilon <= 1,
            "Require targetRttAlpha >= 1 and minRttEpsilon in [0,1]");
    require(std::isfinite(c.muGain) && c.muGain >= 0 && c.muGain <= 1 && std::isfinite(c.muMin) &&
                c.muMin > 0 && std::isfinite(c.minControlScale) && c.minControlScale > 0 &&
                std::isfinite(c.controlFraction) && c.controlFraction >= 0,
            "Invalid QtTOR scaling settings");
    require(c.seed > 0 && c.seed < 2147483647U && c.run > 0 && c.ouStream >= 0 &&
                c.tcpStream >= 0 && c.errorStream >= 0 &&
                c.errorStream < std::numeric_limits<int64_t>::max(),
            "Invalid RNG setting");
    require(c.ouStream != c.errorStream && c.ouStream != c.errorStream + 1,
            "OU and error streams must be separate");
    require(DataRate(c.bottleneckRate).GetBitRate() > 0 && DataRate(c.accessRate).GetBitRate() > 0,
            "Link rates must be positive");
    require(Time(c.bottleneckDelay) >= Time(0) && Time(c.accessDelay) >= Time(0) &&
                Time(c.bottleneckDelay) + 2 * Time(c.accessDelay) > Time(0),
            "Invalid propagation delay");
    if (c.scenario == "stochastic")
    {
        require(std::isfinite(c.ouInterval) && c.ouInterval > 0 &&
                    Seconds(c.ouInterval) > Time(0) && std::isfinite(c.ouTau) && c.ouTau > 0 &&
                    c.ouInterval < 2 * c.ouTau,
                "OU step must satisfy 0 < stoch_interval < 2*stoch_tau");
        require(std::isfinite(c.ouSigma) && c.ouSigma >= 0 && std::isfinite(c.ouStart) &&
                    c.ouStart >= 0 && c.ouStart < c.duration,
                "Invalid OU volatility/start time");
        const double nominal = DataRate(c.bottleneckRate).GetBitRate();
        require(std::isfinite(c.ouLower) && c.ouLower > 0 && std::isfinite(c.ouUpper) &&
                    c.ouUpper * nominal >= std::max(1000.0, c.ouLower * nominal) &&
                    c.ouUpper * nominal < static_cast<double>(std::numeric_limits<uint64_t>::max()),
                "Invalid OU clipping bounds");
        require(std::ceil(std::max(1000.0, c.ouLower * nominal)) <= std::floor(c.ouUpper * nominal),
                "OU clipping interval must contain a whole number of bits per second");
        require(std::isfinite(c.ouSigma * c.ouSigma * c.ouTau),
                "OU stationary variance must be finite");
    }
    if (c.scenario == "step-delay")
    {
        const auto times = Split(c.delayTimes);
        const auto delays = Split(c.delayValues);
        require(times.size() == delays.size(), "delayTimes and delayValues must have equal length");
        double previous = -1.0;
        for (size_t i = 0; i < times.size(); ++i)
        {
            size_t used = 0;
            const double t = std::stod(times[i], &used);
            require(used == times[i].size() && std::isfinite(t) && t > previous && t >= 0 &&
                        t < c.duration,
                    "Delay times must be strictly increasing and within simulation duration");
            require(Time(delays[i]) >= Time(0), "Step delays must be nonnegative");
            previous = t;
        }
    }
    TypeId algorithm;
    TypeId recovery;
    require(TypeId::LookupByNameFailSafe("ns3::" + c.transport, &algorithm) &&
                algorithm.IsChildOf(TcpCongestionOps::GetTypeId()) && algorithm.HasConstructor(),
            "Unavailable congestion controller: " + c.transport);
    require(TypeId::LookupByNameFailSafe("ns3::" + c.recovery, &recovery) &&
                recovery.IsChildOf(TcpRecoveryOps::GetTypeId()) && recovery.HasConstructor(),
            "Unavailable recovery type: " + c.recovery);
}

/**
 * @brief Serialize every effective setting for audit and configuration identification.
 * @param c Resolved settings.
 * @return Plain-text configuration, independent of output destination.
 */
std::string
Describe(const Settings& c)
{
    std::ostringstream s;
    s << std::setprecision(17) << std::boolalpha;
#define QT_SETTING(field) s << #field "=" << c.field << '\n'
    QT_SETTING(scenario);
    QT_SETTING(regime);
    QT_SETTING(transport);
    QT_SETTING(recovery);
    QT_SETTING(bottleneckRate);
    QT_SETTING(bottleneckDelay);
    QT_SETTING(accessRate);
    QT_SETTING(accessDelay);
    QT_SETTING(flows);
    QT_SETTING(mss);
    QT_SETTING(mtu);
    QT_SETTING(queuePackets);
    QT_SETTING(queueBytes);
    QT_SETTING(accessQueuePackets);
    QT_SETTING(socketBuffer);
    QT_SETTING(initialCwnd);
    QT_SETTING(delayedAckCount);
    QT_SETTING(duration);
    QT_SETTING(appStart);
    QT_SETTING(warmup);
    QT_SETTING(interval);
    QT_SETTING(samplingPhaseSeconds);
    QT_SETTING(errorRate);
    QT_SETTING(reverseErrors);
    QT_SETTING(alpha);
    QT_SETTING(epsilon);
    QT_SETTING(muGain);
    QT_SETTING(muMin);
    QT_SETTING(minControlScale);
    QT_SETTING(controlFraction);
    QT_SETTING(ouStart);
    QT_SETTING(ouInterval);
    QT_SETTING(ouTau);
    QT_SETTING(ouSigma);
    QT_SETTING(ouLower);
    QT_SETTING(ouUpper);
    QT_SETTING(bidirectionalOu);
    QT_SETTING(seed);
    QT_SETTING(run);
    QT_SETTING(ouStream);
    QT_SETTING(tcpStream);
    QT_SETTING(errorStream);
    QT_SETTING(delayTimes);
    QT_SETTING(delayValues);
    QT_SETTING(sack);
    QT_SETTING(timestamps);
    QT_SETTING(tracing);
    QT_SETTING(pcap);
    QT_SETTING(progress);
#undef QT_SETTING
    const double baseline =
        2 * Time(c.bottleneckDelay).GetSeconds() + 4 * Time(c.accessDelay).GetSeconds();
    s << "queue=DropTailQueue<Packet>; no root queue disc\n"
      << "rttReferenceSeconds=" << baseline << '\n'
      << "baseBdpPackets=" << DataRate(c.bottleneckRate).GetBitRate() * baseline / (8 * c.mss)
      << '\n'
      << "ouInitialState=log(C0)-sigmaLog^2*tau/4\n"
      << "ouUpdate=Euler-Maruyama; first innovation at stoch_start\n"
      << "power=receiverGoodputBps/currentForwardCapacityBps*rttReferenceSeconds/"
         "meanLastRttSeconds\n"
      << "rttTrace=LastRTT; NaN until every flow has a positive RTT\n"
      << "goodput=PacketSink unique in-order application payload bytes\n"
      << "fairness=Jain over all configured flows, including zero-throughput flows\n"
      << "qtTorFixed=N:5,M:10,eta:0.05,delta:0.05,wR:1,wu:1,wDeltaU:1\n"
      << "controllerVersion=local modified ns-3.49 Qt models; not the original paper binaries\n"
      << "reference=IET Section 6.1, Tables 2-4, Section 6.2 equations 50-54, Appendix C\n"
      << "extension=" << (c.scenario == "step-delay") << '\n';
    return s.str();
}

/**
 * @brief Stable FNV-1a configuration identifier, not a cryptographic checksum.
 * @param text Resolved settings.
 * @return Hexadecimal identifier.
 */
std::string
ConfigId(const std::string& text)
{
    uint64_t value = 14695981039346656037ULL;
    for (unsigned char ch : text)
    {
        value = (value ^ ch) * 1099511628211ULL;
    }
    std::ostringstream s;
    s << std::hex << value;
    return s.str();
}

/**
 * @brief Compute Jain fairness, retaining zero-valued configured flows.
 * @param values Per-flow delivered bytes or goodputs over the same interval.
 * @return Fairness, or zero when nothing was delivered.
 */
double
Fairness(const std::vector<double>& values)
{
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    const double squares = std::inner_product(values.begin(), values.end(), values.begin(), 0.0);
    return squares > 0 ? sum * sum / (values.size() * squares) : 0.0;
}

/** @brief One flow's application objects, sampled state and byte counters. */
struct Flow
{
    uint64_t diagTx{0}, diagRetrans{0}, diagRetransBytes{0}, diagRecovery{0}, diagLoss{0};
    double diagStateTime{0}, diagRecoveryTime{0}, diagLossTime{0};
    TcpSocketState::TcpCongState_t diagState{TcpSocketState::CA_OPEN};
    Ptr<BulkSendApplication> source;                      ///< Unlimited bulk-transfer sender.
    Ptr<PacketSink> sink;                                 ///< Unique application payload receiver.
    uint64_t baselineBytes{0};                            ///< Delivered bytes at measurement start.
    uint64_t lastBytes{0};                                ///< Delivered bytes at preceding sample.
    uint32_t cwnd{0};                                     ///< Latest congestion window in bytes.
    uint32_t inflight{0};                                 ///< Latest unacknowledged bytes.
    double rtt{std::numeric_limits<double>::quiet_NaN()}; ///< Last valid RTT sample, seconds.
    double minRtt{std::numeric_limits<double>::infinity()}; ///< Observed minimum RTT, seconds.
};

/** @brief One deterministic topology, traffic configuration, and metric collector. */
class Experiment
{
  public:
    /**
     * @param settings Resolved experiment settings.
     * @param directory Unique configuration output directory.
     */
    Experiment(const Settings& settings, const std::filesystem::path& directory)
        : m_c(settings),
          m_directory(directory),
          m_flows(settings.flows),
          m_nominal(DataRate(settings.bottleneckRate).GetBitRate()),
          m_capacity(m_nominal),
          m_delay(Time(settings.bottleneckDelay))
    {
    }

    /** @brief Construct and run the experiment, then write final measurement summaries. */
    void Run()
    {
        ConfigureTcp();
        BuildNetwork();
        Open(m_diag, "recovery-events.csv");
        m_diag << "time_s,flow,event,value,bytes\n";
        Open(m_aggregate, "aggregate.csv");
        Open(m_flowSamples, "flows.csv");
        Open(m_capacityTrace, "capacity.csv");
        Open(m_delayTrace, "delay.csv");
        if (m_c.tracing)
        {
            Open(m_events, "socket-events.csv");
            m_events << "time_s,flow,signal,value\n";
        }
        m_aggregate << "time_s,interval_s,goodput_mbps,mean_rtt_ms,valid_rtt_flows,"
                       "inflight_packets,cwnd_packets,capacity_bps,base_rtt_ms,power_norm,"
                       "jain_interval,jain_cumulative,queue_packets\n";
        m_flowSamples << "time_s,flow,interval_payload_bytes,total_payload_bytes,goodput_mbps,"
                         "rtt_ms,observed_min_rtt_ms,inflight_packets,cwnd_packets\n";
        m_capacityTrace << "time_s,forward_bps,reverse_bps\n0," << m_nominal << ',' << m_nominal
                        << '\n';
        m_delayTrace << "time_s,bottleneck_one_way_s,propagation_rtt_s\n0," << m_delay.GetSeconds()
                     << ',' << BaseRtt() << '\n';
        InstallApplications();
        Simulator::Schedule(Seconds(m_c.warmup), &Experiment::BeginMeasurements, this);
        if (m_c.scenario == "stochastic")
        {
            m_normal = CreateObject<NormalRandomVariable>();
            m_normal->SetStream(m_c.ouStream);
            m_logMean = std::log(m_nominal) - m_c.ouSigma * m_c.ouSigma * m_c.ouTau / 4.0;
            m_logCapacity = m_logMean;
            Simulator::Schedule(Seconds(m_c.ouStart), &Experiment::OuUpdate, this);
        }
        if (m_c.scenario == "step-delay")
        {
            const auto times = Split(m_c.delayTimes);
            const auto delays = Split(m_c.delayValues);
            for (size_t i = 0; i < times.size(); ++i)
            {
                Simulator::Schedule(Seconds(std::stod(times[i])),
                                    &Experiment::ChangeDelay,
                                    this,
                                    Time(delays[i]));
            }
        }
        if (m_c.progress)
        {
            Simulator::Schedule(Seconds(1), &Experiment::Progress, this);
        }
        // Permit the final sample at duration even when it was scheduled recursively.
        Simulator::Stop(Seconds(m_c.duration) + TimeStep(1));
        Simulator::Run();
        Summarize();
        Simulator::Destroy();
    }

  private:
    /** @brief Apply controller and host defaults before sockets are constructed. */
    void ConfigureTcp()
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           TypeIdValue(TypeId::LookupByName("ns3::" + m_c.transport)));
        Config::SetDefault("ns3::TcpL4Protocol::RecoveryType",
                           TypeIdValue(TypeId::LookupByName("ns3::" + m_c.recovery)));
        Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(m_c.mss));
        Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(m_c.socketBuffer));
        Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(m_c.socketBuffer));
        Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(m_c.initialCwnd));
        Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(m_c.delayedAckCount));
        Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(m_c.sack));
        Config::SetDefault("ns3::TcpSocketBase::Timestamp", BooleanValue(m_c.timestamps));
        if (m_c.transport == "TcpQtSdr" || m_c.transport == "TcpQtTor")
        {
            Config::SetDefault("ns3::" + m_c.transport + "::targetRttAlpha",
                               DoubleValue(m_c.alpha));
        }
        if (m_c.transport == "TcpQtSdr")
        {
            Config::SetDefault("ns3::TcpQtSdr::minRttEpsilon", DoubleValue(m_c.epsilon));
        }
        if (m_c.transport == "TcpQtTor")
        {
            Config::SetDefault("ns3::TcpQtTor::MuSmoothingGain", DoubleValue(m_c.muGain));
            Config::SetDefault("ns3::TcpQtTor::MinServiceRate", DoubleValue(m_c.muMin));
            Config::SetDefault("ns3::TcpQtTor::MinControlScale", DoubleValue(m_c.minControlScale));
            Config::SetDefault("ns3::TcpQtTor::ControlScaleFraction",
                               DoubleValue(m_c.controlFraction));
        }
    }

    /** @brief Build a dumbbell using only standard ns-3 helpers. */
    void BuildNetwork()
    {
        m_routers.Create(2);
        m_sources.Create(m_c.flows);
        m_sinks.Create(m_c.flows);
        NodeContainer all(m_routers);
        all.Add(m_sources);
        all.Add(m_sinks);
        InternetStackHelper stack;
        stack.Install(all);
        const int64_t count = stack.AssignStreams(all, m_c.tcpStream);
        NS_ABORT_MSG_IF(
            (m_c.ouStream >= m_c.tcpStream && m_c.ouStream < m_c.tcpStream + count) ||
                (m_c.errorStream < m_c.tcpStream + count && m_c.errorStream + 1 >= m_c.tcpStream),
            "Internet-stack streams overlap the dedicated OU/error streams");
        PointToPointHelper bottleneck;
        bottleneck.SetDeviceAttribute("DataRate", DataRateValue(DataRate(m_c.bottleneckRate)));
        bottleneck.SetDeviceAttribute("Mtu", UintegerValue(m_c.mtu));
        bottleneck.SetChannelAttribute("Delay", TimeValue(m_delay));
        bottleneck.SetQueue("ns3::DropTailQueue<Packet>",
                            "MaxSize",
                            QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, m_c.queuePackets)));
        m_bottleneck = bottleneck.Install(m_routers);
        m_forward = DynamicCast<PointToPointNetDevice>(m_bottleneck.Get(0));
        m_reverse = DynamicCast<PointToPointNetDevice>(m_bottleneck.Get(1));
        PointToPointHelper access;
        access.SetDeviceAttribute("DataRate", DataRateValue(DataRate(m_c.accessRate)));
        access.SetDeviceAttribute("Mtu", UintegerValue(m_c.mtu));
        access.SetChannelAttribute("Delay", TimeValue(Time(m_c.accessDelay)));
        access.SetQueue("ns3::DropTailQueue<Packet>",
                        "MaxSize",
                        QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, m_c.accessQueuePackets)));
        Ipv4AddressHelper address;
        address.SetBase("10.0.0.0", "255.255.255.252");
        address.Assign(m_bottleneck);
        NetDeviceContainer everyDevice(m_bottleneck);
        for (uint32_t i = 0; i < m_c.flows; ++i)
        {
            auto left = access.Install(m_sources.Get(i), m_routers.Get(0));
            everyDevice.Add(left);
            address.NewNetwork();
            address.Assign(left);
            auto right = access.Install(m_routers.Get(1), m_sinks.Get(i));
            everyDevice.Add(right);
            address.NewNetwork();
            m_destinations.push_back(address.Assign(right).GetAddress(1));
        }
        // Address assignment installs default root queue discs; remove these so
        // the stated buffer is the actual DropTail device queue, not an extra layer.
        TrafficControlHelper trafficControl;
        trafficControl.Uninstall(everyDevice);
        for (uint32_t direction = 0; direction < (m_c.reverseErrors ? 2U : 1U); ++direction)
        {
            auto errors = CreateObject<RateErrorModel>();
            errors->SetUnit(RateErrorModel::ERROR_UNIT_PACKET);
            errors->SetRate(m_c.errorRate);
            errors->AssignStreams(m_c.errorStream + direction);
            const auto receiver = direction == 0 ? m_reverse : m_forward;
            receiver->SetAttribute("ReceiveErrorModel", PointerValue(errors));
        }
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
        if (m_c.pcap)
        {
            bottleneck.EnablePcap((m_directory / "bottleneck").string(), m_bottleneck);
        }
    }

    /** @brief Install matching saturated sender/receiver applications. */
    void InstallApplications()
    {
        constexpr uint16_t port = 50000;
        for (uint32_t i = 0; i < m_c.flows; ++i)
        {
            PacketSinkHelper receiver("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), port));
            auto sinks = receiver.Install(m_sinks.Get(i));
            sinks.Start(Seconds(0));
            m_flows[i].sink = DynamicCast<PacketSink>(sinks.Get(0));
            BulkSendHelper sender("ns3::TcpSocketFactory",
                                  InetSocketAddress(m_destinations[i], port));
            sender.SetAttribute("MaxBytes", UintegerValue(0));
            sender.SetAttribute("SendSize", UintegerValue(m_c.mss));
            auto sources = sender.Install(m_sources.Get(i));
            sources.Start(Seconds(m_c.appStart));
            sources.Stop(Seconds(m_c.duration));
            m_flows[i].source = DynamicCast<BulkSendApplication>(sources.Get(0));
            Simulator::Schedule(Seconds(m_c.appStart) + TimeStep(1),
                                &Experiment::ConnectTraces,
                                this,
                                i);
        }
    }

    /**
     * @brief Connect metrics directly to the intended sender socket.
     * @param i Flow index.
     */
    void ConnectTraces(uint32_t i)
    {
        const auto socket = m_flows[i].source->GetSocket();
        NS_ABORT_MSG_IF(!socket, "Sender socket was not created");
        NS_ABORT_MSG_IF(!socket->TraceConnectWithoutContext("CongState", MakeCallback(&Experiment::DiagState, this, i)), "Missing CongState");
        NS_ABORT_MSG_IF(!socket->TraceConnectWithoutContext("Retransmission", MakeCallback(&Experiment::DiagRetrans, this, i)), "Missing Retransmission");
        NS_ABORT_MSG_IF(!socket->TraceConnectWithoutContext("Tx", MakeCallback(&Experiment::DiagTx, this, i)), "Missing Tx");
        const bool cwnd =
            socket->TraceConnectWithoutContext("CongestionWindow",
                                               MakeCallback(&Experiment::Cwnd, this, i));
        const bool inflight =
            socket->TraceConnectWithoutContext("BytesInFlight",
                                               MakeCallback(&Experiment::Inflight, this, i));
        const bool rtt =
            socket->TraceConnectWithoutContext("LastRTT", MakeCallback(&Experiment::Rtt, this, i));
        NS_ABORT_MSG_IF(!cwnd || !inflight || !rtt, "A required sender trace is unavailable");
    }

    void DiagTx(uint32_t i, Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket)
    {
        if (packet->GetSize() > 0) ++m_flows[i].diagTx;
    }
    void DiagRetrans(uint32_t i, Ptr<const Packet> packet, const TcpHeader& header, const Address& local, const Address& peer, Ptr<const TcpSocketBase> socket)
    {
        auto& f = m_flows[i]; ++f.diagRetrans; f.diagRetransBytes += packet->GetSize();
        m_diag << Simulator::Now().GetSeconds() << ',' << i << ",retransmission," << header.GetSequenceNumber().GetValue() << ',' << packet->GetSize() << '\n';
    }
    void DiagState(uint32_t i, TcpSocketState::TcpCongState_t old, TcpSocketState::TcpCongState_t value)
    {
        auto& f = m_flows[i]; const double now = Simulator::Now().GetSeconds();
        if (f.diagState == TcpSocketState::CA_RECOVERY) f.diagRecoveryTime += now - f.diagStateTime;
        if (f.diagState == TcpSocketState::CA_LOSS) f.diagLossTime += now - f.diagStateTime;
        f.diagStateTime = now; f.diagState = value;
        if (value == TcpSocketState::CA_RECOVERY) ++f.diagRecovery;
        if (value == TcpSocketState::CA_LOSS) ++f.diagLoss;
        m_diag << now << ',' << i << ",state," << static_cast<int>(value) << ',' << static_cast<int>(old) << '\n';
    }

    /** @param i Flow index. @param old Previous bytes. @param value New bytes. */
    void Cwnd(uint32_t i, uint32_t old, uint32_t value)
    {
        m_flows[i].cwnd = value;
        Event(i, "cwnd_bytes", value);
    }

    /** @param i Flow index. @param old Previous bytes. @param value New bytes. */
    void Inflight(uint32_t i, uint32_t old, uint32_t value)
    {
        m_flows[i].inflight = value;
        Event(i, "inflight_bytes", value);
    }

    /** @param i Flow index. @param old Previous RTT. @param value New RTT. */
    void Rtt(uint32_t i, Time old, Time value)
    {
        if (value > Time(0) && value != Time::Max())
        {
            m_flows[i].rtt = value.GetSeconds();
            m_flows[i].minRtt = std::min(m_flows[i].minRtt, value.GetSeconds());
            Event(i, "rtt_seconds", value.GetSeconds());
        }
    }

    /** @param i Flow index. @param signal Trace name. @param value Updated value. */
    void Event(uint32_t i, const char* signal, double value)
    {
        if (m_c.tracing)
        {
            m_events << Simulator::Now().GetSeconds() << ',' << i << ',' << signal << ',' << value
                     << '\n';
        }
    }

    /** @brief Snapshot receiver counters at the requested measurement start. */
    void BeginMeasurements()
    {
        for (auto& flow : m_flows)
        {
            flow.baselineBytes = flow.sink->GetTotalRx();
            flow.lastBytes = flow.baselineBytes;
        }
        m_lastSample = Simulator::Now();
        NextSample();
    }

    /** @brief Schedule a metric sample, including a final shorter interval if needed. */
    void NextSample()
    {
        const Time stop = Seconds(m_c.duration);
        if (Simulator::Now() < stop)
        {
            const Time now = Simulator::Now();
            const Time phase = Seconds(m_c.samplingPhaseSeconds);
            const Time period = Seconds(m_c.interval);
            // Anchor samples to the absolute phase, including after a custom warmup.
            const Time advance = now < phase
                                     ? phase - now
                                     : TimeStep(period.GetTimeStep() -
                                                (now - phase).GetTimeStep() % period.GetTimeStep());
            const Time next = std::min(stop, now + advance);
            // Sample after link updates already queued for the same timestamp.
            Simulator::Schedule(next - Simulator::Now(),
                                [this]() { Simulator::ScheduleNow(&Experiment::Sample, this); });
        }
    }

    /** @brief Record receiver goodput, sampled RTT, power and flow fairness. */
    void Sample()
    {
        const double now = Simulator::Now().GetSeconds();
        const double dt = (Simulator::Now() - m_lastSample).GetSeconds();
        double goodput = 0;
        double rttSum = 0;
        uint32_t valid = 0;
        uint64_t inflight = 0;
        uint64_t cwnd = 0;
        std::vector<double> intervalBytes;
        std::vector<double> cumulativeBytes;
        for (uint32_t i = 0; i < m_c.flows; ++i)
        {
            auto& flow = m_flows[i];
            const uint64_t total = flow.sink->GetTotalRx();
            const uint64_t delta = total - flow.lastBytes;
            const double rate = delta * 8.0 / dt;
            intervalBytes.push_back(delta);
            cumulativeBytes.push_back(total - flow.baselineBytes);
            goodput += rate;
            if (std::isfinite(flow.rtt))
            {
                rttSum += flow.rtt;
                ++valid;
            }
            cwnd += flow.cwnd;
            inflight += flow.inflight;
            m_flowSamples << now << ',' << i << ',' << delta << ',' << total - flow.baselineBytes
                          << ',' << rate * 1e-6 << ',' << flow.rtt * 1000 << ','
                          << (std::isfinite(flow.minRtt) ? flow.minRtt * 1000
                                                         : std::numeric_limits<double>::quiet_NaN())
                          << ',' << static_cast<double>(flow.inflight) / m_c.mss << ','
                          << static_cast<double>(flow.cwnd) / m_c.mss << '\n';
            flow.lastBytes = total;
        }
        const double meanRtt =
            valid == m_c.flows ? rttSum / m_c.flows : std::numeric_limits<double>::quiet_NaN();
        const double power = goodput / m_capacity * BaseRtt() / meanRtt;
        m_aggregate << now << ',' << dt << ',' << goodput * 1e-6 << ',' << meanRtt * 1000 << ','
                    << valid << ',' << static_cast<double>(inflight) / m_c.mss << ','
                    << static_cast<double>(cwnd) / m_c.mss << ',' << m_capacity << ','
                    << BaseRtt() * 1000 << ',' << power << ',' << Fairness(intervalBytes) << ','
                    << Fairness(cumulativeBytes) << ',' << m_forward->GetQueue()->GetNPackets()
                    << '\n';
        m_lastSample = Simulator::Now();
        NextSample();
    }

    /** @return Propagation-only RTT baseline for two access links and the bottleneck. */
    double BaseRtt() const
    {
        return 2 * m_delay.GetSeconds() + 4 * Time(m_c.accessDelay).GetSeconds();
    }

    /** @brief Update log-OU state using Appendix C, with a dedicated common RNG stream. */
    void OuUpdate()
    {
        const double dt = Seconds(m_c.ouInterval).GetSeconds();
        m_logCapacity += (m_logMean - m_logCapacity) / m_c.ouTau * dt +
                         m_c.ouSigma * std::sqrt(dt) * m_normal->GetValue();
        const double low = std::max(1000.0, m_c.ouLower * m_nominal);
        const double high = m_c.ouUpper * m_nominal;
        // Clip in log space before exponentiation to avoid an invalid integer conversion.
        if (!std::isfinite(m_logCapacity))
        {
            throw std::runtime_error("OU state became non-finite; check volatility and time step");
        }
        const uint64_t bps =
            std::clamp(static_cast<uint64_t>(
                           std::exp(std::clamp(m_logCapacity, std::log(low), std::log(high)))),
                       static_cast<uint64_t>(std::ceil(low)),
                       static_cast<uint64_t>(std::floor(high)));
        IntegrateCapacity();
        m_capacity = bps;
        m_forward->SetDataRate(DataRate(bps));
        if (m_c.bidirectionalOu)
        {
            m_reverse->SetDataRate(DataRate(bps));
        }
        m_capacityTrace << Simulator::Now().GetSeconds() << ',' << bps << ','
                        << (m_c.bidirectionalOu ? bps : static_cast<uint64_t>(m_nominal)) << '\n';
        if (Simulator::Now() + Seconds(m_c.ouInterval) < Seconds(m_c.duration))
        {
            Simulator::Schedule(Seconds(m_c.ouInterval), &Experiment::OuUpdate, this);
        }
    }

    /** @brief Accumulate time-weighted realised capacity moments before a rate change. */
    void IntegrateCapacity()
    {
        const double now = std::min(Simulator::Now().GetSeconds(), m_c.duration);
        const double dt = now - m_lastCapacityTime;
        m_capacityIntegral += m_capacity * dt;
        m_capacitySquaredIntegral += m_capacity * m_capacity * dt;
        m_lastCapacityTime = now;
    }

    /** @param delay New one-way bottleneck delay for the extension scenario. */
    void ChangeDelay(Time delay)
    {
        auto channel = DynamicCast<PointToPointChannel>(m_forward->GetChannel());
        channel->SetAttribute("Delay", TimeValue(delay));
        m_delay = delay;
        m_delayTrace << Simulator::Now().GetSeconds() << ',' << delay.GetSeconds() << ','
                     << BaseRtt() << '\n';
    }

    /** @brief Print optional progress without changing metric collection cadence. */
    void Progress()
    {
        std::cout << "time=" << Simulator::Now().GetSeconds() << "s capacity=" << m_capacity * 1e-6
                  << "Mbps\n";
        if (Simulator::Now() + Seconds(1) < Seconds(m_c.duration))
        {
            Simulator::Schedule(Seconds(1), &Experiment::Progress, this);
        }
    }

    /** @brief Write flow-level time averages and measured capacity variability. */
    void Summarize()
    {
        std::ofstream diag; Open(diag, "recovery-summary.csv");
        diag << "flow,tx_packets,retrans_packets,retrans_bytes,recovery_entries,loss_entries,recovery_seconds,loss_seconds\n";
        for (uint32_t i=0; i<m_c.flows; ++i) {
            auto& f=m_flows[i]; const double dt=m_c.duration-f.diagStateTime;
            if(f.diagState==TcpSocketState::CA_RECOVERY) f.diagRecoveryTime+=dt;
            if(f.diagState==TcpSocketState::CA_LOSS) f.diagLossTime+=dt;
            diag << i << ',' << f.diagTx << ',' << f.diagRetrans << ',' << f.diagRetransBytes << ',' << f.diagRecovery << ',' << f.diagLoss << ',' << f.diagRecoveryTime << ',' << f.diagLossTime << '\n';
        }
        std::ofstream summary;
        Open(summary, "summary.csv");
        summary << "flow,delivered_payload_bytes,mean_goodput_mbps\n";
        std::vector<double> delivered;
        const double elapsed = (Seconds(m_c.duration) - Seconds(m_c.warmup)).GetSeconds();
        for (uint32_t i = 0; i < m_c.flows; ++i)
        {
            const uint64_t bytes = m_flows[i].lastBytes - m_flows[i].baselineBytes;
            delivered.push_back(bytes);
            summary << i << ',' << bytes << ',' << bytes * 8e-6 / elapsed << '\n';
        }
        const double total = std::accumulate(delivered.begin(), delivered.end(), 0.0);
        IntegrateCapacity();
        const double mean = m_capacityIntegral / m_c.duration;
        const double scv =
            std::max(0.0, m_capacitySquaredIntegral / m_c.duration - mean * mean) / (mean * mean);
        std::ofstream totals;
        Open(totals, "totals.txt");
        totals << "measurementSeconds=" << elapsed << "\nreceivedPayloadBytes=" << total
               << "\naggregateMeanGoodputMbps=" << total * 8e-6 / elapsed
               << "\njainMeanGoodput=" << Fairness(delivered)
               << "\nrealisedCapacityMeanBps=" << mean << "\nrealisedCapacityScv=" << scv << '\n';
        std::cout << "Delivered " << total
                  << " payload bytes; aggregate mean goodput=" << total * 8e-6 / elapsed
                  << " Mbps; Jain fairness=" << Fairness(delivered) << '\n';
    }

    /** @param stream Destination stream. @param name Output filename. */
    void Open(std::ofstream& stream, const std::string& name)
    {
        stream.open(m_directory / name);
        if (!stream)
        {
            throw std::runtime_error("Could not open output file " + name);
        }
        stream << std::setprecision(17);
    }

    const Settings& m_c; ///< Effective configuration, valid for the experiment lifetime.
    std::filesystem::path m_directory;       ///< Unique output directory.
    NodeContainer m_routers;                 ///< Shared bottleneck endpoints.
    NodeContainer m_sources;                 ///< Sending leaves.
    NodeContainer m_sinks;                   ///< Receiving leaves.
    NetDeviceContainer m_bottleneck;         ///< Forward and reverse bottleneck devices.
    Ptr<PointToPointNetDevice> m_forward;    ///< Forward data transmitter.
    Ptr<PointToPointNetDevice> m_reverse;    ///< Reverse ACK transmitter.
    std::vector<Ipv4Address> m_destinations; ///< Per-flow destination addresses.
    std::vector<Flow> m_flows;               ///< Sampled socket state and receiver counters.
    std::ofstream m_aggregate;               ///< Aggregate evaluation samples.
    std::ofstream m_flowSamples;             ///< Per-flow evaluation samples.
    std::ofstream m_capacityTrace;           ///< Exact realised service process.
    std::ofstream m_delayTrace;              ///< Actual propagation-delay schedule.
    std::ofstream m_diag;
    std::ofstream m_events;                  ///< Optional high-resolution socket events.
    Time m_lastSample;                       ///< Preceding measurement timestamp.
    double m_nominal;                        ///< Nominal service rate in bits/s.
    double m_capacity;                       ///< Current physical forward service rate in bits/s.
    Time m_delay;                            ///< Current one-way bottleneck delay.
    Ptr<NormalRandomVariable> m_normal;      ///< Dedicated standard-normal OU innovations.
    double m_logMean{0};                     ///< Lognormal mean correction, Appendix C20.
    double m_logCapacity{0};                 ///< Unclipped OU latent log-capacity state.
    double m_lastCapacityTime{0};            ///< Capacity integration timestamp.
    double m_capacityIntegral{0};            ///< Time integral of realised capacity.
    double m_capacitySquaredIntegral{0};     ///< Time integral of squared realised capacity.
};
} // namespace

int
main(int argc, char* argv[])
{
    try
    {
        Settings settings;
        ParseSettings(settings, argc, argv);
        Validate(settings);
        const std::string description = Describe(settings);
        const auto directory = std::filesystem::path(settings.outputRoot) / settings.scenario /
                               settings.transport /
                               ("seed" + std::to_string(settings.seed) + "-run" +
                                std::to_string(settings.run) + "-" + ConfigId(description));
        std::cout << description << "outputDirectory=" << directory.string() << '\n';
        if (settings.dryRun)
        {
            return 0;
        }
        if (std::filesystem::exists(directory) && !settings.overwrite)
        {
            throw std::runtime_error(
                "Output directory already exists; choose --rngRun, --sim_name, or --overwrite=1");
        }
        std::filesystem::create_directories(directory);
        std::ofstream manifest(directory / "resolved-config.txt");
        if (!manifest)
        {
            throw std::runtime_error("Could not write resolved-config.txt");
        }
        manifest << description;
        manifest.close();
        RngSeedManager::SetSeed(settings.seed);
        RngSeedManager::SetRun(settings.run);
        Experiment experiment(settings, directory);
        experiment.Run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "tcp_qt_sims: " << error.what() << '\n';
        Simulator::Destroy();
        return 1;
    }
}
