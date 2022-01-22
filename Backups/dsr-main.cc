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
//    n0 --m-- n3 --s-- n6 
//    |s       |m       |m
//    n1 --h-- n4 --h-- n7
//    |m       |s       |h
//    n2 --h-- n5 --s-- n8
// 
//  h - high capacity link (200Mbps) 
//  m - medium capcity link (200Mbps)
//  s - low capacity link (200Mbps)
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
#include "ns3/mobility-module.h"
#include "ns3/SimpleUdpAppplication-module.h"
//#include "ns3/timestampTag-module.h"
//#include "ns3/timestampTag-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleGlobalRoutingExample");

int 
main (int argc, char *argv[])
{
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
#if 0 
  LogComponentEnable ("SimpleGlobalRoutingExample", LOG_LEVEL_INFO);
#endif

  // Set up some default values for the simulation.  Use the 
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1000));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("448kb/s"));

  //DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);

  // Allow the user to override any of the defaults and the above
  // DefaultValue::Bind ()s at run-time, via command-line arguments
  CommandLine cmd (__FILE__);
  bool enableFlowMonitor = true;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  // Here, we will explicitly create four nodes.  In more sophisticated
  // topologies, we could configure a node factory.
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;

  // create an array of empty nodes for testing purposes 
  // nodes.Create (9);

  // MobilityHelper mobility;
  // // setup the grid itself: objects are laid out
  // // started from (-10,-10) with 3 objects per row, 
  // // the x interval between each object is 6 meters
  // // and the y interval between each object is 6 meters
  // mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
  //                                "MinX", DoubleValue (-10.0),
  //                                "MinY", DoubleValue (-10.0),
  //                                "DeltaX", DoubleValue (6.0),
  //                                "DeltaY", DoubleValue (6.0),
  //                                "GridWidth", UintegerValue (6),
  //                                "LayoutType", StringValue ("RowFirst"));
  // // each object will be attached a static position.
  // // i.e., once set by the "position allocator", the
  // // position will never change.
  // mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  // // finalize the setup by attaching to each object
  // // in the input array a position and initializing
  // // this position with the calculated coordinates.
  // mobility.Install (nodes);

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

  InternetStackHelper internet;
  internet.Install (nodes);

  // We create the channels first without any IP addressing information
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;  
  // High capacity link
  std::string channelDataRate = "200Mbps"; // link data rate
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

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::DsrPrioQueueDisc", "DsrPriomap", StringValue("0 1 2"));
  TrafficControlHelper::ClassIdList cid = tch.AddQueueDiscClasses (handle, 3, "ns3::QueueDiscClass");
  tch.AddChildQueueDisc (handle, cid[0], "ns3::FifoQueueDisc");
  tch.AddChildQueueDisc (handle, cid[1], "ns3::FifoQueueDisc");
  tch.AddChildQueueDisc (handle, cid[2], "ns3::FifoQueueDisc");

  QueueDiscContainer qdiscs1 = tch.Install (d0d1);
  QueueDiscContainer qdiscs2 = tch.Install (d1d2);
  QueueDiscContainer qdiscs3 = tch.Install (d3d4);
  QueueDiscContainer qdiscs4 = tch.Install (d4d5);
  QueueDiscContainer qdiscs5 = tch.Install (d6d7);
  QueueDiscContainer qdiscs6 = tch.Install (d7d8);

  QueueDiscContainer qdiscs7 = tch.Install (d0d3);
  QueueDiscContainer qdiscs8 = tch.Install (d3d6);
  QueueDiscContainer qdiscs9 = tch.Install (d1d4);
  QueueDiscContainer qdiscs10 = tch.Install (d4d7);
  QueueDiscContainer qdiscs11 = tch.Install (d2d5);
  QueueDiscContainer qdiscs12 = tch.Install (d5d8);


  // Later, we add IP addresses.
  NS_LOG_INFO ("Assign IP Addresses.");
  
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);
  i0i1.SetMetric (0, 7000);
  i0i1.SetMetric (1, 7000);

  ipv4.SetBase ("10.1.11.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i3 = ipv4.Assign (d0d3);
  i0i3.SetMetric (0, 6500);
  i0i3.SetMetric (1, 6500);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);
  i1i2.SetMetric (0, 6500);
  i1i2.SetMetric (1, 6500);

  ipv4.SetBase ("10.1.12.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i4 = ipv4.Assign (d1d4);
  i1i4.SetMetric (0, 6000);
  i1i4.SetMetric (1, 6000);

  ipv4.SetBase ("10.1.13.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i5 = ipv4.Assign (d2d5);
  i2i5.SetMetric (0, 6000);
  i2i5.SetMetric (1, 6000);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i4 = ipv4.Assign (d3d4);
  i3i4.SetMetric (0, 6500);
  i3i4.SetMetric (1, 6500);

  ipv4.SetBase ("10.1.14.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i6 = ipv4.Assign (d3d6);
  i3i6.SetMetric (0, 7000);
  i3i6.SetMetric (1, 7000);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i5 = ipv4.Assign (d4d5);
  i4i5.SetMetric (0, 7000);
  i4i5.SetMetric (1, 7000);

  ipv4.SetBase ("10.1.15.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i7 = ipv4.Assign (d4d7);
  i4i7.SetMetric (0, 6000);
  i4i7.SetMetric (1, 6000);

  ipv4.SetBase ("10.1.16.0", "255.255.255.0");
  Ipv4InterfaceContainer i5i8 = ipv4.Assign (d5d8); 
  i5i8.SetMetric (0, 7000);
  i5i8.SetMetric (1, 7000);

  ipv4.SetBase ("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer i6i7 = ipv4.Assign (d6d7);
  i6i7.SetMetric (0, 6500);
  i6i7.SetMetric (1, 6500);

  ipv4.SetBase ("10.1.8.0", "255.255.255.0");
  Ipv4InterfaceContainer i7i8 = ipv4.Assign (d7d8);
  i7i8.SetMetric (0, 6000);
  i7i8.SetMetric (1, 6000);

  // Create router nodes, initialize routing database and set up the routing
  // tables in the nodes.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  // Create the OnOff application to send UDP datagrams of size
  // 1000 bytes at a rate of 448 Kb/s
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9;   // Discard port (RFC 863)
  OnOffHelper onoff ("ns3::UdpSocketFactory", 
                     Address (InetSocketAddress (i1i2.GetAddress (1), port)));
  onoff.SetConstantRate (DataRate ("448kb/s"));
  ApplicationContainer apps = onoff.Install (nodes.Get (2));

  // Create a flow from n2 to n6, starting at time 1.1 seconds
  onoff.SetAttribute ("Remote", 
                      AddressValue (InetSocketAddress (i6i7.GetAddress (0), port)));
  apps = onoff.Install (nodes.Get (2));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Create a packet sink to receive these packets
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  apps = sink.Install (nodes.Get (6));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));
 
  
  // // Create DSR applications to send UDP datagrams
  // Ptr <SimpleUdpApplication> udp0 = CreateObject <SimpleUdpApplication> ();

  // // Set the application at node n2
  // nodes.Get(2)->AddApplication (udp0);
  // udp0->SetStartTime (Seconds(0));
  // udp0->SetStopTime (Seconds(10));

  // // Set destination node IP (node 6)
  // Ipv4Address dest_ip ("10.1.6.1");

  // // Schedule an event to send a DSR packet of size 1000B to node 6
  // Ptr <Packet> packet1 = Create <Packet> (1000);
  // TimestampTag deadline;
  // deadline.SetTimestamp(Seconds(1.1));
  // packet1->AddByteTag(deadline);
  // Simulator::Schedule (Seconds (0), &SimpleUdpApplication::SendPacket, udp0, packet1, dest_ip, 7777);

  // LogComponentEnable("Dsr-test", LOG_LEVEL_INFO);

  

  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("dsr-test.tr"));
  // p2p.EnablePcapAll ("simple-global-routing");

  // Flow Monitor
  FlowMonitorHelper flowmonHelper;
  if (enableFlowMonitor)
    {
      flowmonHelper.InstallAll ();
    }

  AnimationInterface anim("dsr-test.xml");
  anim.SetConstantPosition (nodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition (nodes.Get(1), 0.0, 30.0);
  anim.SetConstantPosition (nodes.Get(2), 0.0, 60.0);
  anim.SetConstantPosition (nodes.Get(3), 30.0, 0.0);
  anim.SetConstantPosition (nodes.Get(4), 30.0, 30.0);
  anim.SetConstantPosition (nodes.Get(5), 30.0, 60.0);
  anim.SetConstantPosition (nodes.Get(6), 60.0, 0.0);
  anim.SetConstantPosition (nodes.Get(7), 60.0, 30.0);
  anim.SetConstantPosition (nodes.Get(8), 60.0, 60.0);
  // anim.SetConstantPosition (nodes.Get(0), 0.0, 60.0);


  // Trace routing tables
  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>
  ("dsr-routing.routes", std::ios::out);
  g.PrintRoutingTableAllAt (Seconds (1), routingStream);


  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (11));
  Simulator::Run ();
  NS_LOG_INFO ("Done.");

  if (enableFlowMonitor)
    {
      flowmonHelper.SerializeToXmlFile ("dsr-test-flowmon.xml", false, false);
    }

  Simulator::Destroy ();
  return 0;
}
