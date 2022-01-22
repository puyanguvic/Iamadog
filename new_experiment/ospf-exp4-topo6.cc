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
 */


/*
Author: Xiangyu Ren
*/
// Experiment2: Test impact of topology size and shape
//
// Network topology
//
// Grid: 6x6
// 
// - all links are point-to-point links with same link condition
// - UDP packet size of 1024 bytes.
// - DropTail queues 
// - Tracing of queues and packet receptions to file "simple-global-routing.tr"



#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <sys/stat.h>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-dsr-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/dsr-routing-module.h"
#include "ns3/point-to-point-grid.h"
#include "ns3/node-list.h"

using namespace ns3;  

// ============== Auxiliary functions ===============

// Function to check queue length of Routers
std::string dir = "results/Exp4-topo6/OSPF/";
std::string ExpName = "OSPFExperiment4-topo6";

void
CheckQueueSize (Ptr<QueueDisc> queueDisc, std::string qID)
{
  uint32_t qSize = queueDisc->GetCurrentSize ().GetValue ();
  uint32_t qMaxSize = queueDisc->GetMaxSize ().GetValue ();
  // Check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queueDisc, qID);
  std::ofstream fPlotQueue (std::stringstream (dir + "/queue-size-" + qID + ".dat").str ().c_str (), std::ios::out | std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue << qMaxSize << std::endl;
  fPlotQueue.close ();
}

// // // Function to calculate drops in a particular Queue
// static void
// DropAtQueue (Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
// {
//   *stream->GetStream () << Simulator::Now ().GetSeconds () << " 1" << std::endl;
// }

// Function to install dsrSink application
void InstallPacketSink (Ptr<Node> node, uint16_t port, std::string socketFactory, double startTime, double stopTime)
{
  DsrSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (node);
  sinkApps.Start (Seconds (startTime));
  sinkApps.Stop (Seconds (stopTime));
}

// Function to install dsrSend application with budget
void InstallDGPacketSend (Ptr<Node> node, Address sinkAddress, double startTime, double stopTime,
                        uint32_t packetSize, uint32_t nPacket, uint32_t budget, 
                        uint32_t dataRate, bool flag)
{
  Ptr<Socket> dsrSocket = Socket::CreateSocket (node, UdpSocketFactory::GetTypeId ());
  Ptr<DsrApplication> dsrApp = CreateObject<DsrApplication> ();
  dsrApp->Setup (dsrSocket, sinkAddress, packetSize, nPacket, DataRate(std::to_string(dataRate) + "Mbps"), budget, flag);
  node->AddApplication (dsrApp);
  dsrApp->SetStartTime (Seconds (startTime));
  dsrApp->SetStopTime (Seconds (stopTime));
}

// Function to install dsrSend application without budget
void InstallBEPacketSend (Ptr<Node> node, Address sinkAddress, double startTime, double stopTime,
                        uint32_t packetSize, uint32_t nPacket, 
                        uint32_t dataRate, bool flag)
{
  Ptr<Socket> dsrSocket = Socket::CreateSocket (node, UdpSocketFactory::GetTypeId ());
  Ptr<DsrApplication> dsrApp = CreateObject<DsrApplication> ();
  dsrApp->Setup (dsrSocket, sinkAddress, packetSize, nPacket, DataRate(std::to_string(dataRate) + "Mbps"), flag);
  node->AddApplication (dsrApp);
  dsrApp->SetStartTime (Seconds (startTime));
  dsrApp->SetStopTime (Seconds (stopTime));
}

NS_LOG_COMPONENT_DEFINE (ExpName);

int 
main (int argc, char *argv[])
{
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
#if 0 
  LogComponentEnable (ExpName, LOG_LEVEL_INFO);
#endif

  // Allow the user to override any of the defaults and the above
  // DefaultValue::Bind ()s at run-time, via command-line arguments
  CommandLine cmd (__FILE__);
  bool enableFlowMonitor = false;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);
  // ------------------ build topology --------------------------
  NS_LOG_INFO ("Build grid topology");
  InternetStackHelper internet;
  uint32_t nRows = 6;
  uint32_t nCols = 6;
  std::string channelDataRate = "10Mbps";
  uint32_t delayInMicro = 3000;
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro)));
  PointToPointGridHelper p2pGrid(nRows,nCols,p2p);

  // install DSR-BP to the grid
  p2pGrid.InstallStack (internet);
  // install dsr-queue
  TrafficControlHelper tch;
