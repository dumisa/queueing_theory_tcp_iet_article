/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * \file my-dumbbell.cc
 * \brief Implementation of dumbbell topology setup for NS-3
 */

#include "my-dumbbell.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-socket-factory.h"

#include <iostream>
#include <string>
#include <sstream>

namespace ns3 {

// -- Constructor & Destructor -------------------------------------------------

dumbbell::dumbbell (uint8_t flows, std::string tcp_type,
                    std::string btlBW, std::string btlDelay,
                    std::string accessBW, std::string accessDelay,
                    std::string queueDisc, uint32_t queueDiscSize,
                    double error_p, double sim_start, double sim_stop)
{
  tcp_type = "ns3::" + tcp_type;
  queueDisc = "ns3::" + queueDisc;

  //SetErrorModel (&m_error_model, error_p);
  //m_bottleNeckLink = CreateP2PLink (btlBW, btlDelay, &m_error_model);
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
  uv->SetStream(50);
  m_error_model.SetRandomVariable(uv);
  m_error_model.SetUnit(RateErrorModel::ERROR_UNIT_PACKET);
  m_error_model.SetRate(error_p);
  std::cout << "m_error_model rate = " << m_error_model.GetRate() << std::endl; 

  //ns3::PointToPointHelper bottleNeckLink;
  m_bottleNeckLink.SetDeviceAttribute("DataRate", ns3::StringValue(btlBW));
  m_bottleNeckLink.SetChannelAttribute("Delay", ns3::StringValue(btlDelay));
  m_bottleNeckLink.SetDeviceAttribute("ReceiveErrorModel", ns3::PointerValue(&m_error_model));

  //PointToPointHelper accessLink = CreateP2PLink (accessBW, accessDelay);
    ns3::PointToPointHelper accessLink; //left and right links
    accessLink.SetDeviceAttribute("DataRate",
            ns3::StringValue(accessBW));
    accessLink.SetChannelAttribute("Delay",
            ns3::StringValue(accessDelay));

 std::cout << "tcp_type:" << tcp_type << "\n";

  m_routers.Create (2);
  m_leftLeaf.Create (flows);
  m_rightLeaf.Create (flows);
  std::cout << "Nodes created" << std::endl; 
  
  m_routerDevices = m_bottleNeckLink.Install (m_routers);

  //TypeId tcpTid = TypeId::LookupByName (tcp_type);
  std::cout << "tcp_type:" << tcp_type << "\n";

  for (uint32_t i = 0; i < flows; ++i)
    {
      NetDeviceContainer leftDev = accessLink.Install (m_routers.Get (0), m_leftLeaf.Get (i));
      m_leftRouterDevices.Add (leftDev.Get (0));
      m_leftLeafDevices.Add (leftDev.Get (1));
      
      NetDeviceContainer rightDev = accessLink.Install (m_routers.Get (1), m_rightLeaf.Get (i));
      m_rightRouterDevices.Add (rightDev.Get (0));
      m_rightLeafDevices.Add (rightDev.Get (1));
      std::cout << "i:" << i << " " << m_leftLeaf.Get (i)->GetId () << "\n";
      std::cout << "i:" << i << " " << m_rightLeaf.Get (i)->GetId () << "\n";
    }

  std::cout << "Senders connected" << std::endl; 
  std::cout << "Receivers connected" << std::endl;

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcp_type));
  InternetStackHelper stack;
  //InstallStack (stack);
  stack.Install (m_routers);
  stack.Install (m_leftLeaf);
  stack.Install (m_rightLeaf);
  std::cout << "done - InstallStack (stack)" <<  "\n";
  //ConfigureTrafficControl (m_btl_queueDiscs, m_routerDevices, queueDisc, queueDiscSize);
    // Bottleneck link traffic control configuration
    TrafficControlHelper tchPfifo_btl;
    tchPfifo_btl.SetRootQueueDisc (queueDisc, "MaxSize",
                                 StringValue (std::to_string(queueDiscSize) + "p"));
    if (queueDiscSize > 0) 
    {
        m_btl_queueDiscs = tchPfifo_btl.Install (m_routerDevices);
    }

    // assign ipv4 addresses
    ns3::Ipv4AddressHelper routerIp =
        ns3::Ipv4AddressHelper("10.3.0.0", "255.255.255.0");
    ns3::Ipv4AddressHelper leftIp =
        ns3::Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
    ns3::Ipv4AddressHelper rightIp =
        ns3::Ipv4AddressHelper("10.2.0.0", "255.255.255.0");
    AssignIpv4Addresses (leftIp, rightIp, routerIp);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    std::cout << "RoutingTables done " << std::endl;

    uint16_t port = 50000;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);

    m_sender_sockets.reserve (LeftCount ());

    std::cout << "sender sockets created: " << LeftCount () << std::endl;

    for (uint16_t i = 0; i < flows; i++)
    {
        AddressValue remoteAddress (InetSocketAddress (m_rightLeafInterfaces.GetAddress (i, 0), port));
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
        BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());

        ftp.SetAttribute ("Remote", remoteAddress);
        ftp.SetAttribute ("SendSize", UintegerValue (1448));
        ftp.SetAttribute ("MaxBytes", UintegerValue (0));

        ApplicationContainer sourceApp = ftp.Install (m_leftLeaf.Get (i));
        sourceApp.Start (Seconds (sim_start + 0.00001));
        sourceApp.Stop (Seconds (sim_stop - 3));

        sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
        ApplicationContainer sinkApp = sinkHelper.Install (m_rightLeaf.Get (i));
        sinkApp.Start (Seconds (sim_start + 0.00001));
        sinkApp.Stop (Seconds (sim_stop));
    }

  std::cout << "dumbbell topology configured " << std::endl;

  std::cout << "[Constructor] Single-TCP-type dumbbell topology configured with " << unsigned(flows) << " flows." << std::endl;
}

