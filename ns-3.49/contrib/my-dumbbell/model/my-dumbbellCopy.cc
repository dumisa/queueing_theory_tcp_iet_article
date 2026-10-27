/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "my-dumbbell.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-header.h"
#include "ns3/traffic-control-module.h"
#include "ns3/udp-header.h"

#include <fstream>
#include <iostream>
#include <string>

namespace ns3 {

//function for dumbbell class

dumbbell::~dumbbell () {}
dumbbell::dumbbell () {}

dumbbell::dumbbell(uint8_t flows, std::string btlBW, std::string btlDelay, 
    std::string accessBW, std::string accessDelay, 
    std::string queueDisc, uint32_t queueDiscSize,
    double error_p, double sim_start, double sim_stop)
{
    queueDisc = "ns3::" + queueDisc;
    // Configure links

    // Configure the error model
    // Here we use RateErrorModel with packet error rate
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

    //bottleNeckLink.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

    ns3::PointToPointHelper accessLink; //left and right links
    accessLink.SetDeviceAttribute("DataRate",
            ns3::StringValue(accessBW));
    accessLink.SetChannelAttribute("Delay",
            ns3::StringValue(accessDelay));

    // Create the nodes
    m_routers.Create (2);
    m_leftLeaf.Create (flows);
    m_rightLeaf.Create (flows);
    std::cout << "Nodes created" << std::endl;   

    // Add the link connecting routers
    m_routerDevices = m_bottleNeckLink.Install (m_routers);

    std::cout << "Bottleneck link installed - created routerdevices" << std::endl;  

    // Add the left side links
    for (uint32_t i = 0; i < flows; ++i)
    {
        NetDeviceContainer c = accessLink.Install (m_routers.Get (0), m_leftLeaf.Get (i));
        m_leftRouterDevices.Add (c.Get (0));
        m_leftLeafDevices.Add (c.Get (1));
    }
    std::cout << "Senders connected" << std::endl; 

    // Add the right side links
    for (uint32_t i = 0; i < flows; ++i)
    {
        NetDeviceContainer c = accessLink.Install (m_routers.Get (1),
                                                  m_rightLeaf.Get (i));
        m_rightRouterDevices.Add (c.Get (0));
        m_rightLeafDevices.Add (c.Get (1));
    }   
    std::cout << "Receivers connected" << std::endl; 

    // install internet stack on all nodes
    ns3::InternetStackHelper stack;
    //InstallStack(stack);
    stack.Install(m_routers);
    stack.Install(m_leftLeaf);
    stack.Install(m_rightLeaf);

    // Bottleneck link traffic control configuration
    TrafficControlHelper tchPfifo_btl;
    tchPfifo_btl.SetRootQueueDisc (queueDisc, "MaxSize",
                                 StringValue (std::to_string(queueDiscSize) + "p"));
    /*TrafficControlHelper tchCoDel_btl;
    tchCoDel.SetRootQueueDisc ("ns3::CoDelQueueDisc");
    Config::SetDefault ("ns3::CoDelQueueDisc::MaxSize", StringValue (std::to_string(queueDiscSize) + "p"));*/

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
    AssignIpv4Addresses(leftIp, rightIp, routerIp);

    //NS_LOG_INFO ("Initialize Global Routing.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    std::cout << "RoutingTables done " << std::endl;

    uint16_t port = 50000;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);

    m_sender_sockets.reserve (LeftCount ());

    std::cout << "sender sockets created " << std::endl;

    for (uint16_t i = 0; i < LeftCount (); i++)
    {
        AddressValue remoteAddress (InetSocketAddress (m_rightLeafInterfaces.GetAddress (i, 0), port));
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
        BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());

        ftp.SetAttribute ("Remote", remoteAddress);
        ftp.SetAttribute ("SendSize", UintegerValue (1448));
        ftp.SetAttribute ("MaxBytes", UintegerValue (0));

        ApplicationContainer sourceApp = ftp.Install (m_leftLeaf.Get (i));
        sourceApp.Start (Seconds (sim_start * i));
        sourceApp.Stop (Seconds (sim_stop - 3));

        sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
        ApplicationContainer sinkApp = sinkHelper.Install (m_rightLeaf.Get (i));
        sinkApp.Start (Seconds (sim_start * i));
        sinkApp.Stop (Seconds (sim_stop));
    }

    std::cout << "dumbbell topology configured " << std::endl;
}


dumbbell::dumbbell(std::string tcp_type1, std::string tcp_type2, uint8_t type1_flows, uint8_t type2_flows, std::string btlBW, std::string btlDelay, 
    std::string accessBW, std::string accessDelay1, std::string accessDelay2, 
    std::string queueDisc, uint32_t queueDiscSize,
    double error_p, double sim_start1, double sim_stop1, double sim_start2, double sim_stop2)
{
    queueDisc = "ns3::" + queueDisc;
    // Configure links

    // Configure the error model
    // Here we use RateErrorModel with packet error rate
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

    //bottleNeckLink.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

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

    // Create the nodes
    m_routers.Create (2);
    m_leftLeaf.Create (type1_flows+type2_flows);
    m_rightLeaf.Create (type1_flows+type2_flows);
    std::cout << "Nodes created" << std::endl;   

    // Add the link connecting routers
    m_routerDevices = m_bottleNeckLink.Install (m_routers);

    std::cout << "Bottleneck link installed - created routerdevices" << std::endl;  

    // Add the left side links
    tcp_type1 = "ns3::" + tcp_type1;
    tcp_type2 = "ns3::" + tcp_type2;
    TypeId tcpTid1 = TypeId::LookupByName(tcp_type1); // or oth
    TypeId tcpTid2 = TypeId::LookupByName(tcp_type2); 
    for (uint32_t i = 0; i < type1_flows+type2_flows; ++i)
    {
        NetDeviceContainer c;
        if(i < type1_flows){
            m_leftLeaf.Get (i)->GetObject<TcpL4Protocol>()->SetAttribute("SocketType", TypeIdValue(tcpTid1));
            c = accessLink1.Install (m_routers.Get (0), m_leftLeaf.Get (i));
        } else {
            m_leftLeaf.Get (i)->GetObject<TcpL4Protocol>()->SetAttribute("SocketType", TypeIdValue(tcpTid2));
            c = accessLink2.Install (m_routers.Get (0), m_leftLeaf.Get (i));
        }
//        NetDeviceContainer c = accessLink.Install (m_routers.Get (0), m_leftLeaf.Get (i));
        m_leftRouterDevices.Add (c.Get (0));
        m_leftLeafDevices.Add (c.Get (1));
    }
    std::cout << "Senders connected" << std::endl; 

    // Add the right side links
    for (uint32_t i = 0; i < type1_flows+type2_flows; ++i)
    {
        NetDeviceContainer c;
        if(i < type1_flows){
            m_rightLeaf.Get (i)->GetObject<TcpL4Protocol>()->SetAttribute("SocketType", TypeIdValue(tcpTid1));
            c = accessLink1.Install (m_routers.Get (1), m_leftLeaf.Get (i));
        } else {
            m_rightLeaf.Get (i)->GetObject<TcpL4Protocol>()->SetAttribute("SocketType", TypeIdValue(tcpTid2));
            c = accessLink2.Install (m_routers.Get (1), m_leftLeaf.Get (i));
        }
        //NetDeviceContainer c = accessLink.Install (m_routers.Get (1), m_rightLeaf.Get (i));
        m_rightRouterDevices.Add (c.Get (0));
        m_rightLeafDevices.Add (c.Get (1));
    }   
    std::cout << "Receivers connected" << std::endl; 

    // install internet stack on all nodes
    ns3::InternetStackHelper stack;
    //InstallStack(stack);
    stack.Install(m_routers);
    stack.Install(m_leftLeaf);
    stack.Install(m_rightLeaf);

    // Bottleneck link traffic control configuration
    TrafficControlHelper tchPfifo_btl;
    tchPfifo_btl.SetRootQueueDisc (queueDisc, "MaxSize",
                                 StringValue (std::to_string(queueDiscSize) + "p"));
    /*TrafficControlHelper tchCoDel_btl;
    tchCoDel.SetRootQueueDisc ("ns3::CoDelQueueDisc");
    Config::SetDefault ("ns3::CoDelQueueDisc::MaxSize", StringValue (std::to_string(queueDiscSize) + "p"));*/

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
    AssignIpv4Addresses(leftIp, rightIp, routerIp);

    //NS_LOG_INFO ("Initialize Global Routing.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    std::cout << "RoutingTables done " << std::endl;

    uint16_t port = 50000;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);

    m_sender_sockets.reserve (LeftCount ());

    std::cout << "sender sockets created " << std::endl;

    for (uint16_t i = 0; i < LeftCount (); i++)
    {
        AddressValue remoteAddress (InetSocketAddress (m_rightLeafInterfaces.GetAddress (i, 0), port));
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
        BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());

        ftp.SetAttribute ("Remote", remoteAddress);
        ftp.SetAttribute ("SendSize", UintegerValue (1448));
        ftp.SetAttribute ("MaxBytes", UintegerValue (0));

        ApplicationContainer sourceApp = ftp.Install (m_leftLeaf.Get (i));
        if (i < type1_flows){
            sourceApp.Start (Seconds (sim_start1 + 0.001 * i));
            sourceApp.Stop (Seconds (sim_stop1 - 0.001*i));

        } else {
            sourceApp.Start (Seconds (sim_start2 + 0.001 * i));  
            sourceApp.Stop (Seconds (sim_stop2 - 0.001*i)); 
        }

        sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
        ApplicationContainer sinkApp = sinkHelper.Install (m_rightLeaf.Get (i));
        if (i < type1_flows){
            sinkApp.Start (Seconds (sim_start1 + 0.001 * i));
            sinkApp.Stop (Seconds (sim_stop1));

        } else {
            sinkApp.Start (Seconds (sim_start2 + 0.001 * i));  
            sinkApp.Stop (Seconds (sim_stop2)); 
        }

    }

    std::cout << "dumbbell topology configured " << std::endl;
}



