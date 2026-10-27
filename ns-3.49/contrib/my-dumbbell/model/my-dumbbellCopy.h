/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: George F. Riley<riley@ece.gatech.edu>
 */

// Define an object to create a dumbbell topology.

#ifndef MY_DUMBBELL_H
#define MY_DUMBBELL_H

#include <string>
#include <fstream>
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/error-model.h"

namespace ns3 {

/**
 * \ingroup point-to-point-layout
 *
 * \brief A helper to make it easier to create a dumbbell topology
 * with p2p links
 */
class dumbbell
{
public:
  dumbbell ();
  dumbbell(uint8_t flows, std::string btlBW, std::string btlDelay, 
        std::string accessBW, std::string accessDelay, std::string queueDisc, uint32_t queueDiscSize, 
        double error_p, double sim_start, double sim_stop);
  dumbbell(uint8_t flows, std::string btlBW, std::string btlDelay, 
        std::string accessBW, std::string accessDelay, std::string queueDisc, uint32_t queueDiscSize,
        double error_p, std::vector<double> app_start, std::vector<double> app_stop);

  dumbbell(std::string tcp_type1, std::string tcp_type2, uint8_t type1_flows, uint8_t type2_flows, std::string btlBW, std::string btlDelay, 
      std::string accessBW, std::string type1_accessDelay, std::string type2_accessDelay, 
      std::string queueDisc, uint32_t queueDiscSize,
      double error_p, double sim_start1, double sim_stop1, double sim_start2, double sim_stop2);


  ~dumbbell ();
  void CreateBtlneck ();
  void CreateRouters ();
  //void AddSenderReceiverPairs (uint32_t);
  void AddAccessLinks ();
  void CreateTopology ();
  void CreateTopology (uint8_t flows, std::string btlBW, std::string btlDelay, 
        std::string accessBW, std::string accessDelay, std::string queueDisc, uint32_t queueDiscSize, 
        double error_p, double sim_start, double sim_stop);
  void InstallStack (InternetStackHelper);
  void AssignIpv4Addresses (Ipv4AddressHelper leftIp, Ipv4AddressHelper rightIp, 
                            Ipv4AddressHelper routerIp);
  void printTopologyConfirmation ();
  void printAssignConfirmation ();
  //void ChangeBW (uint64_t);

  PointToPointHelper m_bottleNeckLink;
  RateErrorModel m_error_model;


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

  uint32_t GetLeftAppCount () const {return m_leftApp.GetN ();}
  Ptr<Application> GetLeftApp (uint32_t i) const {return m_leftApp.Get (i);} 
  Ptr<Application > GetRightApp (uint32_t i) const {return m_rightApp.Get (i);} 
  Ptr< QueueDisc > GetBtlQueueDisc (uint32_t i) const {return m_btl_queueDiscs.Get (i);}
  Ptr< QueueDisc > GetLeftAccessQueueDisc (uint32_t i) const {return m_leftAccess_queueDiscs.Get (i);}
  Ptr< QueueDisc > GetRightAccessQueueDisc (uint32_t i) const {return m_rightAccess_queueDiscs.Get (i);}
  uint16_t GetBtlQueueDiscCount () const {return m_btl_queueDiscs.GetN ();}

private:
    NodeContainer          m_leftLeaf;            //!< Left Leaf nodes
    NetDeviceContainer     m_leftLeafDevices;     //!< Left Leaf NetDevices
    NodeContainer          m_rightLeaf;           //!< Right Leaf nodes
    NetDeviceContainer     m_rightLeafDevices;    //!< Right Leaf NetDevices
    NodeContainer          m_routers;             //!< Routers
    NetDeviceContainer     m_routerDevices;       //!< Routers NetDevices
    NetDeviceContainer     m_leftRouterDevices;     //!< Left router NetDevices
    NetDeviceContainer     m_rightRouterDevices;    //!< Right router NetDevices
    Ipv4InterfaceContainer m_leftLeafInterfaces;    //!< Left Leaf interfaces (IPv4)
    Ipv4InterfaceContainer m_leftRouterInterfaces;  //!< Left router interfaces (IPv4)
    Ipv4InterfaceContainer m_rightLeafInterfaces;   //!< Right Leaf interfaces (IPv4)
    Ipv4InterfaceContainer m_rightRouterInterfaces; //!< Right router interfaces (IPv4)
    Ipv4InterfaceContainer m_routerInterfaces;      //!< Router interfaces (IPv4)
    Ipv6InterfaceContainer m_leftLeafInterfaces6;   //!< Left Leaf interfaces (IPv6)
    Ipv6InterfaceContainer m_leftRouterInterfaces6; //!< Left router interfaces (IPv6)
    Ipv6InterfaceContainer m_rightLeafInterfaces6;  //!< Right Leaf interfaces (IPv6)
    Ipv6InterfaceContainer m_rightRouterInterfaces6;  //!< Right router interfaces (IPv6)
    Ipv6InterfaceContainer m_routerInterfaces6;     //!< Router interfaces (IPv6)
    ApplicationContainer   m_leftApp;
    ApplicationContainer   m_rightApp;
    QueueDiscContainer     m_btl_queueDiscs;
    QueueDiscContainer     m_leftAccess_queueDiscs;
    QueueDiscContainer     m_rightAccess_queueDiscs;
    std::vector<Ptr<TcpSocket>>            m_sender_sockets;
    //m_btlBW;
    //m_btlDelay;
    //vector<double> m_accessDelay

};


}

#endif /* MY_DUMBBELL_H */