dumbbell::dumbbell (std::string tcp_type1, std::string tcp_type2,
                    uint8_t type1_flows, uint8_t type2_flows,
                    std::string btlBW, std::string btlDelay,
                    std::string accessBW, std::string accessDelay1, std::string accessDelay2,
                    std::string queueDisc, uint32_t queueDiscSize,
                    double error_p, double sim_start1, double sim_stop1,
                    double sim_start2, double sim_stop2)
{

  tcp_type1 = "ns3::" + tcp_type1;
  tcp_type2 = "ns3::" + tcp_type2;
  queueDisc = "ns3::" + queueDisc;
  uint32_t totalFlows = type1_flows + type2_flows;

  //SetErrorModel (&m_error_model, error_p);
  //m_bottleNeckLink = CreateP2PLink (btlBW, btlDelay, &m_error_model);
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
  uv->SetStream(50);
  m_error_model.SetRandomVariable(uv);
  m_error_model.SetUnit(RateErrorModel::ERROR_UNIT_PACKET);
  m_error_model.SetRate(error_p);
  std::cout << "m_error_model rate = " << m_error_model.GetRate() << std::endl; 

  //ns3::PointToPointHelper bottleNeckLink;
  m_bottleNeckLink.SetDeviceAttribute("DataRate", ns3::StringValue(btlBW));
  m_bottleNeckLink.SetChannelAttribute("Delay", ns3::StringValue(btlDelay));
  m_bottleNeckLink.SetDeviceAttribute("ReceiveErrorModel", ns3::PointerValue(&m_error_model));

  //PointToPointHelper accessLink = CreateP2PLink (accessBW, accessDelay);
  ns3::PointToPointHelper accessLink1; //left and right links
  accessLink1.SetDeviceAttribute("DataRate",
            ns3::StringValue(accessBW));
  accessLink1.SetChannelAttribute("Delay",
            ns3::StringValue(accessDelay1));
  ns3::PointToPointHelper accessLink2; //left and right links
  accessLink2.SetDeviceAttribute("DataRate",
            ns3::StringValue(accessBW));
  accessLink2.SetChannelAttribute("Delay",
            ns3::StringValue(accessDelay2));

 std::cout << "tcp_type1:" << tcp_type1 << " tcp_type2:" << tcp_type2 << "\n";

  //PointToPointHelper accessLink1 = CreateP2PLink (accessBW, accessDelay1);
  //PointToPointHelper accessLink2 = CreateP2PLink (accessBW, accessDelay2);

  m_routers.Create (2);
  m_leftLeaf.Create (totalFlows);
  m_rightLeaf.Create (totalFlows);
  m_routerDevices = m_bottleNeckLink.Install (m_routers);

  for (uint32_t i = 0; i < type1_flows; ++i)
    {
      NetDeviceContainer leftDev = accessLink1.Install (m_routers.Get (0), m_leftLeaf.Get (i));
      m_leftRouterDevices.Add (leftDev.Get (0));
      m_leftLeafDevices.Add (leftDev.Get (1));
      
      NetDeviceContainer rightDev = accessLink1.Install (m_routers.Get (1), m_rightLeaf.Get (i));
      m_rightRouterDevices.Add (rightDev.Get (0));
      m_rightLeafDevices.Add (rightDev.Get (1));
      std::cout << "i:" << i << "\n";
    }
  for (uint32_t i = type1_flows; i < totalFlows; ++i)
    {
      NetDeviceContainer leftDev = accessLink2.Install (m_routers.Get (0), m_leftLeaf.Get (i));
      m_leftRouterDevices.Add (leftDev.Get (0));
      m_leftLeafDevices.Add (leftDev.Get (1));
      
      NetDeviceContainer rightDev = accessLink2.Install (m_routers.Get (1), m_rightLeaf.Get (i));
      m_rightRouterDevices.Add (rightDev.Get (0));
      m_rightLeafDevices.Add (rightDev.Get (1));
      std::cout << "i:" << i << "\n";
    }

  //TypeId tcpTid1 = TypeId::LookupByName (tcp_type1);
  //TypeId tcpTid2 = TypeId::LookupByName (tcp_type2);

  std::cout << "Senders connected" << std::endl; 
  std::cout << "Receivers connected" << std::endl;

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcp_type1));
  InternetStackHelper stack;
  InstallStack (stack);

  for (uint32_t i = type1_flows; i < totalFlows; ++i)
    {
      Ptr<Node> left_node = m_leftLeaf.Get (i);
      Ptr<Node> right_node = m_rightLeaf.Get (i);
      Ptr<TcpL4Protocol> left_tcp = left_node->GetObject<TcpL4Protocol>();
      Ptr<TcpL4Protocol> right_tcp = right_node->GetObject<TcpL4Protocol>();

      left_tcp->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName (tcp_type2)));
      right_tcp->SetAttribute ("SocketType", TypeIdValue (TypeId::LookupByName (tcp_type2)));
    }

  // Bottleneck link traffic control configuration
  TrafficControlHelper tchPfifo_btl;
  tchPfifo_btl.SetRootQueueDisc (queueDisc, "MaxSize",
                                 StringValue (std::to_string(queueDiscSize) + "p"));
  if (queueDiscSize > 0) 
  {
    m_btl_queueDiscs = tchPfifo_btl.Install (m_routerDevices);
  }

    // assign ipv4 addresses
    ns3::Ipv4AddressHelper routerIp =
        ns3::Ipv4AddressHelper("10.3.0.0", "255.255.255.0");
    ns3::Ipv4AddressHelper leftIp =
        ns3::Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
    ns3::Ipv4AddressHelper rightIp =
        ns3::Ipv4AddressHelper("10.2.0.0", "255.255.255.0");
    AssignIpv4Addresses (leftIp, rightIp, routerIp);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    std::cout << "RoutingTables done " << std::endl;

    uint16_t port = 50000;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);

    m_sender_sockets.reserve (LeftCount ());

    std::cout << "sender sockets created " << std::endl;

    for (uint16_t i = 0; i < type1_flows; i++)
    {
        AddressValue remoteAddress (InetSocketAddress (m_rightLeafInterfaces.GetAddress (i, 0), port));
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
        BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());

        ftp.SetAttribute ("Remote", remoteAddress);
        ftp.SetAttribute ("SendSize", UintegerValue (1448));
        ftp.SetAttribute ("MaxBytes", UintegerValue (0));

        ApplicationContainer sourceApp = ftp.Install (m_leftLeaf.Get (i));
        sourceApp.Start (Seconds (sim_start1));
        sourceApp.Stop (Seconds (sim_stop1 - 1));

        sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
        ApplicationContainer sinkApp = sinkHelper.Install (m_rightLeaf.Get (i));
        sinkApp.Start (Seconds (sim_start1+0.0001));
        sinkApp.Stop (Seconds (sim_stop1));
    }
    for (uint16_t i = type1_flows; i < totalFlows; i++)
    {
        AddressValue remoteAddress (InetSocketAddress (m_rightLeafInterfaces.GetAddress (i, 0), port));
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
        BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());

        ftp.SetAttribute ("Remote", remoteAddress);
        ftp.SetAttribute ("SendSize", UintegerValue (1448));
        ftp.SetAttribute ("MaxBytes", UintegerValue (0));

        ApplicationContainer sourceApp = ftp.Install (m_leftLeaf.Get (i));
        sourceApp.Start (Seconds (sim_start2));
        sourceApp.Stop (Seconds (sim_stop2 - 1));

        sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
        ApplicationContainer sinkApp = sinkHelper.Install (m_rightLeaf.Get (i));
        sinkApp.Start (Seconds (sim_start2 + 0.001));
        sinkApp.Stop (Seconds (sim_stop2));
    }

  std::cout << "[Constructor] Dual-TCP-type dumbbell topology configured with " << totalFlows << " flows." << std::endl;
}

