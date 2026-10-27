#include "tcp_qt_examples.h"

using namespace ns3;
using namespace ns3::tcp_qt_examples;

namespace
{
struct StochasticServiceConfig
{
    bool enable = true;

    // The OU process is centred on config.bottleneckRate.  The value below is
    // only an optional hard upper cap; use 0bps/none to leave the cap disabled.
    std::string upperRate = "0bps";

    double startTime = 0.0;
    double updateInterval = 0.001;
    double tau = 0.5;
    double sigmaLog = 0.62;

    uint32_t rngSeed = 12345;
    uint64_t rngRun = 1;
};

std::string
FormatDoubleForPath(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << value;
    std::string text = stream.str();

    while (!text.empty() && text.back() == '0')
    {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.')
    {
        text.pop_back();
    }
    std::replace(text.begin(), text.end(), '.', 'p');
    return text.empty() ? "0" : text;
}

bool
IsDisabledRateCap(const std::string& rate)
{
    return rate.empty() || rate == "0" || rate == "0bps" || rate == "none" || rate == "None" ||
           rate == "NONE";
}

uint64_t
ParseRateCapBps(const std::string& rate)
{
    if (IsDisabledRateCap(rate))
    {
        return 0;
    }
    return DataRate(rate).GetBitRate();
}

std::string
BuildStochasticSuffix(const StochasticServiceConfig& stochastic)
{
    if (!stochastic.enable)
    {
        return "staticBW";
    }

    std::ostringstream suffix;
    suffix << "stochBW"
           << "_dt" << FormatDoubleForPath(stochastic.updateInterval)
           << "_tau" << FormatDoubleForPath(stochastic.tau)
           << "_sig" << FormatDoubleForPath(stochastic.sigmaLog);

    if (!IsDisabledRateCap(stochastic.upperRate))
    {
        suffix << "_cap" << stochastic.upperRate;
    }
    suffix << "_run" << stochastic.rngRun;

    return suffix.str();
}

void
AppendStochasticOutputPath(OutputPaths& paths, const StochasticServiceConfig& stochastic)
{
    const std::string suffix = BuildStochasticSuffix(stochastic);
    paths.prefix += "-" + suffix;
    paths.directory += suffix + "/";
    std::filesystem::create_directories(paths.directory);
}

bool
ValidateConfig(const SimulationConfig& config, const StochasticServiceConfig& stochastic)
{
    if (config.numFlows == 0)
    {
        std::cerr << "num_flows must be at least 1" << std::endl;
        return false;
    }
    if (config.segmentSize == 0)
    {
        std::cerr << "segmentSize must be greater than zero" << std::endl;
        return false;
    }
    if (config.simDuration <= 0.0)
    {
        std::cerr << "sim_duration must be positive" << std::endl;
        return false;
    }
    if (stochastic.enable)
    {
        if (stochastic.updateInterval <= 0.0)
        {
            std::cerr << "stoch_interval must be positive" << std::endl;
            return false;
        }
        if (stochastic.tau <= 0.0)
        {
            std::cerr << "stoch_tau must be positive" << std::endl;
            return false;
        }
        if (stochastic.sigmaLog < 0.0)
        {
            std::cerr << "stoch_sigmaLog cannot be negative" << std::endl;
            return false;
        }
        if (stochastic.startTime < 0.0 || stochastic.startTime >= config.simDuration)
        {
            std::cerr << "stoch_start must satisfy 0 <= stoch_start < sim_duration" << std::endl;
            return false;
        }
    }
    return true;
}

void
PrintStochasticSummary(const SimulationConfig& config,
                       const StochasticServiceConfig& stochastic,
                       uint64_t upperRateBps)
{
    if (!stochastic.enable)
    {
        std::cout << "  service model     : static bottleneck rate" << std::endl;
        return;
    }

    std::cout << "  service model     : stochastic log-OU bottleneck rate" << std::endl
              << "  mean service rate : " << config.bottleneckRate << std::endl
              << "  stoch_start      : " << stochastic.startTime << " s" << std::endl
              << "  stoch_interval   : " << stochastic.updateInterval << " s" << std::endl
              << "  stoch_tau        : " << stochastic.tau << " s" << std::endl
              << "  stoch_sigmaLog   : " << stochastic.sigmaLog << std::endl
              << "  rngSeed/rngRun   : " << stochastic.rngSeed << "/" << stochastic.rngRun << std::endl;

    if (upperRateBps > 0)
    {
        std::cout << "  stoch_upperBW    : " << upperRateBps * 1e-6 << " Mbps" << std::endl;
    }
    else
    {
        std::cout << "  stoch_upperBW    : disabled" << std::endl;
    }
}
} // namespace

int
main(int argc, char* argv[])
{
    SimulationConfig config;
    StochasticServiceConfig stochastic;

    CommandLine cmd(__FILE__);
    cmd.AddValue("transport_prot",
                 "Transport protocol: TcpNewReno, TcpCubic, TcpBbr, TcpBbrV2, TcpQtSdr, TcpQtTor, etc.",
                 config.transport);
    cmd.AddValue("error_p", "Packet error rate", config.errorRate);
    cmd.AddValue("btlBW", "Mean bottleneck service rate", config.bottleneckRate);
    cmd.AddValue("btlDelay", "Bottleneck one-way delay", config.bottleneckDelay);
    cmd.AddValue("accessBW", "Access link bandwidth", config.accessRate);
    cmd.AddValue("accessDelay", "Access link one-way delay", config.accessDelay);
    cmd.AddValue("sim_name", "Top-level output directory", config.simName);
    cmd.AddValue("queue_disc_type", "Queue disc type", config.queueDiscType);
    cmd.AddValue("queueSize_bytes", "Bottleneck queue size in bytes", config.queueSizeBytes);
    cmd.AddValue("num_flows", "Number of long-lived TCP flows", config.numFlows);
    cmd.AddValue("sim_duration", "Simulation duration in seconds", config.simDuration);
    cmd.AddValue("targetRttAlpha", "Target RTT multiplier for TcpQt* controllers", config.targetRttAlpha);
    cmd.AddValue("minRttEpsilon", "Minimum RTT tolerance/epsilon for TcpQtSdr", config.minRttEpsilon);
    cmd.AddValue("data_logging_interval", "Aggregate logging interval in seconds", config.dataLoggingInterval);
    cmd.AddValue("tracing", "Enable per-flow TCP socket traces", config.enableTraces);
    cmd.AddValue("logging", "Enable aggregate metric loggers", config.enableLoggers);
    cmd.AddValue("progress", "Print one-line progress output every second", config.enableProgress);

    cmd.AddValue("stoch_enable", "Enable stochastic bottleneck service-rate process", stochastic.enable);
    cmd.AddValue("stoch_start", "Time at which stochastic service-rate updates start", stochastic.startTime);
    cmd.AddValue("stoch_interval", "Interval between stochastic service-rate updates, in seconds", stochastic.updateInterval);
    cmd.AddValue("stoch_tau", "OU mean-reversion time constant, in seconds", stochastic.tau);
    cmd.AddValue("stoch_sigmaLog", "Diffusion scale of the log-rate OU process", stochastic.sigmaLog);
    cmd.AddValue("stoch_upperBW", "Optional upper service-rate cap, e.g., 175Mbps; use 0bps or none to disable", stochastic.upperRate);
    cmd.AddValue("rngSeed", "Global ns-3 RNG seed", stochastic.rngSeed);
    cmd.AddValue("rngRun", "Global ns-3 RNG run number", stochastic.rngRun);
    cmd.Parse(argc, argv);

    if (!ValidateConfig(config, stochastic))
    {
        return 1;
    }

    RngSeedManager::SetSeed(stochastic.rngSeed);
    RngSeedManager::SetRun(stochastic.rngRun);

    ConfigureTcpDefaults(config);

    const double startTime = 0.0;
    const double stopTime = startTime + config.simDuration;
    const uint32_t queueSizePackets = std::max<uint32_t>(1, config.queueSizeBytes / config.segmentSize);
    const uint64_t upperRateBps = ParseRateCapBps(stochastic.upperRate);

    OutputPaths outputPaths = BuildOutputPaths(config, queueSizePackets);
    AppendStochasticOutputPath(outputPaths, stochastic);

    PrintSimulationSummary(config, queueSizePackets, outputPaths);
    PrintStochasticSummary(config, stochastic, upperRateBps);

    dumbbell topology(config.numFlows,
                      config.transport,
                      config.bottleneckRate,
                      config.bottleneckDelay,
                      config.accessRate,
                      config.accessDelay,
                      config.queueDiscType,
                      queueSizePackets,
                      config.errorRate,
                      startTime,
                      stopTime);

    TraceState traceState(config);
    traceState.SetOutputPaths(outputPaths);
    traceState.EnablePerFlowTraces(topology);
    traceState.EnableAggregateLoggers();
    traceState.EnableProgressPrinter();

    if (stochastic.enable)
    {
        Simulator::Schedule(Seconds(stochastic.startTime),
                            &TraceState::ScheduleStochasticBottleneckRate,
                            &traceState,
                            topology,
                            upperRateBps,
                            Seconds(stochastic.updateInterval),
                            stochastic.tau,
                            stochastic.sigmaLog);
    }

    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
