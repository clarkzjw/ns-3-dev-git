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

// This is a simple DASH streaming demo over QUIC.
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

NS_LOG_COMPONENT_DEFINE ("MtuStreamingIssueDemo");

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

int
main (int argc, char *argv[])
{
  // Logging setup
  LogComponentEnableAll (LOG_PREFIX_NODE);  // Prefix log messages with node id (0 for client, 1 for server)
  LogComponentEnableAll (LOG_PREFIX_FUNC);  // Prefix the function name before each log message
  LogComponentEnableAll (LOG_PREFIX_TIME);  // Prefix log messages with a timestamp
  LogComponentEnableAll (LOG_PREFIX_LEVEL); // Prefix log messages with their severity level

  LogComponentEnableAll (LOG_WARN);

  LogComponentEnable ("MtuStreamingIssueDemo", LOG_LEVEL_LOGIC);
  LogComponentEnable ("TcpStreamClientApplication", LOG_LEVEL_LOGIC);
  LogComponentEnable ("TcpStreamServerApplication", LOG_LEVEL_LOGIC);
  
  LogComponentEnable ("QuicSocketBase", LOG_LEVEL_LOGIC);
  LogComponentEnable ("QuicSocketTxBuffer", LOG_LEVEL_LOGIC);
  LogComponentEnable ("QuicCongestionControl", LOG_LEVEL_LOGIC);

  LogComponentEnable ("QuicSocketTxScheduler", LOG_LEVEL_INFO);
  LogComponentEnable ("QuicStreamBase", LOG_LEVEL_INFO);
  LogComponentEnable ("QuicStreamTxBuffer", LOG_LEVEL_INFO);
  LogComponentEnable ("QuicSubheader", LOG_LEVEL_INFO);

//   LogComponentEnable ("QuicSocketRxBuffer", LOG_LEVEL_INFO);
//   LogComponentEnable ("QuicL5Protocol", LOG_LEVEL_INFO);
//   LogComponentEnable ("QuicL4Protocol", LOG_LEVEL_INFO);
  
  // Command-line parameters
  uint64_t segmentDuration {2000000};
  uint32_t simulationId {1};
  std::string adaptationAlgo {"festive"};
  std::string segmentSizeFilePath {"contrib/dash/segmentSizes.txt"};
  std::string dataRate {"5Mbps"};

  NS_LOG_UNCOND("\n##### Simulation Config #####");
  NS_LOG_UNCOND("Simulation ID  : " << simulationId);
  NS_LOG_UNCOND("ABR Algorithm  : " << adaptationAlgo);
  NS_LOG_UNCOND("Data Rate      : " << dataRate);
  NS_LOG_UNCOND("Segment File   : " << segmentSizeFilePath);
  NS_LOG_UNCOND("##### ##### ##### ##### #####\n");
  
  auto loggingFolder = createLoggingFolder(adaptationAlgo, simulationId);

  Config::SetDefault ("ns3::QuicSocketBase::MaxPacketSize", UintegerValue (1446)); 
  Config::SetDefault ("ns3::QuicSocketBase::SocketRcvBufSize", UintegerValue (524288));
  Config::SetDefault ("ns3::QuicSocketBase::SocketSndBufSize", UintegerValue (524288));
  Config::SetDefault ("ns3::QuicStreamBase::StreamSndBufSize", UintegerValue (524288));
  Config::SetDefault ("ns3::QuicStreamBase::StreamRcvBufSize", UintegerValue (524288));
  
  Time::SetResolution (Time::NS);

  // Two nodes, one for client and one for server
  NodeContainer nodes;
  nodes.Create (2);

  // A single p2p connection exists between the client and server with no packet loss
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer netDevices;
  netDevices = pointToPoint.Install (nodes);

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
  // TODO rename the dash classes so they don't incorrectly include the "Tcp"
  //      prefix when they actually use QUIC.
  uint16_t serverPort {80};
  TcpStreamServerHelper serverHelper (serverPort);

  auto serverNode = nodes.Get(1);
  ApplicationContainer serverApp = serverHelper.Install (serverNode);
  serverApp.Start (Seconds (1.0));

  // Set up a single streaming client
  auto serverAddress = interfaces.GetAddress(1);
  TcpStreamClientHelper clientHelper (serverAddress, serverPort);

  clientHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
  clientHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
  clientHelper.SetAttribute ("NumberOfClients", UintegerValue (1));
  clientHelper.SetAttribute ("SimulationId", UintegerValue (simulationId));

  auto clientNode = nodes.Get(0);
  std::pair<Ptr<Node>, std::string> clientAlgoPair {clientNode, adaptationAlgo};
  ApplicationContainer clientApps = clientHelper.Install ({clientAlgoPair});
  clientApps.Get(0)->SetStartTime(Seconds(2)); 


  NS_LOG_INFO ("Run Simulation. (" << "id: " << simulationId << ")");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation Complete.");

  return 0;
}
