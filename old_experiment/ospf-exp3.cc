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
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/dsr-tags-module.h"
#include "ns3/dsr-application-module.h"

using namespace ns3;  

// ============== Auxiliary functions ===============

// Function to check queue length of Routers
std::string dir = "results/Exp3/OSPF";
std::string ExpName = "OspfExperiment3";


void
CheckQueueSize (Ptr<QueueDiscClass> queueClass, std::string qID)
{
  uint32_t qSize = queueClass->GetQueueDisc()->GetCurrentSize ().GetValue ();
  // Check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queueClass, qID);
  std::ofstream fPlotQueue (std::stringstream (dir + "/queue-size-" + qID + ".dat").str ().c_str (), std::ios::out | std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue.close ();
}


// // Function to calculate drops in a particular Queue
static void
DropAtQueue (Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " 1" << std::endl;
}


// Function to install dsrSink application
void InstallPacketSink (Ptr<Node> node, uint16_t port, std::string socketFactory, double startTime, double stopTime)
{
  DsrSinkHelper sink (socketFactory, InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (node);
  sinkApps.Start (Seconds (startTime));
  sinkApps.Stop (Seconds (stopTime));
}


// Function to install dsrSend application with budget
void InstallDGPacketSend (Ptr<Node> node, Address sinkAddress, double startTime, double stopTime,
                        uint32_t packetSize, uint32_t nPacket, uint32_t budget, 
                        uint16_t dataRate, bool flag, bool TCP=false)
{
  if (TCP == true){}
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

  // Set up some default values for the simulation.  Use the 
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1000));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("5Mb/s"));
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("35p")));
  //DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);
  std::string channelDataRate1 = "200Mbps"; // link data rate (High)
  std::string channelDataRate2 = "100Mbps"; // link data rate (low)
  uint32_t delayInMicro1 = 3000; // transmission + propagation delay (Short)
  uint32_t delayInMicro2 = 5000; // transmission + propagation delay (Long)
  double BeginTime = 0.0;
  double StopTime = 10.0; // Simulation stop time

  // Allow the user to override any of the defaults and the above
  // DefaultValue::Bind ()s at run-time, via command-line arguments
  CommandLine cmd (__FILE__);
  bool enableFlowMonitor = false;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  // ------------------ build topology ---------------------------
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create(9);
  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
  NodeContainer n0n3 = NodeContainer(nodes.Get(0), nodes.Get(3));
  NodeContainer n3n4 = NodeContainer(nodes.Get(3), nodes.Get(4));
  NodeContainer n1n4 = NodeContainer(nodes.Get(1), nodes.Get(4));
  NodeContainer n4n5 = NodeContainer(nodes.Get(4), nodes.Get(5));
  NodeContainer n2n5 = NodeContainer(nodes.Get(2), nodes.Get(5));
  NodeContainer n3n6 = NodeContainer(nodes.Get(3), nodes.Get(6));
  NodeContainer n6n7 = NodeContainer(nodes.Get(6), nodes.Get(7));
  NodeContainer n4n7 = NodeContainer(nodes.Get(4), nodes.Get(7));
  NodeContainer n7n8 = NodeContainer(nodes.Get(7), nodes.Get(8));
  NodeContainer n5n8 = NodeContainer(nodes.Get(5), nodes.Get(8));

  // ------------------- install internet protocol to nodes-----------
  InternetStackHelper internet;
  internet.Install (nodes);

  // ----------------create the channels -----------------------------
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p; 
  // PointToPointHelper p2p2; 
  // PointToPointHelper p2p3; 
  // PointToPointHelper p2p4; 
  std::vector<NetDeviceContainer> devices(12);


  // High capacity link + Short base delay
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate1)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro1)));
  devices[10] = p2p.Install (n2n5);
  devices[5] = p2p.Install (n1n4);
  devices[9] = p2p.Install (n7n8);
  devices[1] = p2p.Install (n3n6);
  devices[6] = p2p.Install (n4n7);

  // High capacity link + Long base delay
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate1)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro2)));
  devices[2] = p2p.Install (n0n1);
  devices[8] = p2p.Install (n4n5);


  // Low capacity link + Short base delay
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate2)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro1)));
  devices[0] = p2p.Install (n0n3);
  devices[7] = p2p.Install (n1n2);



  // Low capacity link + Long base delay
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate2)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro2)));
  devices[4] = p2p.Install (n6n7);
  devices[3] = p2p.Install (n3n4);
  devices[11] = p2p.Install (n5n8);

  // ------------------- install normal-queue -----------------------------
  // Change back to single queue
  TrafficControlHelper tch1;
  uint16_t handle1 = tch1.SetRootQueueDisc ("ns3::DsrPrioQueueDisc");

  
  std::vector<QueueDiscContainer> qdisc(12);
  for (int i = 0; i < 12; i ++)
  {
    // tch.Uninstall (devices[i]);
    qdisc[i] = tch1.Install (devices[i]);
  }

  // ------------------- IP addresses AND Link Metric ----------------------
  
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> interface(12);
  for (int i=0; i < 12; i ++)
  {
    std::ostringstream subset;
    subset<<"10.1."<<i+1<<".0";
    ipv4.SetBase (subset.str().c_str(), "255.255.255.0");
    interface[i] = ipv4.Assign (devices[i]);
  }

  // ---------------- Create routingTable ---------------------
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

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

  // Calls function to check queue size
  std::string qID1f = "n1n2-0";
  Simulator::ScheduleNow (&CheckQueueSize, qdisc[7].Get (1)->GetQueueDiscClass(0), qID1f);
  std::string qID1s = "n1n2-1";
  Simulator::ScheduleNow (&CheckQueueSize, qdisc[7].Get (1)->GetQueueDiscClass(1), qID1s);
  std::string qID1n = "n1n2-2";
  Simulator::ScheduleNow (&CheckQueueSize, qdisc[7].Get (1)->GetQueueDiscClass(2), qID1n);

  // std::string qID2f = "n1n4-0";
  // Simulator::ScheduleNow (&CheckQueueSize, qdiscs9.Get (0)->GetQueueDiscClass(0), qID2f);
  // std::string qID2s = "n1n4-1";
  // Simulator::ScheduleNow (&CheckQueueSize, qdiscs9.Get (0)->GetQueueDiscClass(1), qID2s);
  // std::string qID2n = "n1n4-2";
  // Simulator::ScheduleNow (&CheckQueueSize, qdiscs9.Get (0)->GetQueueDiscClass(2), qID2n);

  // Create .dat to store packets dropped and marked at the router
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> streamWrapper;

  streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n4n7.dat");
  qdisc[7].Get(1)->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));
  // streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n1n4.dat");
  // qdiscs9.Get(0)-> TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));
  // streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n4n7.dat");
  // qdiscs10.Get(0)-> TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));

  // Store queue stats in a file
  std::ofstream myfile;
  myfile.open (dir + "/queueTraces/queueStats.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  myfile << std::endl;
  myfile << "Stat for Queue 0";
  myfile << qdisc[7].Get(0)->GetStats ();
  myfile.close ();


  // ------------------------------------*** Traffic Target Flow (n2 --> n6)-------------------------------------  
   //Create a dsrSink applications 
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interface[4].GetAddress (0), sinkPort));
  InstallPacketSink (nodes.Get (6), sinkPort, "ns3::UdpSocketFactory", BeginTime, StopTime);

  // create a dsrSender application
  uint32_t Budget = 40;
  uint32_t PacketSize = 1024;
  uint32_t NPacket = 1000;
  for (int i=0; i<100; i+=10)
  {
    InstallDGPacketSend (nodes.Get(0), sinkAddress, i, i+10, PacketSize, NPacket, Budget-i*2, i*10, true);
  }
  


