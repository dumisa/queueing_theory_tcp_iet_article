#include "tcp_qt_examples.h"

using namespace ns3;
using namespace ns3::tcp_qt_examples;

int
main(int argc, char* argv[])
{
    SimulationConfig config;

    // Default experiment: start at 20 ms, change to 40 ms at 25 s,
    // then return to 20 ms at 45 s.
    double delayChangeTime1 = 15.0;
    double delayChangeTime2 = 25.0;
    double delayChangeTime3 = 35.0;
    std::string bottleneckDelay1 = "20ms";
    std::string bottleneckDelay2 = "40ms";
    std::string bottleneckDelay3 = "10ms";

    // The initial bottleneck delay is carried by config.bottleneckDelay.
    config.bottleneckDelay = "20ms";

    CommandLine cmd(__FILE__);
    cmd.AddValue("transport_prot",
                 "Transport protocol: TcpNewReno, TcpCubic, TcpBbr, TcpBbrV2, TcpQtSdr, TcpQtTor, etc.",
                 config.transport);
    cmd.AddValue("error_p", "Packet error rate", config.errorRate);
    cmd.AddValue("btlBW", "Bottleneck link bandwidth", config.bottleneckRate);
    cmd.AddValue("btlDelay", "Initial bottleneck one-way delay", config.bottleneckDelay);
    cmd.AddValue("btlDelay1", "Bottleneck one-way delay after delayChangeTime1", bottleneckDelay1);
    cmd.AddValue("btlDelay2", "Bottleneck one-way delay after delayChangeTime2", bottleneckDelay2);
    cmd.AddValue("delayChangeTime1", "Time in seconds to apply btlDelay1", delayChangeTime1);
    cmd.AddValue("delayChangeTime2", "Time in seconds to apply btlDelay2", delayChangeTime2);
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

    std::cout << "  delay schedule    : "
              << config.bottleneckDelay << " at 0 s, "
              << bottleneckDelay1 << " at " << delayChangeTime1 << " s, "
              << bottleneckDelay2 << " at " << delayChangeTime2 << " s"
              << std::endl;

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

    if (delayChangeTime1 >= startTime && delayChangeTime1 <= stopTime)
    {
        Simulator::Schedule(Seconds(delayChangeTime1),
                            &TraceState::ChangePropagationDelay,
                            &traceState,
                            topology,
                            Time(bottleneckDelay1));
    }

    if (delayChangeTime2 >= startTime && delayChangeTime2 <= stopTime)
    {
        Simulator::Schedule(Seconds(delayChangeTime2),
                            &TraceState::ChangePropagationDelay,
                            &traceState,
                            topology,
                            Time(bottleneckDelay2));
    }

    if (delayChangeTime3 >= startTime && delayChangeTime3 <= stopTime)
    {
        Simulator::Schedule(Seconds(delayChangeTime3),
                            &TraceState::ChangePropagationDelay,
                            &traceState,
                            topology,
                            Time(bottleneckDelay3));
    }

    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
