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
// Experiment3: Test impact of delay requirements under TCP traffic
//
// Network topology
//
//
//                      
//    n0 --m3-- n3 --h3-- n6 
//    |h5       |m5       |m5
//    n1 --h3-- n4 --h3-- n7
//    |m3       |h5       |h3
//    n2 --h3-- n5 --m5-- n8
// 
//  h - high capacity link (200Mbps) 
//  m - low capacity link (100Mbps)
//  3 - Base delay = 3ms
//  5 - Base delay = 5ms
// 
// - all links are point-to-point links with indicated one-way BW/delay
// - TCP packet size of 1024 bytes, TCP-CUBIC is enabled
// - DropTail queues 



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
#include "ns3/traffic-control-module.h"
#include "ns3/dsr-routing-module.h"
#include "ns3/point-to-point-grid.h"
#include "ns3/node-list.h"

using namespace ns3;  

// ============== Auxiliary functions ===============

// Function to check queue length of Routers
std::string dir = "results/Exp1/DSR/";
std::string ExpName = "DsrExperiment1";

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
                        uint16_t dataRate, bool flag)
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
                        uint16_t dataRate, bool flag)
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
  Ipv4DSRRoutingHelper dsr;
  Ipv4ListRoutingHelper list;
  list.Add (dsr, 10);
  InternetStackHelper internet;
  internet.SetRoutingHelper (list);
  uint32_t nRows = 3;
  uint32_t nCols = 3;
  PointToPointHelper p2p;
  PointToPointGridHelper p2pGrid(3,3,p2p);
  // install DSR-BP to the grid
  p2pGrid.InstallStack (internet);
  // install dsr-queue
  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::DsrVirtualQueueDisc");
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

  // -------------- Set Metric --------------------------
  for (uint32_t i = 0; i < nRows; i ++)
    {
      for (uint32_t j = 0; j < nCols; j ++)
        {
          std::cout << i << "," << j << std::endl;
          Ptr<Ipv4> ipv4 = p2pGrid.GetNode (i,j)->GetObject<Ipv4> ();
          std::cout << ipv4->GetNInterfaces () << std::endl;
          for (uint32_t k = 1; k < ipv4->GetNInterfaces (); k ++)
            {
              // Ptr<NetDevice> dev = ipv4->GetNetDevice (k);
              // DataRateValue dataRate;
              // dev->GetAttribute ("DataRate", dataRate);
              // uint32_t matric = dataRate.Get ().GetBitRate ();
              Ptr<Channel> channel = ipv4->GetNetDevice (k)->GetChannel ();
              TimeValue delay;
              channel->GetAttribute ("Delay", delay);
              uint32_t matric = delay.Get ().GetMicroSeconds ();
              ipv4->SetMetric(k, matric);
              std::cout << "the matric = " << matric << std::endl;
            }
        } 
    }
  // // ---------------- Create routingTable ---------------------
  Ipv4DSRRoutingHelper::PopulateRoutingTables ();
  // Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //---------------- Application ---------------
  // uint32_t beginTime = 0;
  uint32_t stopTime = 10.0;
  // ------------------------------------*** DSR Traffic Target Flow (n2 --> n6)-------------------------------------  
  //Create a dsrSink applications 
  // uint16_t sinkPort = 8080;
  // Ptr<Node> sinkNode = p2pGrid.GetNode (0, 0);
  // Ptr<NetDevice> sinkDev = sinkNode->GetDevice (0);
  // Ptr<Ipv4> ipv4 = sinkNode->GetObject<Ipv4> ();
  // int32_t interface = ipv4->GetInterfaceForDevice (sinkDev);
  // Address sinkAddress (InetSocketAddress (addr, sinkPort));
  // addr.Print (std::cout);
 
  // InstallPacketSink (sinkNode, sinkPort, "ns3::UdpSocketFactory", beginTime, stopTime);

  // // create a dsrSender application
  // // uint32_t PacketSize = 1024;
  // // for test
  // uint32_t PacketSize = 52;
  // uint32_t NPacket = 1;
  // uint32_t budget = 20;
  // for (int i=1; i<2; i++)
  // {
  //   InstallDGPacketSend (nodes.Get(2), sinkAddress, i-1, i, PacketSize, NPacket, budget, i, true);
  // }




  // ---------------- Net Anim ---------------------
  AnimationInterface anim(ExpName + ".xml");

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
