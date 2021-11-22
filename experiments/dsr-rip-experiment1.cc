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
std::string dir = "Ctrl_results/";
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


// // Function to calculate drops in a particular Queue
static void
DropAtQueue (Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " 1" << std::endl;
}


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


NS_LOG_COMPONENT_DEFINE ("DsrRipExperiment1");

int 
main (int argc, char *argv[])
{
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
#if 0 
  LogComponentEnable ("DsrRipxperiment1", LOG_LEVEL_INFO);
#endif

  // Set up some default values for the simulation.  Use the 
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1000));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("5Mb/s"));
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("35p")));
  //DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

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
  Ipv4ListRoutingHelper listRH;
  RipHelper ripRouting;
  listRH.Add (ripRouting, 0);
  Ipv4GlobalRoutingHelper globalRH;
  listRH.Add (globalRH, 1);

  InternetStackHelper internet;
  internet.SetIpv6StackInstall (false);
  internet.SetRoutingHelper (listRH);
  internet.Install (nodes);

  // ----------------create the channels -----------------------------
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;  
  // High capacity link
  std::string channelDataRate = "20Mbps"; // link data rate
  uint32_t delayInMicro = 5000; // transmission + propagation delay
  double StopTime = 5.0; // Simulation stop time

  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro)));
  NetDeviceContainer d2d5 = p2p.Install (n2n5);
  NetDeviceContainer d1d4 = p2p.Install (n1n4);
  NetDeviceContainer d4d7 = p2p.Install (n4n7);
  NetDeviceContainer d7d8 = p2p.Install (n7n8);

  // Medium capacity link
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro)));
  NetDeviceContainer d0d3 = p2p.Install (n0n3);
  NetDeviceContainer d1d2 = p2p.Install (n1n2);
  NetDeviceContainer d6d7 = p2p.Install (n6n7);
  NetDeviceContainer d3d4 = p2p.Install (n3n4);

  // Low capacity link
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro)));
  NetDeviceContainer d0d1 = p2p.Install (n0n1);
  NetDeviceContainer d4d5 = p2p.Install (n4n5);
  NetDeviceContainer d3d6 = p2p.Install (n3n6);
  NetDeviceContainer d5d8 = p2p.Install (n5n8);

  // ------------------- install dsr-queue -----------------------------
  TrafficControlHelper tch1;
  uint16_t handle1 = tch1.SetRootQueueDisc ("ns3::DsrPrioQueueDisc", "DsrPriomap", StringValue("0 1 2"));
  TrafficControlHelper::ClassIdList cid1 = tch1.AddQueueDiscClasses (handle1, 3, "ns3::QueueDiscClass");
  tch1.AddChildQueueDisc (handle1, cid1[0], "ns3::FifoQueueDisc", "MaxSize", StringValue("35p"));
  tch1.AddChildQueueDisc (handle1, cid1[1], "ns3::FifoQueueDisc", "MaxSize", StringValue("120p"));
  tch1.AddChildQueueDisc (handle1, cid1[2], "ns3::FifoQueueDisc", "MaxSize", StringValue("2000p"));

  TrafficControlHelper tch2;
  uint16_t handle2 = tch2.SetRootQueueDisc ("ns3::DsrPrioQueueDisc", "DsrPriomap", StringValue("0 1 2"));
  TrafficControlHelper::ClassIdList cid2 = tch2.AddQueueDiscClasses (handle2, 3, "ns3::QueueDiscClass");
  tch2.AddChildQueueDisc (handle2, cid2[0], "ns3::FifoQueueDisc", "MaxSize", StringValue("55p"));
  tch2.AddChildQueueDisc (handle2, cid2[1], "ns3::FifoQueueDisc", "MaxSize", StringValue("180p"));
  tch2.AddChildQueueDisc (handle2, cid2[2], "ns3::FifoQueueDisc", "MaxSize", StringValue("2000p"));

  TrafficControlHelper tch3;
  uint16_t handle3 = tch3.SetRootQueueDisc ("ns3::DsrPrioQueueDisc", "DsrPriomap", StringValue("0 1 2"));
  TrafficControlHelper::ClassIdList cid3 = tch3.AddQueueDiscClasses (handle3, 3, "ns3::QueueDiscClass");
  tch3.AddChildQueueDisc (handle3, cid3[0], "ns3::FifoQueueDisc", "MaxSize", StringValue("70p"));
  tch3.AddChildQueueDisc (handle3, cid3[1], "ns3::FifoQueueDisc", "MaxSize", StringValue("240p"));
  tch3.AddChildQueueDisc (handle3, cid3[2], "ns3::FifoQueueDisc", "MaxSize", StringValue("2000p"));

  


  QueueDiscContainer qdiscs1 = tch3.Install (d0d1);
  QueueDiscContainer qdiscs2 = tch2.Install (d1d2);
  QueueDiscContainer qdiscs3 = tch2.Install (d3d4);
  QueueDiscContainer qdiscs4 = tch3.Install (d4d5);
  QueueDiscContainer qdiscs5 = tch2.Install (d6d7);
  QueueDiscContainer qdiscs6 = tch1.Install (d7d8);

  QueueDiscContainer qdiscs7 = tch2.Install (d0d3);
  QueueDiscContainer qdiscs8 = tch3.Install (d3d6);
  QueueDiscContainer qdiscs9 = tch1.Install (d1d4);
  QueueDiscContainer qdiscs10 = tch1.Install (d4d7);
  QueueDiscContainer qdiscs11 = tch1.Install (d2d5);
  QueueDiscContainer qdiscs12 = tch3.Install (d5d8);

  // ------------------- IP addresses AND Link Metric ----------------------
  uint16_t hMetric = 6000;
  uint16_t mMetric = 6500;
  uint16_t lMetric = 7000;
  
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);
  i0i1.SetMetric (0, lMetric);
  i0i1.SetMetric (1, lMetric);

  ipv4.SetBase ("10.1.11.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i3 = ipv4.Assign (d0d3);
  i0i3.SetMetric (0, mMetric);
  i0i3.SetMetric (1, mMetric);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);
  i1i2.SetMetric (0, mMetric);
  i1i2.SetMetric (1, mMetric);

  ipv4.SetBase ("10.1.12.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i4 = ipv4.Assign (d1d4);
  i1i4.SetMetric (0, hMetric);
  i1i4.SetMetric (1, hMetric);

  ipv4.SetBase ("10.1.13.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i5 = ipv4.Assign (d2d5);
  i2i5.SetMetric (0, hMetric);
  i2i5.SetMetric (1, hMetric);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i4 = ipv4.Assign (d3d4);
  i3i4.SetMetric (0, mMetric);
  i3i4.SetMetric (1, mMetric);

  ipv4.SetBase ("10.1.14.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i6 = ipv4.Assign (d3d6);
  i3i6.SetMetric (0, lMetric);
  i3i6.SetMetric (1, lMetric);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i5 = ipv4.Assign (d4d5);
  i4i5.SetMetric (0, lMetric);
  i4i5.SetMetric (1, lMetric);

  ipv4.SetBase ("10.1.15.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i7 = ipv4.Assign (d4d7);
  i4i7.SetMetric (0, hMetric);
  i4i7.SetMetric (1, hMetric);

  ipv4.SetBase ("10.1.16.0", "255.255.255.0");
  Ipv4InterfaceContainer i5i8 = ipv4.Assign (d5d8); 
  i5i8.SetMetric (0, lMetric);
  i5i8.SetMetric (1, lMetric);

  ipv4.SetBase ("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer i6i7 = ipv4.Assign (d6d7);
  i6i7.SetMetric (0, mMetric);
  i6i7.SetMetric (1, mMetric);

  ipv4.SetBase ("10.1.8.0", "255.255.255.0");
  Ipv4InterfaceContainer i7i8 = ipv4.Assign (d7d8);
  i7i8.SetMetric (0, hMetric);
  i7i8.SetMetric (1, hMetric);

  // ---------------- Create routingTable ---------------------
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  // ------------------------------------*** DSR Traffic Target Flow (n2 --> n6)-------------------------------------  
   //Create a dsrSink applications 
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i6i7.GetAddress (0), sinkPort));
  InstallPacketSink (nodes.Get (6), sinkPort, "ns3::UdpSocketFactory", 0.0, 10.0);

  // create a dsrSender application
  uint32_t Packetsize = 1024;
  uint32_t NPacket = 1000;
  // uint32_t budget = 30;
  InstallBEPacketSend (nodes.Get (2), sinkAddress, 0.1, 0.6, Packetsize, NPacket, "2Mbps", true);
  InstallBEPacketSend (nodes.Get (2), sinkAddress, 0.6, 1.0, Packetsize, NPacket, "2.5Mbps", true);
  InstallBEPacketSend (nodes.Get (2), sinkAddress, 1.0, 1.6, Packetsize, NPacket, "3Mbps", true);
  InstallBEPacketSend (nodes.Get (2), sinkAddress, 1.6, 2.0, Packetsize, NPacket, "3.5Mbps", true);

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



  // ---------------------------------------Network UDP Traffic C1 (n1-->n7) ------------------

  //Create a dsrSink applications 
  Address sinkAddress_1 (InetSocketAddress (i7i8.GetAddress (0), sinkPort));
  InstallPacketSink (nodes.Get (7), sinkPort, "ns3::UdpSocketFactory", 0.0, 10.0);
  // create a dsrSender application
  uint32_t budget_1 = 20;
  InstallDGPacketSend (nodes.Get (1), sinkAddress_1, 0.0, 2.0, Packetsize, NPacket, budget_1, "2Mbps", false);

//   // create a dsrSender application
//   uint32_t budget_11 = 14;
//   InstallDGPacketSend (nodes.Get (1), sinkAddress_1, 0.2, 1.2, Packetsize, NPacket, budget_11, "1Mbps", false);

//   // create a dsrSender application
//   uint32_t budget_12 = 13;
//   InstallDGPacketSend (nodes.Get (1), sinkAddress_1, 0.6, 1.6, Packetsize, NPacket, budget_12, "0.5Mbps", false);


  // // ---------------------------------------Network Traffic C2 (n2-->n0) ------------------
  //Create a dsrSink applications 
  Address sinkAddress_2 (InetSocketAddress (i0i1.GetAddress (0), sinkPort));
  InstallPacketSink (nodes.Get (0), sinkPort, "ns3::UdpSocketFactory", 0.0, 10.0);
  // create a dsrSender application
  uint32_t budget_2 = 14;
  InstallDGPacketSend (nodes.Get (2), sinkAddress_2, 0.0, 2.0, Packetsize, NPacket, budget_2, "3Mbps", false);

//   // create a dsrSender application
//   uint32_t budget_21 = 18;
//   InstallDGPacketSend (nodes.Get (2), sinkAddress_2, 0.3, 0.8, Packetsize, NPacket, budget_21, "2Mbps", false);

//   // create a dsrSender application
//   uint32_t budget_22 = 20;
//   InstallDGPacketSend (nodes.Get (2), sinkAddress_2, 0.5, 1.3, Packetsize, NPacket, budget_22, "1Mbps", false);

//   // create a dsrSender application
//   uint32_t budget_23 = 13;
//   InstallDGPacketSend (nodes.Get (2), sinkAddress_2, 0.8, 1.6, Packetsize, NPacket, budget_23, "2Mbps", false);


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
  //  ------- n2 ---------------
  std::string qn2f = "n2-0";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs2.Get (1)->GetQueueDiscClass(0), qn2f);
  std::string qn2s = "n2-1";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs2.Get (1)->GetQueueDiscClass(1), qn2s);
  std::string qn2n = "n2-2";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs2.Get (1)->GetQueueDiscClass(2), qn2n);
  //  ------- n1 ---------------
  std::string qn1f = "n1-0";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs2.Get (0)->GetQueueDiscClass(0), qn1f);
  std::string qn1s = "n1-1";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs2.Get (0)->GetQueueDiscClass(1), qn1s);
  std::string qn1n = "n1-2";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs2.Get (0)->GetQueueDiscClass(2), qn1n);
  //  ------- n4 ---------------
  std::string qn4f = "n4-0";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs9.Get (0)->GetQueueDiscClass(0), qn4f);
  std::string qn4s = "n4-1";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs9.Get (0)->GetQueueDiscClass(1), qn4s);
  std::string qn4n = "n4-2";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs9.Get (0)->GetQueueDiscClass(2), qn4n);
  //  ------- n7 ---------------
  std::string qn7f = "n7-0";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs5.Get (1)->GetQueueDiscClass(0), qn7f);
  std::string qn7s = "n7-1";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs5.Get (1)->GetQueueDiscClass(1), qn7s);
  std::string qn7n = "n7-2";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs5.Get (1)->GetQueueDiscClass(2), qn7n);
  //  ------- n6 ---------------
  std::string qn6f = "n6-0";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs5.Get (0)->GetQueueDiscClass(0), qn6f);
  std::string qn6s = "n6-1";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs5.Get (0)->GetQueueDiscClass(1), qn6s);
  std::string qn6n = "n6-2";
  Simulator::ScheduleNow (&CheckQueueSize, qdiscs5.Get (0)->GetQueueDiscClass(2), qn6n);

  // Create dat to store packets dropped and marked at the router
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> streamWrapper;
  //  ------- n1n2 ---------------
  streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n1n2-0.dat");
  qdiscs2.Get(0)->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));
  streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n1n2-1.dat");
  qdiscs2.Get(1)->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));
  //  ------- n4 ---------------
  streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n1n4-1.dat");
  qdiscs9.Get(1)-> TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));
  //  ------- n7 ---------------
  streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n4n7-1.dat");
  qdiscs10.Get(1)-> TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));
  //  ------- n6 ---------------
  streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n6n7-0.dat");
  qdiscs10.Get(0)-> TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));


  // Store queue stats in a file
  std::ofstream myfile;
  myfile.open (dir + "/queueTraces/queueStats.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  myfile << std::endl;
  myfile << "Stat for Queue 0";
  myfile << qdiscs2.Get(0)->GetStats ();
  myfile.close ();

  // ---------------- Net Anim ---------------------
  AnimationInterface anim(dir + "dsr-experiment1-rip.xml");
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
  (dir + "dsr-experiment1.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (0), routingStream);

  // --------------- Store Recorded .dat --------------------
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (StopTime));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}
