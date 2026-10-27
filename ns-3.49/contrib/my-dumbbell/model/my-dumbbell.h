/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * \file my-dumbbell.h
 * \brief Header for dumbbell topology setup for NS-3
 *
 * This helper class constructs a dumbbell topology with flexible
 * configurations: single or dual TCP variants, per-flow timing,
 * custom bandwidths, delays, queue disciplines, and error models.
 */

#ifndef MY_DUMBBELL_H
#define MY_DUMBBELL_H

#include <string>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"

namespace ns3 {

/**
 * \ingroup topology
 * \brief Helper to configure a dumbbell topology
 */
class dumbbell
{
public:
  /** Default constructor */
  dumbbell ();
  /** Destructor */
  ~dumbbell ();

  /**
   * Single TCP type, uniform (staggered) flow start/stop times.
   */
  dumbbell (uint8_t flows, std::string tcp_type,
                    std::string btlBW, std::string btlDelay,
                    std::string accessBW, std::string accessDelay,
                    std::string queueDisc, uint32_t queueDiscSize,
                    double error_p, double sim_start, double sim_stop);

  /**
   * Single TCP type, explicit per-flow start/stop times vectors.
   */
  // Constructor with per-flow app start/stop times
dumbbell (uint8_t flows, std::string btlBW, std::string btlDelay,
                    std::string accessBW, std::string accessDelay,
                    std::string queueDisc, uint32_t queueDiscSize,
                    double error_p, 
                    std::vector<double> app_start, std::vector<double> app_stop);

  /**
   * Dual TCP variants, separate flow counts, delays, and intervals.
   */
  dumbbell (std::string tcp_type1, std::string tcp_type2, 
                    uint8_t type1_flows, uint8_t type2_flows,
                    std::string btlBW, std::string btlDelay,
                    std::string accessBW, std::string accessDelay1, std::string accessDelay2,
                    std::string queueDisc, uint32_t queueDiscSize,
                    double error_p, double sim_start1, double sim_stop1,
                    double sim_start2, double sim_stop2);

  /** Install the Internet stack on all nodes */
  void InstallStack (InternetStackHelper stack);

  /** Assign IPv4 addresses to all links */
  void AssignIpv4Addresses (Ipv4AddressHelper leftIp,
                             Ipv4AddressHelper rightIp,
                             Ipv4AddressHelper routerIp);

  /** Print a summary of the topology */
  void printTopologyConfirmation ();

  /** Print assigned IP addresses */
  void printAssignConfirmation ();

  /**
   * \returns total number of nodes
   */
  uint32_t RoutersCount () const {return m_routers.GetN (); } ;
  uint32_t LeftCount () const {return m_leftLeaf.GetN ();} ;
  uint32_t RightCount () const {return m_rightLeaf.GetN ();} ;

    /**
   * \returns total number of Devices
   */
  uint32_t  RouterDevicesCount () const {return m_routerDevices.GetN ();} ;
  uint32_t  LeftRouterDevicesCount () const {return m_leftRouterDevices.GetN (); } ;
  uint32_t  LeftLeafDevicesCount () const {return m_leftLeafDevices.GetN (); };
  uint32_t  RightRouterDevicesCount () const {return m_rightRouterDevices.GetN (); };
  uint32_t  RightLeafDevicesCount () const {return m_rightLeafDevices.GetN (); };

  /**
   * \returns pointer to the routers/nodes
   */
  Ptr<Node> GetLeftRouter () const {return m_routers.Get (0);} ;
  Ptr<Node> GetRightRouter () const {return m_routers.Get (1);} ;

    /**
   * \returns pointer to the i'th left and right sides leaf node
   * \param i node number
   */
  Ptr<Node> GetLeftLeaf (uint32_t i) const {return m_leftLeaf.Get (i);};
  Ptr<Node> GetRightLeaf (uint32_t i) const {return m_rightLeaf.Get (i);};