dumbbell::dumbbell(uint8_t flows, std::string btlBW, std::string btlDelay, 
    std::string accessBW, std::string accessDelay, std::string queueDisc, uint32_t queueDiscSize,
    double error_p, std::vector<double> app_start, std::vector<double> app_stop)
{
    queueDisc = "ns3::" + queueDisc;
    // Configure links

    // Configure the error model
    // Here we use RateErrorModel with packet error rate
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

    //bottleNeckLink.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

    ns3::PointToPointHelper accessLink; //left and right links
      
    accessLink.SetDeviceAttribute("DataRate",
                ns3::StringValue(accessBW));
    accessLink.SetChannelAttribute("Delay",
                ns3::StringValue(accessDelay));

    // Create the nodes
    m_routers.Create (2);
    m_leftLeaf.Create (flows);
    m_rightLeaf.Create (flows);
    std::cout << "Nodes created" << std::endl;   

    // Add the link connecting routers
    m_routerDevices = m_bottleNeckLink.Install (m_routers);

    std::cout << "Bottleneck link installed - created routerdevices" << std::endl;  

    // Add the left side links
    for (uint32_t i = 0; i < flows; ++i)
    {
        NetDeviceContainer c = accessLink.Install (m_routers.Get (0), m_leftLeaf.Get (i));
        m_leftRouterDevices.Add (c.Get (0));
        m_leftLeafDevices.Add (c.Get (1));
    }
    std::cout << "Senders connected" << std::endl; 

    // Add the right side links
    for (uint32_t i = 0; i < flows; ++i)
    {
        NetDeviceContainer c = accessLink.Install (m_routers.Get (1),
                                                  m_rightLeaf.Get (i));
        m_rightRouterDevices.Add (c.Get (0));
        m_rightLeafDevices.Add (c.Get (1));
    }   
    std::cout << "Receivers connected" << std::endl; 

    // install internet stack on all nodes
    ns3::InternetStackHelper stack;
    //InstallStack(stack);
    stack.Install(m_routers);
    stack.Install(m_leftLeaf);
    stack.Install(m_rightLeaf);

    // Bottleneck link traffic control configuration
    TrafficControlHelper tchPfifo_btl;
    tchPfifo_btl.SetRootQueueDisc (queueDisc, "MaxSize",
                                 StringValue (std::to_string(queueDiscSize) + "p"));
    /*TrafficControlHelper tchCoDel_btl;
    tchCoDel.SetRootQueueDisc ("ns3::CoDelQueueDisc");
    Config::SetDefault ("ns3::CoDelQueueDisc::MaxSize", StringValue (std::to_string(queueDiscSize) + "p"));*/

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
    AssignIpv4Addresses(leftIp, rightIp, routerIp);

    //NS_LOG_INFO ("Initialize Global Routing.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    std::cout << "RoutingTables done " << std::endl;

    uint16_t port = 50000;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);

    m_sender_sockets.reserve (LeftCount ());

    std::cout << "sender sockets created " << std::endl;

    for (uint16_t i = 0; i < LeftCount (); i++)
    {
        AddressValue remoteAddress (InetSocketAddress (m_rightLeafInterfaces.GetAddress (i, 0), port));
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
        BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());

        ftp.SetAttribute ("Remote", remoteAddress);
        ftp.SetAttribute ("SendSize", UintegerValue (1448));
        ftp.SetAttribute ("MaxBytes", UintegerValue (0));

        ApplicationContainer sourceApp = ftp.Install (m_leftLeaf.Get (i));
        sourceApp.Start (Seconds (app_start[i]));
        sourceApp.Stop (Seconds (app_stop[i]));

        sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
        ApplicationContainer sinkApp = sinkHelper.Install (m_rightLeaf.Get (i));
        sinkApp.Start (Seconds (app_start[i]));
        sinkApp.Stop (Seconds (app_stop[i]));
    }

    std::cout << "dumbbell topology configured " << std::endl;
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

