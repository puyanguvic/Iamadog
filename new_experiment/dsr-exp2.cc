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
// Grid: 3x3,  5x5,  8x8,  10x10
// Random: 10, 20, 30, 40 
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
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
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
std::string dir = "results/Exp2/DSR/";
std::string ExpName = "DsrExperiment2";


// void
// CheckQueueSize (Ptr<QueueDiscClass> queueClass, std::string qID)
// {
//   uint32_t qSize = queueClass->GetQueueDisc()->GetCurrentSize ().GetValue ();
//   // Check queue size every 1/100 of a second
//   Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queueClass, qID);
//   std::ofstream fPlotQueue (std::stringstream (dir + "/queue-size-" + qID + ".dat").str ().c_str (), std::ios::out | std::ios::app);
//   fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
//   fPlotQueue.close ();
// }


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

  // Set up some default values for the simulation.  Use the 
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1000));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("5Mb/s"));
  Config::SetDefault ("ns3::FifoQueueDisc::MaxSize", QueueSizeValue (QueueSize ("35p")));
  //DefaultValue::Bind ("DropTailQueue::m_maxPackets", 30);
  double BeginTime = 0.0;
  double StopTime = 10.0; // Simulation stop time

  // Allow the user to override any of the defaults and the above
  // DefaultValue::Bind ()s at run-time, via command-line arguments
  CommandLine cmd (__FILE__);
  bool enableFlowMonitor = false;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  // ----------------create the channels -----------------------------
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  std::string channelDataRate1 = "200Mbps"; // link data rate (High)
  uint32_t delayInMicro1 = 3000; // transmission + propagation delay (Long)
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (channelDataRate1)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delayInMicro1)));


  // ------------------ build grid topology ---------------------------
  NS_LOG_INFO ("Create nodes.");
  uint32_t nRows = 3;
  uint32_t nCols = 3;
  
  PointToPointGridHelper grid (nRows, nCols, p2p);


  // ------------------- install internet protocol to nodes-----------
  InternetStackHelper internet;
  grid.InstallStack (internet);
  
  // ------------------- install dsr-queue -----------------------------
  TrafficControlHelper tch1;
  tch1.SetRootQueueDisc ("ns3::DsrVirtualQueueDisc");


  for (uint32_t i = 0; i < nRows; i++ )
    {
      for (uint32_t j = 0; j < nCols; j++ )
        {
          std::cout <<  "Number of netDevice at " << grid.GetNode (i, j)<< " is " << grid.GetNode (i, j)->GetNDevices()<< std::endl;
          for (uint32_t k=0; k< grid.GetNode (i, j)->GetNDevices(); k++)
            {
              tch1.Install (grid.GetNode (i, j)->GetDevice(k));
            }
        }
    }


  // std::vector<QueueDiscContainer> qdisc(12);
  // for (int i = 0; i < 12; i ++)
  // {
  //   // tch.Uninstall (devices[i]);
  //   qdisc[i] = tch1.Install (devices[i]);
  // }

  // ------------------- IP addresses AND Link Metric ----------------------
  NS_LOG_INFO ("Assign IP Addresses.");
  grid.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"));
  

  // ---------------- Create routingTable ---------------------
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // // ---------------- Schedule Recording Events ---------------
  // // Create directories to store dat files
  // struct stat buffer;
  // int retVal;
  // if ((stat (dir.c_str (), &buffer)) == 0)
  //   {
  //     std::string dirToRemove = "rm -rf " + dir;
  //     retVal = system (dirToRemove.c_str ());
  //     NS_ASSERT_MSG (retVal == 0, "Error in return value");
  //   }
  // std::string dirToSave = "mkdir -p " + dir;
  // retVal = system (dirToSave.c_str ());
  // NS_ASSERT_MSG (retVal == 0, "Error in return value");
  // retVal = system ((dirToSave + "/queueTraces/").c_str ());
  // NS_ASSERT_MSG (retVal == 0, "Error in return value");
  // NS_UNUSED (retVal);

  // // Calls function to check queue size
  // std::string qID1f = "n1n2-0";
  // Simulator::ScheduleNow (&CheckQueueSize, qdisc[7].Get (1)->GetQueueDiscClass(0), qID1f);
  // std::string qID1s = "n1n2-1";
  // Simulator::ScheduleNow (&CheckQueueSize, qdisc[7].Get (1)->GetQueueDiscClass(1), qID1s);
  // std::string qID1n = "n1n2-2";
  // Simulator::ScheduleNow (&CheckQueueSize, qdisc[7].Get (1)->GetQueueDiscClass(2), qID1n);

  // // std::string qID2f = "n1n4-0";
  // // Simulator::ScheduleNow (&CheckQueueSize, qdiscs9.Get (0)->GetQueueDiscClass(0), qID2f);
  // // std::string qID2s = "n1n4-1";
  // // Simulator::ScheduleNow (&CheckQueueSize, qdiscs9.Get (0)->GetQueueDiscClass(1), qID2s);
  // // std::string qID2n = "n1n4-2";
  // // Simulator::ScheduleNow (&CheckQueueSize, qdiscs9.Get (0)->GetQueueDiscClass(2), qID2n);

  // // Create .dat to store packets dropped and marked at the router
  // AsciiTraceHelper asciiTraceHelper;
  // Ptr<OutputStreamWrapper> streamWrapper;

  // streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n4n7.dat");
  // qdisc[7].Get(1)->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));
  // // streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n1n4.dat");
  // // qdiscs9.Get(0)-> TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));
  // // streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-n4n7.dat");
  // // qdiscs10.Get(0)-> TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));

  // // Store queue stats in a file
  // std::ofstream myfile;
  // myfile.open (dir + "/queueTraces/queueStats.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  // myfile << std::endl;
  // myfile << "Stat for Queue 0";
  // myfile << qdisc[7].Get(0)->GetStats ();
  // myfile.close ();


  // ------------------------------------*** DSR Traffic Target Flow (n2 --> n6)-------------------------------------  
   //Create a dsrSink applications 
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (grid.GetIpv4Address (6, 6), sinkPort));
  InstallPacketSink (grid.GetNode (6, 6), sinkPort, "ns3::UdpSocketFactory", BeginTime, StopTime);

  // create a dsrSender application
  uint32_t PacketSize = 1024;
  uint32_t NPacket = 1;
  uint32_t budget = 30;
  InstallDGPacketSend (grid.GetNode (6, 6), sinkAddress, 0, 1, PacketSize, NPacket, budget, 1, true);
  // for (int i=1; i<11; i++)
  // {
  //   InstallDGPacketSend (grid.GetNode (6, 6), sinkAddress, i-1, i, PacketSize, NPacket, budget, i*10, true);
  // }


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



  // // ---------------------------------------Network UDP Traffic C1 (n1-->n7) ------------------

  // //Create a dsrSink applications 
  // Address sinkAddress_1 (InetSocketAddress (i7i8.GetAddress (0), sinkPort));
  // InstallPacketSink (nodes.Get (7), sinkPort, "ns3::UdpSocketFactory", 0.0, 10.0);
  // // create a dsrSender application
  // uint32_t budget_1 = 20;
  // InstallDGPacketSend (nodes.Get (1), sinkAddress_1, 0.0, 2.0, Packetsize, NPacket, budget_1, "2Mbps", false);

  // // create a dsrSender application
  // uint32_t budget_11 = 14;
  // InstallDGPacketSend (nodes.Get (1), sinkAddress_1, 0.2, 1.2, Packetsize, NPacket, budget_11, "1Mbps", false);

  // // create a dsrSender application
  // uint32_t budget_12 = 13;
  // InstallDGPacketSend (nodes.Get (1), sinkAddress_1, 0.6, 1.6, Packetsize, NPacket, budget_12, "0.5Mbps", false);


  // // // ---------------------------------------Network Traffic C2 (n2-->n0) ------------------
  // //Create a dsrSink applications 
  // Address sinkAddress_2 (InetSocketAddress (i0i1.GetAddress (0), sinkPort));
  // InstallPacketSink (nodes.Get (0), sinkPort, "ns3::UdpSocketFactory", 0.0, 10.0);
  // // create a dsrSender application
  // uint32_t budget_2 = 14;
  // InstallDGPacketSend (nodes.Get (2), sinkAddress_2, 0.0, 2.0, Packetsize, NPacket, budget_2, "3Mbps", false);

  // // create a dsrSender application
  // uint32_t budget_21 = 18;
  // InstallDGPacketSend (nodes.Get (2), sinkAddress_2, 0.3, 0.8, Packetsize, NPacket, budget_21, "2Mbps", false);

  // // create a dsrSender application
  // uint32_t budget_22 = 20;
  // InstallDGPacketSend (nodes.Get (2), sinkAddress_2, 0.5, 1.3, Packetsize, NPacket, budget_22, "1Mbps", false);

  // // create a dsrSender application
  // uint32_t budget_23 = 13;
  // InstallDGPacketSend (nodes.Get (2), sinkAddress_2, 0.8, 1.6, Packetsize, NPacket, budget_23, "2Mbps", false);

  // // ------------------------------------------------------------------


  // ---------------- Net Anim ---------------------
  AnimationInterface anim(ExpName + ".xml");

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
