#include "tcp_qt_examples.h"

using namespace ns3;
using namespace ns3::tcp_qt_examples;

int
main(int argc, char* argv[])
{
    SimulationConfig config;

    CommandLine cmd(__FILE__);
    cmd.AddValue("transport_prot",
                 "Transport protocol: TcpNewReno, TcpCubic, TcpBbr, TcpBbrV2, TcpQtSdr, TcpQtTor, etc.",
                 config.transport);
    cmd.AddValue("error_p", "Packet error rate", config.errorRate);
    cmd.AddValue("btlBW", "Bottleneck link bandwidth", config.bottleneckRate);
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
    cmd.Parse(argc, argv);

    ConfigureTcpDefaults(config);

    const double startTime = 0.0;
    const double stopTime = startTime + config.simDuration;
    const uint32_t queueSizePackets = std::max<uint32_t>(1, config.queueSizeBytes / config.segmentSize);

    OutputPaths outputPaths = BuildOutputPaths(config, queueSizePackets);
    PrintSimulationSummary(config, queueSizePackets, outputPaths);

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

    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