//   tch.SetRootQueueDisc ("ns3::DsrVirtualQueueDisc");
  tch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue ("100p"));

  for (uint32_t i = 0; i < nRows; i ++)
    {
      for (uint32_t j = 0; j < nCols; j ++)
        {
          Ptr<Node> node = p2pGrid.GetNode (i, j);
          Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
          for (uint32_t k = 0; k < node->GetNDevices (); k ++)
            {
              Ptr<NetDevice> dev = node->GetDevice (k);
              tch.Install (dev);
            }
        }
    }
  Ipv4AddressHelper rowIp;
  rowIp.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4AddressHelper colIp;
  colIp.SetBase ("10.2.1.0", "255.255.255.0");
  p2pGrid.AssignIpv4Addresses (rowIp, colIp);
//   // -------------- Set Metric --------------------------
//   for (uint32_t i = 0; i < nRows; i ++)
//     {
//       for (uint32_t j = 0; j < nCols; j ++)
//         {
//           // std::cout << i << "," << j << std::endl;
//           Ptr<Ipv4> ipv4 = p2pGrid.GetNode (i,j)->GetObject<Ipv4> ();
//           // std::cout << ipv4->GetNInterfaces () << std::endl;
//           for (uint32_t k = 1; k < ipv4->GetNInterfaces (); k ++)
//             {
//               // Ptr<NetDevice> dev = ipv4->GetNetDevice (k);
//               // DataRateValue dataRate;
//               // dev->GetAttribute ("DataRate", dataRate);
//               // uint32_t matric = dataRate.Get ().GetBitRate ();
//               Ptr<Channel> channel = ipv4->GetNetDevice (k)->GetChannel ();
//               TimeValue delay;
//               channel->GetAttribute ("Delay", delay);
//               uint32_t matric = delay.Get ().GetMicroSeconds ();
//               // std::cout << "delay in microseconds = " << delay.Get ().GetMicroSeconds () << std::endl;
//               ipv4->SetMetric(k, matric);
//               // std::cout << "the matric = " << matric << std::endl;
//             }
//         } 
//     }
  // // ---------------- Create routingTable ---------------------