void dumbbell::printTopologyConfirmation ()
{
    std::cout << "Confirmation: " << std::endl
              << "     " << RoutersCount () << " routers, " << std::endl
              << "     " << LeftCount ()  << " senders and " << std::endl
              << "     " << RightCount ()  << " receivers" << std::endl
              << "     " << RouterDevicesCount ()  
                        << " BtlNeck Router Devices (netcard)" << std::endl 
              << "     " << LeftRouterDevicesCount ()  
                        << " Left Router Devices (netcard)" << std::endl 
              << "     " << LeftLeafDevicesCount ()  
                        << " Left Leaf Devices (netcard)" << std::endl 
              << "     " << RightRouterDevicesCount ()  
                        << " Right Router Devices (netcard)" << std::endl 
              << "     " << RightLeafDevicesCount ()  << " Right Leaf Devices (netcard)" << std::endl; 

    std::cout << "Done confirmation" << std::endl;
}

void dumbbell::printAssignConfirmation ()
{
    uint32_t flows = LeftCount ();
    std::cout << "Routers IP Addresses:" << std::endl;

    for (uint32_t i=0; i < 2; i++)
    {
        std::cout << "   " << GetRouterIpv4Address (i) << std::endl;
    }

    std::cout << "Left Leaf IP Addresses:" << std::endl;
    for (uint32_t i=0; i < flows; i++)
    {
        std::cout << "   " << GetLeftLeafIpv4Address (i) << std::endl;
    }    

    std::cout << "Left Router IP Addresses:" << std::endl;
    for (uint32_t i=0; i<flows; i++)
    {
        std::cout << "   " << GetLeftRouterIpv4Address (i) << std::endl;
    }

    std::cout << "Right Leaf IP Addresses:" << std::endl;
    for (uint32_t i=0; i<flows; i++)
    {
        std::cout << "   " << GetRightLeafIpv4Address (i) << std::endl;
    }

    std::cout << "Right Router IP Addresses:" << std::endl;
    for (uint32_t i=0; i<flows; i++)
    {
        std::cout << "   " << GetRightRouterIpv4Address (i) << std::endl;
    }
    std::cout << "Done confirmation" << std::endl;
}


}