dumbbell::dumbbell () {}
dumbbell::~dumbbell () {}

// Constructor with per-flow app start/stop times
dumbbell::dumbbell (uint8_t flows, std::string btlBW, std::string btlDelay,
                    std::string accessBW, std::string accessDelay,
                    std::string queueDisc, uint32_t queueDiscSize,
                    double error_p, std::vector<double> app_start, std::vector<double> app_stop)
{
  NS_ASSERT (app_start.size () == flows && app_stop.size () == flows);

  SetErrorModel (&m_error_model, error_p);
  m_bottleNeckLink = CreateP2PLink (btlBW, btlDelay, &m_error_model);
  PointToPointHelper accessLink = CreateP2PLink (accessBW, accessDelay);

  m_routers.Create (2);
  m_leftLeaf.Create (flows);
  m_rightLeaf.Create (flows);

  m_routerDevices = m_bottleNeckLink.Install (m_routers);

  for (uint32_t i = 0; i < flows; ++i)
    {
      NetDeviceContainer leftDevices = accessLink.Install (m_routers.Get (0), m_leftLeaf.Get (i));
      m_leftRouterDevices.Add (leftDevices.Get (0));
      m_leftLeafDevices.Add (leftDevices.Get (1));

      NetDeviceContainer rightDevices = accessLink.Install (m_routers.Get (1), m_rightLeaf.Get (i));
      m_rightRouterDevices.Add (rightDevices.Get (0));
      m_rightLeafDevices.Add (rightDevices.Get (1));
    }

  InternetStackHelper stack;
  InstallStack (stack);

  ConfigureTrafficControl (m_btl_queueDiscs, m_routerDevices, queueDisc, queueDiscSize);

  Ipv4AddressHelper leftIp  ("10.1.0.0", "255.255.255.0");
  Ipv4AddressHelper rightIp ("10.2.0.0", "255.255.255.0");
  Ipv4AddressHelper routerIp("10.3.0.0", "255.255.255.0");
  AssignIpv4Addresses (leftIp, rightIp, routerIp);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  for (uint32_t i = 0; i < flows; ++i)
    {
      SetupApplicationPair (m_leftLeaf.Get (i), m_rightLeaf.Get (i),
                            m_rightLeafInterfaces.GetAddress (i),
                            app_start[i], app_stop[i]);
    }

  std::cout << "[Constructor] Per-flow timing dumbbell topology configured with " << unsigned(flows) << " flows." << std::endl;
}