// ------------------------ Network DSR TCP application--------------------------------------------
  // uint16_t sinkPort = 8080;
  // Address sinkAddress (InetSocketAddress (i0i1.GetAddress (1), sinkPort));
  // PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  // ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  // sinkApps.Start (Seconds (0.));
  // sinkApps.Stop (Seconds (10.));

  // Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  // // ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  // Ptr<DsrApplication> app = CreateObject<DsrApplication> ();
  // uint32_t budget = 40;
  // app->Setup (ns3TcpSocket, sinkAddress, 1040, 1000, DataRate ("1Mbps"), budget);
  // nodes.Get (0)->AddApplication (app);
  // app->SetStartTime (Seconds (1.));
  // app->SetStopTime (Seconds (10.));



  // ---------------------------------------Network UDP Traffic C1 (n0-->n8) ------------------

  //Create a dsrSink applications 
  Address sinkAddress_1 (InetSocketAddress (interface[11].GetAddress (1), sinkPort));
  InstallPacketSink (nodes.Get (7), sinkPort, "ns3::UdpSocketFactory", 0.0, 10.0);
  // create a dsrSender application
  uint32_t PacketSize1 = 1024;
  uint32_t NPacket1 = 1000;
  for (int i=0; i<100; i+=10)
  {
    InstallBEPacketSend (nodes.Get(0), sinkAddress_1, i, i+10, PacketSize1, NPacket1, 20, false);
  }
  



  // // // ---------------------------------------Network Traffic C2 (n1-->n6) ------------------
  // //Create a dsrSink applications 
  // Address sinkAddress_2 (InetSocketAddress (interface[4].GetAddress (1), sinkPort));
  // InstallPacketSink (nodes.Get (6), sinkPort, "ns3::UdpSocketFactory", 0.0, 10.0);
  // // create a dsrSender application
  // uint32_t PacketSize2 = 1024;
  // uint32_t NPacket2 = 1000;
  // for (int i=0; i<100; i+=10)
  // {
  //   InstallBEPacketSend (nodes.Get(0), sinkAddress, i, i+10, PacketSize2, NPacket2, 20, false);
  // }
  
  

  // // ------------------------------------------------------------------


  // ---------------- Net Anim ---------------------
  AnimationInterface anim(ExpName + ".xml");
  anim.SetConstantPosition (nodes.Get(0), 0.0, 100.0);
  anim.SetConstantPosition (nodes.Get(1), 0.0, 110.0);
  anim.SetConstantPosition (nodes.Get(2), 0.0, 120.0);
  anim.SetConstantPosition (nodes.Get(3), 10.0, 100.0);
  anim.SetConstantPosition (nodes.Get(4), 10.0, 110.0);
  anim.SetConstantPosition (nodes.Get(5), 10.0, 120.0);
  anim.SetConstantPosition (nodes.Get(6), 20.0, 100.0);
  anim.SetConstantPosition (nodes.Get(7), 20.0, 110.0);
  anim.SetConstantPosition (nodes.Get(8), 20.0, 120.0);

  // -------------- Print the routing table ----------------
  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>
  (ExpName + ".routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (0), routingStream);

  // --------------- Store Recorded .dat --------------------
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (StopTime));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}