//   Ipv4DSRRoutingHelper::PopulateRoutingTables ();
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // // ---------- Allocate Node Positions --------------------------------------

  // NS_LOG_INFO ("Allocate Positions to Nodes.");

  // MobilityHelper mobility;
  //  // setup the grid itself: objects are laid out
  //  // started from (-100,-100) with 20 objects per row, 
  //  // the x interval between each object is 5 meters
  //  // and the y interval between each object is 20 meters
  // mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
  //                                "MinX", DoubleValue (-100.0),
  //                                "MinY", DoubleValue (-100.0),
  //                                "DeltaX", DoubleValue (10.0),
  //                                "DeltaY", DoubleValue (10.0),
  //                                "GridWidth", UintegerValue (20),
  //                                "LayoutType", StringValue ("RowFirst"));
  //  // each object will be attached a static position.
  //  // i.e., once set by the "position allocator", the
  //  // position will never change.
  //  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  
  //  // finalize the setup by attaching to each object
  //  // in the input array a position and initializing
  //  // this position with the calculated coordinates.
  //  for (uint32_t i = 0; i < nRows; i++)
  //  {
  //    for (uint32_t j = 0; j < nCols; j++)
  //    {
  //      mobility.Install (p2pGrid.GetNode (i, j));
  //    }
  //  }
 
  //----------------Target Application ---------------
  //Create a dsrSink applications 
  uint32_t beginTime = 0;
  uint32_t stopTime = 10;
  uint16_t sinkPort = 8080;
  uint32_t packetSize = 52;
  Ptr<Node> sinkNode = p2pGrid.GetNode (nRows-1, nCols-1);
  
  Ipv4Address sinkAddr = p2pGrid.GetIpv4Address (nRows-1, nCols-1);
  Address sinkAddress (InetSocketAddress (sinkAddr, sinkPort));
  InstallPacketSink (sinkNode, sinkPort, "ns3::UdpSocketFactory", beginTime, stopTime);

  // // create a dsrSender application
  // uint32_t packetSize = 1024;
  uint32_t nPacket = 10000;
  uint32_t dataRate = 5;
  Ptr<Node> sourceNode = p2pGrid.GetNode (0, 0);
  bool CheckFlag = true;
  InstallBEPacketSend (sourceNode, sinkAddress, beginTime, stopTime,
                        packetSize, nPacket,  
                        dataRate, CheckFlag);
  // for (int i=1; i<2; i++)
  // {
  //   InstallDGPacketSend (sourceNode, sinkAddress, i-1, i,  packetSize, nPacket, budget, i, CheckFlag);
  // }

  // //----------------Background Best Effort Application ---------------
  // uint32_t nPacketInterf1 = 1;
  // uint32_t dataRateInterf1 = 1;
  // bool checkFlagInterf1 = false;
  // // uint32_t i = 1;
  // for (uint32_t i = 1; i < nRows-1; i++)
  // {
  //   // Sink Node Configure
  //   Ptr<Node> sinkNodeInterf = p2pGrid.GetNode (nRows-1-i, nCols-1);
  //   Ipv4Address sinkAddrInterf = p2pGrid.GetIpv4Address (nRows-1-i, nCols-1);
  //   Address sinkAddressInterf (InetSocketAddress (sinkAddrInterf, sinkPort));
  //   InstallPacketSink (sinkNodeInterf, sinkPort, "ns3::UdpSocketFactory", beginTime, stopTime);

  //   // Source Node Configur
  //   Ptr<Node> sourceNodeInterf = p2pGrid.GetNode (i, 0);
  //   InstallBEPacketSend (sourceNodeInterf, sinkAddressInterf, beginTime, stopTime,
  //                        packetSize, nPacketInterf1, dataRateInterf1, checkFlagInterf1);
  // }

  //----------------Background Delay Guaranteed Application ---------------
  uint32_t nPacketInterf2 = 5000;
  // uint32_t dataRateInterf2 = 2;
  bool checkFlagInterf2 = false;
  // uint32_t i = 1;
  for (uint32_t i = 1; i < nRows-1; i++)
  {
    // Sink Node Configure
    Ptr<Node> sinkNodeInterf = p2pGrid.GetNode (nRows-1-i, nCols-1);
    Ipv4Address sinkAddrInterf = p2pGrid.GetIpv4Address (nRows-1-i, nCols-1);
    Address sinkAddressInterf (InetSocketAddress (sinkAddrInterf, sinkPort));
    InstallPacketSink (sinkNodeInterf, sinkPort, "ns3::UdpSocketFactory", beginTime, stopTime);

    // Source Node Configur
    Ptr<Node> sourceNodeInterf = p2pGrid.GetNode (i, 0);
    // InstallDGPacketSend (sourceNodeInterf, sinkAddressInterf, beginTime, stopTime,
    //                      packetSize, nPacketInterf2, budgetInterf,
    //                      dataRateInterf2, CheckFlagInterf2);
    for (int j=1; j<11; j++)
    {
      InstallBEPacketSend (sourceNodeInterf, sinkAddressInterf, (j-1)*0.1, j*0.1,
                           packetSize, nPacketInterf2,
                           2+j*0.5, checkFlagInterf2);
    }
  }


  // ---------------- Net Anim ---------------------
//   AnimationInterface anim(ExpName + ".xml");s

  // -------------- Print the routing table ----------------
  Ipv4DSRRoutingHelper d;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>
  (ExpName + ".routes", std::ios::out);
  d.PrintRoutingTableAllAt (Seconds (0), routingStream);

  // --------------- Store Recorded .dat --------------------
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}

