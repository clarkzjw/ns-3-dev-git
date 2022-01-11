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

// This is a simple DASH streaming demo over QUIC or TCP.
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
#include "ns3/tcp-stream-interface.h"
#include "ns3/quic-module.h"
#include "ns3/stream-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DashStreaming");

// Create folder for client log files
std::string
createLoggingFolder(const std::string& adaptationAlgo, int simulationId) { 
  const char * mylogsDir = dashLogDirectory.c_str();
  mkdir (mylogsDir, 0775);

  std::string algodirstr (dashLogDirectory + adaptationAlgo);  
  const char * algodir = algodirstr.c_str();
  mkdir (algodir, 0775);
  
  std::string dirstr (algodirstr + "/" + std::to_string(simulationId) + "/");
  const char * dir = dirstr.c_str();
  mkdir(dir, 0775);

  return dirstr;
}

bool
isValidProtocol (std::string name) {
  for (char& c : name) {
    c = std::toupper (c);
  }

  return name == "QUIC" or name == "TCP";
}

int
main (int argc, char *argv[])
{
  // Logging setup
  LogComponentEnableAll (LOG_PREFIX_NODE);  // Prefix log messages with node id (0 for client, 1 for server)
  LogComponentEnableAll (LOG_PREFIX_FUNC);  // Prefix the function name before each log message
  LogComponentEnableAll (LOG_PREFIX_TIME);  // Prefix log messages with a timestamp
  LogComponentEnableAll (LOG_PREFIX_LEVEL); // Prefix log messages with their severity level
  // LogComponentEnableAll (LOG_PREFIX_ALL);

  LogComponentEnableAll (LOG_WARN);

  // LogComponentEnable ("DashStreaming", LOG_LEVEL_LOGIC);
  // LogComponentEnable ("StreamClientApplication", LOG_LEVEL_LOGIC);
  // LogComponentEnable ("StreamServerApplication", LOG_LEVEL_LOGIC);
  // LogComponentEnable ("QuicSocketBase", LOG_LEVEL_FUNCTION);
  // LogComponentEnable ("QuicSocketTxBuffer", LOG_LEVEL_INFO);
  // LogComponentEnable ("QuicCongestionControl", LOG_LEVEL_LOGIC);

  // LogComponentEnable ("QuicSocketTxScheduler", LOG_LEVEL_INFO);
  // // LogComponentEnable ("QuicSocketRxBuffer", LOG_LEVEL_INFO);
  // LogComponentEnable ("QuicStreamBase", LOG_LEVEL_INFO);
  // LogComponentEnable ("QuicStreamTxBuffer", LOG_LEVEL_INFO);
  // // LogComponentEnable ("QuicStreamRxBuffer", LOG_LEVEL_INFO);
  // // LogComponentEnable ("QuicL5Protocol", LOG_LEVEL_INFO);
  // // LogComponentEnable ("QuicL4Protocol", LOG_LEVEL_INFO);
  // LogComponentEnable ("QuicSubheader", LOG_LEVEL_INFO);
  
  // Hard-coded simulation parameters
  uint64_t segmentDuration {2000000};
  std::string segmentSizeFilePath {"contrib/dash/segmentSizes.txt"};

  // Command-line parameters
  uint32_t simulationId;
  std::string adaptationAlgo;
  std::string transportProtocol;
  bool pacingEnabled;
  std::string pacingRate;
  std::string dataRate;
  double errorRate;

  CommandLine cmd;
  cmd.Usage ("Simulation of streaming with DASH over QUIC.\n");
  cmd.AddValue ("simulationId", "The simulation's index (for logging purposes)", simulationId);
  cmd.AddValue ("adaptationAlgo", "The adaptation algorithm that the client uses for the simulation", adaptationAlgo);
  cmd.AddValue ("transportProtocol", "The transport protocol used for streaming (QUIC or TCP)", transportProtocol);
  cmd.AddValue ("dataRate", "The data rate of the link connecting the client and server. E.g. 1Mbps", dataRate);
  cmd.AddValue ("pacingEnabled", "true if pacing should be enabled. If enabled, pacing rate equals data rate.", pacingEnabled);
  cmd.AddValue ("errorRate", "The percentage of packets that should be lost, expressed as a double where 1 == 100%", errorRate);

  cmd.Parse (argc, argv);

  NS_ASSERT_MSG (isValidProtocol(transportProtocol), "Protocol '" << transportProtocol << "' is not supported.");

  NS_LOG_UNCOND("\n##### Simulation Config #####");
  NS_LOG_UNCOND("Simulation ID  : " << simulationId);
  NS_LOG_UNCOND("Protocol       : " << transportProtocol);
  NS_LOG_UNCOND("ABR Algorithm  : " << adaptationAlgo);
  NS_LOG_UNCOND("Data Rate      : " << dataRate);
  NS_LOG_UNCOND("Error Rate     : " << errorRate);
  NS_LOG_UNCOND("Pacing Enabled : " << (pacingEnabled ? "True" : "False"));
  NS_LOG_UNCOND("Segment File   : " << segmentSizeFilePath);
  NS_LOG_UNCOND("##### ##### ##### ##### #####\n");
  
  auto loggingFolder = createLoggingFolder(adaptationAlgo, simulationId);

  // Set similar buffer size parameters for TCP and QUIC
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (524288));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (524288));

  Config::SetDefault ("ns3::QuicSocketBase::MaxPacketSize", UintegerValue (1446)); // TODO Try making this smaller to see if we get less fragmentation?
  Config::SetDefault ("ns3::QuicSocketBase::SocketRcvBufSize", UintegerValue (524288));
  Config::SetDefault ("ns3::QuicSocketBase::SocketSndBufSize", UintegerValue (524288));
  Config::SetDefault ("ns3::QuicStreamBase::StreamSndBufSize", UintegerValue (524288));
  Config::SetDefault ("ns3::QuicStreamBase::StreamRcvBufSize", UintegerValue (524288));
  
  if (pacingEnabled) {
    Config::SetDefault ("ns3::TcpSocketState::EnablePacing", BooleanValue (true));    
  }

  Time::SetResolution (Time::NS);

  // Two nodes, one for client and one for server
  NodeContainer nodes;
  nodes.Create (2);

  // A single p2p connection exists between the client and server
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate)); 
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms")); // TODO make this a parameter

  NetDeviceContainer netDevices;
  netDevices = pointToPoint.Install (nodes);

  auto clientDevice = netDevices.Get(0);

  // Configure Error (loss) Rate
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetRate (errorRate);
  em->SetUnit(RateErrorModel::ERROR_UNIT_PACKET);
  clientDevice->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  // Enable packet capture
  std::string pcapPrefix = loggingFolder + "dash-tracing";
  pointToPoint.EnablePcap(pcapPrefix, nodes, true);

  // Install QUIC stack on client and server nodes
  QuicHelper stack;
  stack.InstallQuic (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (netDevices);

  // Set up the streaming server
  uint16_t serverPort {80};
  StreamServerHelper serverHelper (serverPort);

  serverHelper.SetAttribute ("TransportProtocol", StringValue (transportProtocol));

  auto serverNode = nodes.Get(1);
  ApplicationContainer serverApp = serverHelper.Install (serverNode);
  serverApp.Start (Seconds (1.0));

  // Set up streaming client
  auto serverAddress = interfaces.GetAddress(1);
  StreamClientHelper clientHelper (serverAddress, serverPort);

  clientHelper.SetAttribute ("TransportProtocol", StringValue (transportProtocol)); 
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
