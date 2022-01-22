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

  // ------------------- install internet protocol to all nodes-----------
  InternetStackHelper internet;
  internet.Install (nodes);

  // ----------------create the channels -----------------------------
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;  
  // High capacity link
  std::string channelDataRate = "10Mbps"; // link data rate
  uint32_t delayInMicro = 5000; // transmission + propagation delay

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
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("35p")));
  tch1.AddChildQueueDisc (handle1, cid1[0], "ns3::FifoQueueDisc");
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("120p")));
  tch1.AddChildQueueDisc (handle1, cid1[1], "ns3::FifoQueueDisc");
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("2000p")));
  tch1.AddChildQueueDisc (handle1, cid1[2], "ns3::FifoQueueDisc");

  TrafficControlHelper tch2;
  uint16_t handle2 = tch2.SetRootQueueDisc ("ns3::DsrPrioQueueDisc", "DsrPriomap", StringValue("0 1 2"));
  TrafficControlHelper::ClassIdList cid2 = tch2.AddQueueDiscClasses (handle2, 3, "ns3::QueueDiscClass");
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("55p")));
  tch2.AddChildQueueDisc (handle2, cid2[0], "ns3::FifoQueueDisc");
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("180p")));
  tch2.AddChildQueueDisc (handle2, cid2[1], "ns3::FifoQueueDisc");
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("2000p")));
  tch2.AddChildQueueDisc (handle2, cid2[2], "ns3::FifoQueueDisc");

  TrafficControlHelper tch3;
  uint16_t handle3 = tch3.SetRootQueueDisc ("ns3::DsrPrioQueueDisc", "DsrPriomap", StringValue("0 1 2"));
  TrafficControlHelper::ClassIdList cid3 = tch3.AddQueueDiscClasses (handle3, 3, "ns3::QueueDiscClass");
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("70p")));
  tch3.AddChildQueueDisc (handle3, cid3[0], "ns3::FifoQueueDisc");
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("240p")));
  tch3.AddChildQueueDisc (handle3, cid3[1], "ns3::FifoQueueDisc");
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("2000p")));
  tch3.AddChildQueueDisc (handle3, cid3[2], "ns3::FifoQueueDisc");

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

  // ---------------- Create routingTable---------------------
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  // //--------------- Create best effort applications Target Flow ------------------------------
  // // Create the OnOff application to send UDP datagrams of size
  // // 1000 bytes at a rate of 5 Mb/s
  // NS_LOG_INFO ("Create Applications.");
  // uint16_t port = 9;   // Discard port (RFC 863)
  // OnOffHelper onoff ("ns3::UdpSocketFactory", 
  //                    Address (InetSocketAddress (i1i2.GetAddress (1), port)));
  // onoff.SetConstantRate (DataRate ("5Mb/s"));
  // // Create a flow from n2 to n6, starting at time 1.1 seconds
  // onoff.SetAttribute ("Remote", 
  //                     AddressValue (InetSocketAddress (i6i7.GetAddress (0), port)));
  // ApplicationContainer apps = onoff.Install (nodes.Get (2));
  // apps.Start (Seconds (1.0));
  // apps.Stop (Seconds (10.0));

  // // Create a packet sink to receive these packets
  // PacketSinkHelper sink ("ns3::UdpSocketFactory",
  //                        Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  // apps = sink.Install (nodes.Get (6));
  // apps.Start (Seconds (1.0));
  // apps.Stop (Seconds (10.0));

  // // Create a onoff application
  // uint16_t port_1 = 9;   // Discard port (RFC 863)
  // OnOffHelper onoff_1 ("ns3::UdpSocketFactory", 
  //                    Address (InetSocketAddress (i0i1.GetAddress (0), port_1)));
  // onoff_1.SetConstantRate (DataRate ("3Mb/s"));
  // // Create a flow from n2 to n6, starting at time 1.1 seconds
  // onoff_1.SetAttribute ("Remote", 
  //                     AddressValue (InetSocketAddress (i7i8.GetAddress (1), port_1)));
  // ApplicationContainer apps_1 = onoff_1.Install (nodes.Get (2));
  // apps.Start (Seconds (1.0));
  // apps.Stop (Seconds (10.0));

  // // Create a bulk application
  // uint16_t port_2 = 9;
  // BulkSendHelper sourceHelper ("ns3::TcpSocketFactory",
  //                              InetSocketAddress (i4i5.GetAddress (0), port_2));
  // sourceHelper.SetAttribute ("MaxBytes", UintegerValue (300000));
  // sourceHelper.SetAttribute ("EnableSeqTsSizeHeader", BooleanValue (true));
  // ApplicationContainer sourceApp = sourceHelper.Install (nodes.Get (5));
  // sourceApp.Start (Seconds (0.0));
  // sourceApp.Stop (Seconds (10.0));


  // --------------- Create best effort applications Network Flow ------------------------------


  // ------------------Create DSR TCP application--------------------------------------------
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i0i1.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (10.));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  // ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  Ptr<DsrApplication> app = CreateObject<DsrApplication> ();
  uint32_t budget = 40;
  app->Setup (ns3TcpSocket, sinkAddress, 1040, 1000, DataRate ("1Mbps"), budget);
  nodes.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (1.));
  app->SetStopTime (Seconds (10.));

  // >>>????


  // // ------------------------------------DSR Traffic Target Flow (n2 --> n6)-------------------------------------  
  // //Create a dsrSink applications 
  // uint16_t sinkPort = 8080;
  // Address sinkAddress (InetSocketAddress (i6i7.GetAddress (0), sinkPort));
  // DsrSinkHelper dsrSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  // ApplicationContainer sinkApps = dsrSinkHelper.Install (nodes.Get (6));
  // sinkApps.Start (Seconds (0.0));
  // sinkApps.Stop (Seconds (1.0));

  // // create a DsrSender application 
  // Ptr<Socket> dsrUdpSocket = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  // uint32_t budget = 26;
  // Ptr<DsrApplication> dsrApp = CreateObject<DsrApplication> ();
  // dsrApp->Setup (dsrUdpSocket, sinkAddress, 1024, 100000, DataRate ("2Mbps"), budget);
  // nodes.Get (2)->AddApplication (dsrApp);
  // dsrApp->SetStartTime (Seconds (0.01));
  // dsrApp->SetStopTime (Seconds (0.16));


  // // ---------------------------------------Network Traffic C1 (n1-->n7) ------------------
  // // Create a DsrSender application 
  // Address sinkAddress_1 (InetSocketAddress (i7i8.GetAddress (0), sinkPort));
  // DsrSinkHelper dsrSinkHelper1 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  // ApplicationContainer sinkApps1 = dsrSinkHelper1.Install (nodes.Get (7));
  // sinkApps1.Start (Seconds (0.0));
  // sinkApps1.Stop (Seconds (1.0));

  // Ptr<Socket> dsrUdpSocket_1 = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());
  // uint32_t budget_1 = 16;
  // Ptr<DsrApplication> dsrApp_1 = CreateObject<DsrApplication> ();
  // dsrApp_1->Setup (dsrUdpSocket_1, sinkAddress_1, 1024, 100000, DataRate ("2Mbps"), budget_1);
  // nodes.Get (1)->AddApplication (dsrApp_1);
  // dsrApp_1->SetStartTime (Seconds (0.0));
  // dsrApp_1->SetStopTime (Seconds (0.1));

  //   // Create a DsrSender application 
  // Ptr<Socket> dsrUdpSocket_11 = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());
  // uint32_t budget_11 = 14;
  // Ptr<DsrApplication> dsrApp_11 = CreateObject<DsrApplication> ();
  // dsrApp_11->Setup (dsrUdpSocket_11, sinkAddress_1, 1024, 100000, DataRate ("1Mbps"), budget_11);
  // nodes.Get (1)->AddApplication (dsrApp_11);
  // dsrApp_11->SetStartTime (Seconds (0.05));
  // dsrApp_11->SetStopTime (Seconds (0.15));

  //   // Create a DsrSender application 
  // Ptr<Socket> dsrUdpSocket_12 = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());
  // uint32_t budget_12 = 13;
  // Ptr<DsrApplication> dsrApp_12 =CreateObject<DsrApplication> ();
  // dsrApp_12->Setup (dsrUdpSocket_12, sinkAddress_1, 1024, 100000, DataRate ("0.5Mbps"), budget_12);
  // nodes.Get (1)->AddApplication (dsrApp_12);
  // dsrApp_12->SetStartTime (Seconds (0.1));
  // dsrApp_12->SetStopTime (Seconds (0.2));


  // // ---------------------------------------Network Traffic C2 (n2-->n0) ------------------
  // // Create a DsrSender application 
  // Address sinkAddress_2 (InetSocketAddress (i0i1.GetAddress (0), sinkPort));
  // DsrSinkHelper dsrSinkHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  // ApplicationContainer sinkApps2 = dsrSinkHelper1.Install (nodes.Get (0));
  // sinkApps2.Start (Seconds (0.0));
  // sinkApps2.Stop (Seconds (1.0));

  // Ptr<Socket> dsrUdpSocket_2 = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  // uint32_t budget_2 = 14;
  // Ptr<DsrApplication> dsrApp_2 = CreateObject<DsrApplication> ();
  // dsrApp_2->Setup (dsrUdpSocket_2, sinkAddress_2, 1024, 100000, DataRate ("3Mbps"), budget_2);
  // nodes.Get (2)->AddApplication (dsrApp_2);
  // dsrApp_2->SetStartTime (Seconds (0.0));
  // dsrApp_2->SetStopTime (Seconds (0.2));

  // // Create a DsrSender application 
  // Ptr<Socket> dsrUdpSocket_21 = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  // uint32_t budget_21 = 14;
  // Ptr<DsrApplication> dsrApp_21 = CreateObject<DsrApplication> ();
  // dsrApp_21->Setup (dsrUdpSocket_21, sinkAddress_2, 1024, 100000, DataRate ("2Mbps"), budget_21);
  // nodes.Get (2)->AddApplication (dsrApp_21);
  // dsrApp_21->SetStartTime (Seconds (0.003));
  // dsrApp_21->SetStopTime (Seconds (0.023));


  // // Create a DsrSender application 
  // uint32_t budget_22 = 15;
  // Ptr<Socket> dsrUdpSocket_22 = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  // Ptr<DsrApplication> dsrApp_22 = CreateObject<DsrApplication> ();
  // dsrApp_22->Setup (dsrUdpSocket_22, sinkAddress_2, 1024, 100000, DataRate ("1Mbps"), budget_22);
  // nodes.Get (2)->AddApplication (dsrApp_22);
  // dsrApp_22->SetStartTime (Seconds (0.005));
  // dsrApp_22->SetStopTime (Seconds (0.173));


  // // Create a DsrSender appication 
  // uint32_t budget_23 = 13;
  // Ptr<Socket> dsrUdpSocket_23 = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  // Ptr<DsrApplication> dsrApp_23 = CreateObject<DsrApplication> ();
  // dsrApp_23->Setup (dsrUdpSocket_23, sinkAddress_2, 1024, 1000, DataRate ("2Mbps"), budget_23);
  // nodes.Get (2)->AddApplication (dsrApp_23);
  // dsrApp_23->SetStartTime (Seconds (0.15));
  // dsrApp_23->SetStopTime (Seconds (0.2));

  // ------------------------------------------------------------------


  // ---------------- Net Anim ---------------------
  AnimationInterface anim("dsr-experiment1.xml");
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
  ("dsr-experiment1.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (0), routingStream);


  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (1));
  Simulator::Run ();
  NS_LOG_INFO ("Done.");

  Simulator::Destroy ();
  return 0;
}
