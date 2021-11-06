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
 */

// This is a simple DASH streaming demo over TCP.
// The simulation consists of a single client and a single server with 
// a point-to-point link between them.
//
//  n1 (client)                 n2 (server)
//   |                           |
//   +---------------------------+
//    point-to-point connection


#include <fstream>
#include <sys/stat.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-stream-helper.h"
#include "ns3/tcp-stream-interface.h"
#include "ns3/quic-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleTcpStreaming");

// Create folder for client log files
void
createLoggingFolder(const std::string& adaptationAlgo, int simulationId) { 
  const char * mylogsDir = dashLogDirectory.c_str();
  mkdir (mylogsDir, 0775);

  std::string algodirstr (dashLogDirectory + adaptationAlgo);  
  const char * algodir = algodirstr.c_str();
  mkdir (algodir, 0775);
  
  std::string dirstr (algodirstr + "/" + std::to_string(simulationId) + "/");
  const char * dir = dirstr.c_str();
  mkdir(dir, 0775);
}

int
main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("SimpleTcpStreaming", LOG_LEVEL_LOGIC);
  LogComponentEnable ("TcpStreamClientApplication", LOG_LEVEL_LOGIC);
  LogComponentEnable ("TcpStreamServerApplication", LOG_LEVEL_LOGIC);
  LogComponentEnable ("QuicSocketBase", LOG_LEVEL_WARN);
  LogComponentEnable ("QuicStreamBase", LOG_LEVEL_DEBUG); // TODO make this warn level instead
  LogComponentEnable ("QuicStreamTxBuffer", LOG_LEVEL_WARN);
  LogComponentEnable ("QuicStreamRxBuffer", LOG_LEVEL_WARN);
  // LogComponentEnable ("QuicL5Protocol", LOG_LEVEL_INFO);
  
  // Command-line parameters
  uint64_t segmentDuration;
  uint32_t simulationId;
  std::string adaptationAlgo;
  std::string segmentSizeFilePath;

  CommandLine cmd;
  cmd.Usage ("Simulation of streaming with DASH over TCP.\n");
  cmd.AddValue ("simulationId", "The simulation's index (for logging purposes)", simulationId);
  cmd.AddValue ("segmentDuration", "The duration of a video segment in microseconds", segmentDuration);
  cmd.AddValue ("adaptationAlgo", "The adaptation algorithm that the client uses for the simulation", adaptationAlgo);
  cmd.AddValue ("segmentSizeFile", "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes", segmentSizeFilePath);
  cmd.Parse (argc, argv);

  createLoggingFolder(adaptationAlgo, simulationId);

  // TODO Set similar buffer size parameters for TCP and QUIC
  // Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  // Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (524288));
  // Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (524288));

  // Stolen from QUIC example.
  Config::SetDefault ("ns3::QuicSocketBase::SocketRcvBufSize", UintegerValue (524288));
  Config::SetDefault ("ns3::QuicSocketBase::SocketSndBufSize", UintegerValue (524288));
  Config::SetDefault ("ns3::QuicStreamBase::StreamSndBufSize", UintegerValue (524288));
  Config::SetDefault ("ns3::QuicStreamBase::StreamRcvBufSize", UintegerValue (524288));

  Time::SetResolution (Time::NS);

  // Two nodes, one for client and one for server
  NodeContainer nodes;
  nodes.Create (2);

  // A single p2p connection exists between the client and server
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps")); // Arbitrary; can be changed later.
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms")); // Arbitrary; can be changed later.

  NetDeviceContainer netDevices;
  netDevices = pointToPoint.Install (nodes);

  // Enable packet capture
  pointToPoint.EnablePcap("dash-tracing", nodes, true);

  // Install QUIC stack on client and server nodes
  QuicHelper stack;
  stack.InstallQuic (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (netDevices);

  // Set up the streaming server
  uint16_t serverPort {80};
  TcpStreamServerHelper serverHelper (serverPort);

  auto serverNode = nodes.Get(1);
  ApplicationContainer serverApp = serverHelper.Install (serverNode);
  serverApp.Start (Seconds (1.0));

  // Set up streaming client
  auto serverAddress = interfaces.GetAddress(1);
  TcpStreamClientHelper clientHelper (serverAddress, serverPort);

  clientHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
  clientHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
  clientHelper.SetAttribute ("NumberOfClients", UintegerValue (1));
  clientHelper.SetAttribute ("SimulationId", UintegerValue (simulationId));

  auto clientNode = nodes.Get(0);
  std::pair<Ptr<Node>, std::string> clientAlgoPair {clientNode, adaptationAlgo};
  ApplicationContainer clientApps = clientHelper.Install ({clientAlgoPair});
  clientApps.Get(0)->SetStartTime(Seconds(2)); // Only have one client application to start 

  NS_LOG_INFO ("Run Simulation. (" << "id: " << simulationId << ")");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation Complete.");

  return 0;
}