// -- Helper Functions ---------------------------------------------------------

void dumbbell::SetErrorModel (Ptr<RateErrorModel> model, double error_p) {
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (50);
  model->SetRandomVariable (uv);
  model->SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
  model->SetRate (error_p);
}

PointToPointHelper dumbbell::CreateP2PLink (std::string bw, std::string delay, Ptr<RateErrorModel> errorModel) {
  PointToPointHelper link;
  link.SetDeviceAttribute ("DataRate", StringValue (bw));
  link.SetChannelAttribute ("Delay", StringValue (delay));
  if (errorModel)
    link.SetDeviceAttribute ("ReceiveErrorModel", PointerValue (errorModel));
  return link;
}

void dumbbell::ConfigureTrafficControl (QueueDiscContainer &container, 
            NetDeviceContainer &devices, std::string &queueDisc, 
            uint32_t queueDiscSize) 
{
  if (queueDiscSize > 0) {
    TrafficControlHelper tch;
    tch.SetRootQueueDisc ("ns3::" + queueDisc, "MaxSize", StringValue (std::to_string (queueDiscSize) + "p"));
    container = tch.Install (devices);
  }
}

void dumbbell::SetupApplicationPair (Ptr<Node> sender, Ptr<Node> receiver, Ipv4Address dstAddr,
                           double startTime, double stopTime, uint16_t port) {
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
  BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());
  ftp.SetAttribute ("Remote", AddressValue (InetSocketAddress (dstAddr, port)));
  ftp.SetAttribute ("SendSize", UintegerValue (1448));
  ftp.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApp = ftp.Install (sender);
  sourceApp.Start (Seconds (startTime));
  sourceApp.Stop (Seconds (stopTime - 3));

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
  ApplicationContainer sinkApp = sinkHelper.Install (receiver);
  sinkApp.Start (Seconds (startTime));
  sinkApp.Stop (Seconds (stopTime));
}



