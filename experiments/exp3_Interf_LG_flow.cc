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
// Experiment1: Test Guarantee properties
//
// Network topology
//
//
//                      
//    n0 --m-- n3 --s-- n6 
//    |s       |m       |m
//    n1 --h-- n4 --h-- n7
//    |m       |s       |h
//    n2 --h-- n5 --s-- n8
// 
//  h - high capacity link (200Mbps) 
//  m - medium capcity link (150Mbps)
//  s - low capacity link (100Mbps)
// 
// - all links are point-to-point links with indicated one-way BW/delay
// - CBR/UDP flows from n0 to n3, and from n3 to n1
// - FTP/TCP flow from n0 to n3, starting at time 1.2 to time 1.35 sec.
// - UDP packet size of 210 bytes, with per-packet interval 0.00375 sec.
//   (i.e., DataRate of 448,000 bps)
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
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/dsr-tags-module.h"
#include "ns3/dsr-application-module.h"

using namespace ns3;  

// ============== Auxiliary functions ===============

// Function to check queue length of Routers
std::string dir = "exp3_results/";
void
CheckQueueSize (Ptr<QueueDiscClass> queueClass, std::string qID)
{
  uint32_t qSize = queueClass->GetQueueDisc()->GetCurrentSize ().GetValue ();
  // Check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queueClass, qID);
  std::ofstream fPlotQueue (std::stringstream (dir + "/queueTraces/queue-size-" + qID + ".dat").str ().c_str (), std::ios::out | std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
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
                        std::string dataRate, bool flag)
{
  Ptr<Socket> dsrSocket = Socket::CreateSocket (node, UdpSocketFactory::GetTypeId ());
  Ptr<DsrApplication> dsrApp = CreateObject<DsrApplication> ();
  dsrApp->Setup (dsrSocket, sinkAddress, packetSize, nPacket, DataRate(dataRate), budget, flag);
  node->AddApplication (dsrApp);
  dsrApp->SetStartTime (Seconds (startTime));
  dsrApp->SetStopTime (Seconds (stopTime));
}

// Function to install dsrSend application without budget
void InstallBEPacketSend (Ptr<Node> node, Address sinkAddress, double startTime, double stopTime,
                        uint32_t packetSize, uint32_t nPacket, 
                        std::string dataRate, bool flag)
{
  Ptr<Socket> dsrSocket = Socket::CreateSocket (node, UdpSocketFactory::GetTypeId ());
  Ptr<DsrApplication> dsrApp = CreateObject<DsrApplication> ();
  dsrApp->Setup (dsrSocket, sinkAddress, packetSize, nPacket, DataRate(dataRate), flag);
  node->AddApplication (dsrApp);
  dsrApp->SetStartTime (Seconds (startTime));
  dsrApp->SetStopTime (Seconds (stopTime));
}


NS_LOG_COMPONENT_DEFINE ("DsrExperiment1");

int 
main (int argc, char *argv[])
{
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
#if 0 
  LogComponentEnable ("DsrExperiment1", LOG_LEVEL_INFO);
#endif

  // Set up some default values for the simulation.  Use the 
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1000));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("5Mb/s"));
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("35p")));
  Config::SetDefault ("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue (QueueSize ("10p")));
  //DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

  // Allow the user to override any of the defaults and the above
  // DefaultValue::Bind ()s at run-time, via command-line arguments
  CommandLine cmd (__FILE__);
  bool enableFlowMonitor = false;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  // ------------------ build topology ---------------------------
  NS_LOG_INFO ("Create nodes.");
  NodeContainer left, right, routers, othernodes;
  left.Create (1);
  routers.Create (6);
  right.Create (1);
  othernodes.Create (4);
  NodeContainer nodes = NodeContainer(left, routers, right, othernodes);

  // record the links
  std::vector<NodeContainer> nodeAdjacencyList (13);
  nodeAdjacencyList[0] = NodeContainer(nodes.Get(0), nodes.Get(1));
  nodeAdjacencyList[1] = NodeContainer(nodes.Get(1), nodes.Get(2));
  nodeAdjacencyList[2] = NodeContainer(nodes.Get(1), nodes.Get(3));
  nodeAdjacencyList[3] = NodeContainer(nodes.Get(2), nodes.Get(4));
  nodeAdjacencyList[4] = NodeContainer(nodes.Get(3), nodes.Get(5));
  nodeAdjacencyList[5] = NodeContainer(nodes.Get(4), nodes.Get(6));
  nodeAdjacencyList[6] = NodeContainer(nodes.Get(5), nodes.Get(6));
  nodeAdjacencyList[7] = NodeContainer(nodes.Get(6), nodes.Get(7));
  nodeAdjacencyList[8] = NodeContainer(nodes.Get(2), nodes.Get(8));
  nodeAdjacencyList[9] = NodeContainer(nodes.Get(4), nodes.Get(9));
  nodeAdjacencyList[10] = NodeContainer(nodes.Get(3), nodes.Get(10));
  nodeAdjacencyList[11] = NodeContainer(nodes.Get(5), nodes.Get(11));

  // special link with high bandwidth and high delay
  nodeAdjacencyList[12] = NodeContainer(nodes.Get(1), nodes.Get(6));
  
  // ------------------- install internet protocol to nodes-----------
  Ipv4ListRoutingHelper listRH;
  // RipHelper ripRouting;
  // listRH.Add (ripRouting, 0);
  Ipv4StaticRoutingHelper staticRouting;
  listRH.Add (staticRouting, 0);
  Ipv4GlobalRoutingHelper globalRH;
  listRH.Add (globalRH, 1);

  InternetStackHelper internet;
  internet.SetIpv6StackInstall (false);
  internet.SetRoutingHelper (listRH);
  internet.Install (nodes);

  // ----------------create the channels -----------------------------
  NS_LOG_INFO ("Create channels.");
  std::vector<PointToPointHelper> p2p(13);

  // High capacity link
  std::string channelDataRate1 = "10Mbps"; // link data rate
  uint32_t delayInMicro1 = 5000; // transmission + propagation delay

  std::string channelDataRate2 = "20Mbps"; // link data rate
  uint32_t delayInMicro2 = 20000; // transmission + propagation delay
  for (int i = 0;i < 12; i ++)
  {
    p2p[i].SetQueue ("ns3::DropTailQueue");
    p2p[i].SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate1)));
    p2p[i].SetChannelAttribute ("Delay", TimeValue (MicroSeconds (delayInMicro1)));
    //p2p[i].SetQueue();
  }

  {
    p2p[12].SetQueue ("ns3::DropTailQueue");
    p2p[12].SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate2)));
    p2p[12].SetChannelAttribute ("Delay", TimeValue (MicroSeconds (delayInMicro2)));
  }
  
  std::vector<NetDeviceContainer> devices(13);
  for (int i = 0; i < 13; i++)
  {
    devices[i] = p2p[i].Install (nodeAdjacencyList [i]);
  }

  // ---------------- Schedule Recording Events ---------------
  // Create directories to store dat files
  struct stat buffer;
  int retVal;
  if ((stat (dir.c_str (), &buffer)) == 0)
    {
      std::string dirToRemove = "rm -rf " + dir;
      retVal = system (dirToRemove.c_str ());
      NS_ASSERT_MSG (retVal == 0, "Error in return value");
    }
  std::string dirToSave = "mkdir -p " + dir;
  retVal = system (dirToSave.c_str ());
  NS_ASSERT_MSG (retVal == 0, "Error in return value");
  retVal = system ((dirToSave + "/queueTraces/").c_str ());
  NS_ASSERT_MSG (retVal == 0, "Error in return value");
  NS_UNUSED (retVal);

  // ------------------- install dsr-queue -----------------------------
  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::DsrPrioQueueDisc", "DsrPriomap", StringValue("0 1 2"));
  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses (handle, 3, "ns3::QueueDiscClass");
  tch.AddChildQueueDisc (handle, cid[0], "ns3::FifoQueueDisc", "MaxSize", StringValue("10p"));
  tch.AddChildQueueDisc (handle, cid[1], "ns3::FifoQueueDisc", "MaxSize", StringValue("10p"));
  tch.AddChildQueueDisc (handle, cid[2], "ns3::FifoQueueDisc", "MaxSize", StringValue("2000p"));

  std::vector<QueueDiscContainer> qdisc(13);
  for (int i = 0; i < 13; i ++)
  {
     qdisc[i] = tch.Install (devices[i]);
  }

  // ------------------- IP addresses AND Link Metric ----------------------
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> interface(13);
  for (int i=0; i < 13; i ++)
  {
    std::ostringstream subset;
    subset<<"10.1."<<i+1<<".0";
    ipv4.SetBase (subset.str().c_str(), "255.255.255.0");
    interface[i] = ipv4.Assign (devices[i]);
    interface[i].SetMetric (0, delayInMicro1);
    interface[i].SetMetric (1, delayInMicro1);
  }
  interface[12].SetMetric (0, delayInMicro2);
  interface[12].SetMetric (1, delayInMicro2);

  // ---------------- Create routingTable ---------------------
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // ------------------------------------*** DSR Traffic Target Flow (n0 --> n7)-------------------------------------  
  //Create a dsrSink applications 
  uint16_t sinkPort = 9;
  Address sinkAddress (InetSocketAddress (interface[7].GetAddress (1), sinkPort));
  InstallPacketSink (nodes.Get (7), sinkPort, "ns3::UdpSocketFactory", 0.0, 10.0);

  // create a dsrSender application
  uint32_t Packetsize = 232;
  uint32_t NPacket = 100000;
  uint32_t budget = 30;
  InstallDGPacketSend (nodes.Get (0), sinkAddress, 0.0, 10.0, Packetsize, NPacket, budget, "5Mbps", true);

  // ---------------------------------------Network UDP Traffic C1 (n9-->n11) ------------------
  // Create a dsrSink applications
  Address sinkAddress_1 (InetSocketAddress (interface[11].GetAddress (1), sinkPort));
  InstallPacketSink (nodes.Get (11), sinkPort, "ns3::UdpSocketFactory", 0.0, 10.0);
  uint32_t Packetsize1 = 232;
  uint32_t NPacket1 = 100000;
  uint32_t budget1 = 20;
  for (int i=0; i < 10; i ++)
  {
    double startTime = i*1.0;
    double endTime = startTime + 1.0;
    std::ostringstream subset;
    subset<<i+1<<"Mbps";
    InstallDGPacketSend (nodes.Get (9), sinkAddress_1, startTime, endTime, Packetsize1, NPacket1, budget1, subset.str().c_str(), false);
  }


  // ---------------- Net Anim ---------------------
  AnimationInterface anim(dir + "ex2NetAnim.xml");
  anim.SetConstantPosition (nodes.Get(0), 10.0, 30.0);
  anim.SetConstantPosition (nodes.Get(1), 20.0, 30.0);
  anim.SetConstantPosition (nodes.Get(2), 30.0, 20.0);
  anim.SetConstantPosition (nodes.Get(3), 30.0, 40.0);
  anim.SetConstantPosition (nodes.Get(4), 40.0, 20.0);
  anim.SetConstantPosition (nodes.Get(5), 40.0, 40.0);
  anim.SetConstantPosition (nodes.Get(6), 50.0, 30.0);
  anim.SetConstantPosition (nodes.Get(7), 60.0, 30.0);
  anim.SetConstantPosition (nodes.Get(8), 30.0, 10.0);
  anim.SetConstantPosition (nodes.Get(9), 40.0, 10.0);
  anim.SetConstantPosition (nodes.Get(10), 30.0, 50.0);
  anim.SetConstantPosition (nodes.Get(11), 40.0, 50.0);
  // -------------- Print the routing table ----------------
  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>
  (dir + "exp2_routing_table.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (0), routingStream);

  // --------------- Store Recorded .dat --------------------
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10.1));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}