  Ptr<NetDevice> GetRouterDevices (uint32_t i) const {return m_routerDevices.Get (i);}
  uint32_t GetRouterDevicesCount () const {return m_routerDevices.GetN ();}

    /**
   * \returns an Ipv4Address Ipv6Address of the i'th left and right leafs
   * \param i node number
   */
  Ipv4Address GetRouterIpv4Address (uint32_t i ) const
        {return m_routerInterfaces.GetAddress (i); } ; // Get left leaf address
  Ipv4Address GetLeftLeafIpv4Address (uint32_t i ) const
        {return m_leftLeafInterfaces.GetAddress (i); } ; // Get left leaf address
  Ipv4Address GetRightLeafIpv4Address (uint32_t i) const
        {return m_rightLeafInterfaces.GetAddress (i); } ; // Get right leaf address
  Ipv4Address GetLeftRouterIpv4Address (uint32_t i ) const
        {return m_leftRouterInterfaces.GetAddress (i); } ; // Get left leaf address
  Ipv4Address GetRightRouterIpv4Address (uint32_t i) const
        {return m_rightRouterInterfaces.GetAddress (i); } ; // Get right leaf address

  // QueueDisc accessors
  Ptr<QueueDisc> GetBtlQueueDisc     (uint32_t i) const;
  Ptr<QueueDisc> GetLeftAccessQueueDisc  (uint32_t i) const;
  Ptr<QueueDisc> GetRightAccessQueueDisc (uint32_t i) const;
  uint16_t       GetBtlQueueDiscCount () const;

  /**
   * Helper: configure the error model packet error rate
   */
  static void SetErrorModel(Ptr<RateErrorModel> model, double error_p);

  /**
   * Helper: create a point-to-point link with optional error model
   */
  static PointToPointHelper CreateP2PLink(const std::string bw,
                                          const std::string delay,
                                          Ptr<RateErrorModel> errorModel = nullptr);

  /**
   * Helper: setup traffic control queue disc on devices
   */
  static void ConfigureTrafficControl(QueueDiscContainer &container,
                                       NetDeviceContainer &devices,
                                      std::string &queueDisc,
                                      uint32_t queueDiscSize);




  /**
   * Helper: install a bulk send and sink application pair
   */
  static void SetupApplicationPair(Ptr<Node> sender,
                                   Ptr<Node> receiver,
                                   Ipv4Address dstAddr,
                                   double startTime,
                                   double stopTime,
                                   uint16_t port = 50000);

private:
  // Nodes
  NodeContainer          m_leftLeaf;
  NodeContainer          m_rightLeaf;
  NodeContainer          m_routers;

  // NetDevices
  NetDeviceContainer     m_leftLeafDevices;
  NetDeviceContainer     m_rightLeafDevices;
  NetDeviceContainer     m_leftRouterDevices;
  NetDeviceContainer     m_rightRouterDevices;
  NetDeviceContainer     m_routerDevices;

  // Interfaces
  Ipv4InterfaceContainer m_leftLeafInterfaces;
  Ipv4InterfaceContainer m_rightLeafInterfaces;
  Ipv4InterfaceContainer m_leftRouterInterfaces;
  Ipv4InterfaceContainer m_rightRouterInterfaces;
  Ipv4InterfaceContainer m_routerInterfaces;

  // Applications and queues
  ApplicationContainer   m_leftApp;
  ApplicationContainer   m_rightApp;
  QueueDiscContainer     m_btl_queueDiscs;
  QueueDiscContainer     m_leftAccess_queueDiscs;
  QueueDiscContainer     m_rightAccess_queueDiscs;

  // BulkSendHelper sockets
  std::vector<Ptr<TcpSocket>> m_sender_sockets;

  // Link and error model
  PointToPointHelper     m_bottleNeckLink;
  RateErrorModel         m_error_model;
};

} // namespace ns3

#endif // MY_DUMBBELL_H