void dumbbell::InstallStack (InternetStackHelper stack)
{
  stack.Install (m_routers);
  stack.Install (m_leftLeaf);
  stack.Install (m_rightLeaf);
  std::cout << "Internet stack installed " << std::endl;
}

void dumbbell::AssignIpv4Addresses (Ipv4AddressHelper leftIp,
                  Ipv4AddressHelper rightIp, Ipv4AddressHelper routerIp)
{
  // Assign the router network
  m_routerInterfaces = routerIp.Assign (m_routerDevices);
  // Assign to left side 
  for (uint32_t i = 0; i < LeftCount (); ++i)
    {
      NetDeviceContainer ndc;
      ndc.Add (m_leftLeafDevices.Get (i));
      ndc.Add (m_leftRouterDevices.Get (i));
      Ipv4InterfaceContainer ifc = leftIp.Assign (ndc);
      m_leftLeafInterfaces.Add (ifc.Get (0));
      m_leftRouterInterfaces.Add (ifc.Get (1));
      std::cout << GetLeftLeafIpv4Address (i) << " " << m_leftRouterInterfaces.GetAddress (i) << std::endl; 
      leftIp.NewNetwork ();
    }
  // Assign to right side
  for (uint32_t i = 0; i < RightCount (); ++i)
    {
      NetDeviceContainer ndc;
      ndc.Add (m_rightLeafDevices.Get (i));
      ndc.Add (m_rightRouterDevices.Get (i));
      Ipv4InterfaceContainer ifc = rightIp.Assign (ndc);
      m_rightLeafInterfaces.Add (ifc.Get (0));
      m_rightRouterInterfaces.Add (ifc.Get (1));
      std::cout << m_rightLeafInterfaces.GetAddress (i) << " " << m_rightRouterInterfaces.GetAddress (i) << std::endl; 
      rightIp.NewNetwork ();
    }

    std::cout << "IP addresses assigned " << std::endl;
}

// -- Public Methods -----------------------------------------------------------

// ... (unchanged public methods follow)

} // namespace ns3
