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

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <sstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RPC");


// Output Trace Length
struct queueLog
  {
     Ptr<Node> node;
     int type;
     int nodeNum;
     int deviceNum;
     double sampling_time;
  };
void handler (struct queueLog queueLogInfo, Ptr<OutputStreamWrapper> stream) {
  std::ostream *output = stream->GetStream();
  *output << queueLogInfo.type << " " << queueLogInfo.nodeNum << " "<< queueLogInfo.deviceNum << " " << (int)(queueLogInfo.sampling_time * 1e9 + 0.5) <<" "<< DynamicCast<PointToPointNetDevice>(queueLogInfo.node->GetDevice(queueLogInfo.deviceNum))->GetQueue()->GetNBytes() << std::endl;
}

int 
main (int argc, char *argv[])
{
  uint32_t ecmpMode = 2;

  uint32_t level_num = 3; // Number of levels of the network, <=5
  // rack_size < 256
  int rack_size = 4;
  int rack_num = 4;
  int l2_size = 4;
  int l2_num = 4;
  int l3_size = 4;
  int l3_num = 2;
  int l4_size = 2;
  int l4_num = 2;
  int l5_size = 2;

  double simulation_stop_time = 0.1;

  double server_link_speed = 40000000000; // link speed in bps
  double load = 0.4; // receiving load on the servers regarding to link speed

  // double sampling_rate_per_link = 10000;

  double reconstruction_period = 0.00025;
  int num_dst_server = 20;

  int differentQueueSize = 0; //Default: false

  int enablePcapTracing = 0; // 0: disable, 1: enable
  int enableServerPcapTracing = 0;
  int enableTorPcapTracing = 0;
  int enableL2PcapTracing = 0;
  int enableL3PcapTracing = 0;

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> streamQueueSize = ascii.CreateFileStream("queuesize.log");
  Ptr<OutputStreamWrapper> streamFlowInfo = ascii.CreateFileStream("flowinfo.log");
  Ptr<OutputStreamWrapper> streamQueue = ascii.CreateFileStream("rpcoutput.log");
  Ptr<OutputStreamWrapper> streamRTTinfo = ascii.CreateFileStream("RTTinfo.txt");

  //LogComponentEnable("Ipv4GlobalRouting", LOG_LEVEL_LOGIC);
  //LogComponentEnable("RPC", LOG_LEVEL_INFO);
  //LogComponentEnable("RPC", LOG_LEVEL_LOGIC);
  //LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  //LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_FUNCTION);
  //LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  //LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_DEBUG);
  
  // Allow the user to override any of the defaults and the above
  // Bind ()s at run-time, via command-line arguments
  CommandLine cmd;
  cmd.AddValue ("EcmpMode", "Ecmp Mode: (0 none, 1 random, 2 flow)", ecmpMode);

  cmd.AddValue ("LevelNum", "How many levels in this network (default = 5)", level_num);

  cmd.AddValue ("RackSize", "Num of servers under a rack (default = 4)", rack_size);
  cmd.AddValue ("RackNum", "Num of racks in a L2 block (default = 4)", rack_num);
  cmd.AddValue ("L2Size", "Num of L2 switches in a L2 block (default = 4)", l2_size);
  cmd.AddValue ("L2Num", "Num of L2 blocks in a L3 block (default = 2)", l2_num);
  cmd.AddValue ("L3Size", "Num of L3 switches in a L3 block (default = 2)", l3_size);
  cmd.AddValue ("L3Num", "Num of L3 blocks in a L4 block (default = 2)", l3_num);
  cmd.AddValue ("L4Size", "Num of L4 switches in a L4 block (default = 2)", l4_size);
  cmd.AddValue ("L4Num", "Num of L4 blocks in a L5 block (default = 2)", l4_num);
  cmd.AddValue ("L5Size", "Num of L5 switches in a L5 block (default = 2)", l5_size);

  cmd.AddValue ("SimulationStopTime", "Time when simulation stops (sec, default = 0.1)", simulation_stop_time);
  cmd.AddValue ("ServerLinkSpeed", "Link speed (bits per sec, default = 10000000000, 10G)", server_link_speed);
  cmd.AddValue ("Load", "Receiving load on the servers regarding to link speed (default = 0.4)", load);
//  cmd.AddValue ("ProbeSamplingRatePerLink", "Num of probe packets sent per sec in each server-ToR link (default = 10000)", sampling_rate_per_link);
  
  cmd.AddValue ("ReconstructionPeriod", "Length of each reconstruction cycle (sec, default = 0.001)", reconstruction_period);
  cmd.AddValue ("NumDstServer", "Num of destination servers to which each server sends probe pkts in each reconstruction cycle (default = 10)", num_dst_server);
  cmd.AddValue ("DifferentQueueSize", "Set queue size differently or not (default = 0, e.g. false)", differentQueueSize);
  cmd.AddValue ("EnablePcapTracing", "Enable pcap tracing or not (0: false, 1: true. Default = 0)", enablePcapTracing);
  cmd.AddValue ("EnableServerPcapTracing", "Enable server pcap tracing or not (0: false, 1: true. Default = 0)", enableServerPcapTracing);
  cmd.AddValue ("EnableTorPcapTracing", "Enable ToR pcap tracing or not (0: false, 1: true. Default = 0)", enableTorPcapTracing);
  cmd.AddValue ("EnableL2PcapTracing", "Enable L2-switch pcap tracing or not (0: false, 1: true. Default = 0)", enableL2PcapTracing);
  cmd.AddValue ("EnableL3PcapTracing", "Enable L3-switch pcap tracing or not (0: false, 1: true. Default = 0)", enableL3PcapTracing);

  cmd.Parse (argc, argv);

  switch (ecmpMode)
    {
      case 0:
        break;  //no ECMP
      case 1:
        Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue (true));
        break;
      case 2:
        Config::SetDefault ("ns3::Ipv4GlobalRouting::FlowEcmpRouting", BooleanValue (true));
        break;  
      default:
        NS_FATAL_ERROR ("Illegal command value for EcmpMode: " << ecmpMode);
        break;
    }

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(1460));
  if (enablePcapTracing != 0)
  {
    Config::SetDefault("ns3::PcapFileWrapper::CaptureSize", UintegerValue(128));
  }

  // Allow the user to override any of the defaults and the above



  // message size distribution in bytes
  double DCTCP_CDF[2][12] = {
    {0, 1e4, 2e4, 3e4, 5e4, 8e4, 2e5, 1e6, 2e6, 5e6, 1e7, 3e7},
    {0, 0.15, 0.2, 0.3, 0.4, 0.53, 0.6, 0.7, 0.8, 0.9, 0.97, 1}
  };

  // compute average packet size
  double DCTCP_MEAN = 0;
  for (int i = 1; i < 12; i++) {
    DCTCP_MEAN += DCTCP_CDF[0][i] * (DCTCP_CDF[1][i] - DCTCP_CDF[1][i - 1]);
  }
  NS_LOG_LOGIC("DCTCP_MEAN " << DCTCP_MEAN);

  // fanout distribution
  double FANOUT_CDF[2][4] = {
    {0, 1, 2, 4},
    {0, 0.5, 0.8, 1}
  };
  double FANOUT_MEAN = 0;
  for (int i = 1; i < 4; i++) {
    FANOUT_MEAN += FANOUT_CDF[0][i] * (FANOUT_CDF[1][i] - FANOUT_CDF[1][i - 1]);
  }
  NS_LOG_LOGIC("FANOUT_MEAN " << FANOUT_MEAN);

  // compute average waiting time
  double average_request_interval = FANOUT_MEAN * DCTCP_MEAN * 8 / (server_link_speed * load);
  double lambda = 1 / average_request_interval;
  NS_LOG_LOGIC("average request interval " << average_request_interval);


  // Create nodes and links
  if (level_num == 2){
    l2_num = 1;
    l3_size = 0;
    l3_num = 1;
    l4_size = 0;
    l4_num = 1;
    l5_size = 0;
  }
  else if (level_num == 3){
    l3_num = 1;
    l4_size = 0;
    l4_num = 1;
    l5_size = 0;
  }
  else if (level_num == 4){
    l4_num = 1;
    l5_size = 0;
  }
  else if (level_num == 5){
    //Do nothing
  }
  else {
    std::cout << "Wrong LevelNum (# of levels). EXIT." << std::endl;
    exit(1);
  }
  int rack_num_total = rack_num*l2_num*l3_num*l4_num;

  char buffer[50];

  InternetStackHelper internet;
  PointToPointHelper p2p;
  sprintf (buffer, "%lubps", (unsigned long)server_link_speed);
  p2p.SetDeviceAttribute ("DataRate", StringValue (buffer));
  p2p.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2p.SetChannelAttribute ("Delay", StringValue ("1us"));
  p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", StringValue("1000")); //Initial setting of queues
  Ipv4AddressHelper ipv4;


  NS_LOG_INFO ("Create Nodes.");
  NS_LOG_INFO ("Create servers.");
  NodeContainer c[rack_num*l2_num*l3_num*l4_num];
  for (int i = 0; i < rack_num*l2_num*l3_num*l4_num; i++) {
    c[i].Create (rack_size);
    internet.Install (c[i]);
  }
  NS_LOG_INFO ("Create ToRs.");
  NodeContainer tor[l2_num*l3_num*l4_num];
  for (int i = 0; i < l2_num*l3_num*l4_num; i++) {
    tor[i].Create (rack_num);
    internet.Install (tor[i]);
  }
  NS_LOG_INFO ("Create l2 routers.");
  NodeContainer l2[l2_num*l3_num*l4_num];
  for (int i = 0; i < l2_num*l3_num*l4_num; i++) {
    l2[i].Create (l2_size);
    internet.Install (l2[i]);
  }
  NodeContainer l3[l3_num*l4_num];
  if (level_num >= 3) {
    NS_LOG_INFO ("Create l3 routers.");
    for (int i = 0; i < l3_num*l4_num; i++) {
      l3[i].Create (l3_size);
      internet.Install (l3[i]);
    }
  }
  NodeContainer l4[l4_num];
  if (level_num >= 4) {
    NS_LOG_INFO ("Create l4 routers.");
    for (int i = 0; i < l4_num; i++) {
      l4[i].Create (l4_size);
      internet.Install (l4[i]);
    }
  }
  NodeContainer l5;
  if (level_num >= 5) {
    NS_LOG_INFO ("Create l5 routers.");
    l5.Create (l5_size);
    internet.Install (l5);
  }

  NS_LOG_INFO ("Create links.");
  NS_LOG_INFO ("Connect servers to ToRs.");
  for (int i = 0; i < l2_num*l3_num*l4_num; i++) {
    for (int j = 0; j < rack_num; j++) {
      for (int k = 0; k < rack_size; k++) {
        NodeContainer nc_tmp = NodeContainer (c[i*rack_num+j].Get (k), tor[i].Get (j));
        NetDeviceContainer ndc_tmp = p2p.Install (nc_tmp);
        sprintf (buffer, "%d.%d.%d.0", 10 + i/l2_num, (i%l2_num)*rack_num + j, k);
        ipv4.SetBase (buffer, "255.255.255.240");
        ipv4.Assign (ndc_tmp);
      }
    }
  }
  NS_LOG_INFO ("Connect ToRs to l2 switches");
  for (int i = 0; i < l2_num*l3_num*l4_num; i++) {
    for (int j = 0; j < l2_size; j++) {
      for (int k = 0; k < rack_num; k++) {
        NodeContainer nc_tmp = NodeContainer (l2[i].Get (j), tor[i].Get (k));
        NetDeviceContainer ndc_tmp = p2p.Install (nc_tmp);
        sprintf (buffer, "%d.%d.%d.16", 10 + i/l2_num, (i%l2_num)*l2_size + j, k);
        ipv4.SetBase (buffer, "255.255.255.240");
        ipv4.Assign (ndc_tmp);
      }
    }
  }
  if (level_num >= 3) {
    NS_LOG_INFO ("Connect l2 switches to l3 switches");
    for (int i = 0; i < l3_num*l4_num; i++) {
      for (int j = 0; j < l3_size; j++) {
        for (int k = 0; k < l2_num; k++) {
          for (int m = 0; m < l2_size; m++) {
            NodeContainer nc_tmp = NodeContainer (l3[i].Get (j), l2[i*l2_num+k].Get (m));
            NetDeviceContainer ndc_tmp = p2p.Install (nc_tmp);
            sprintf (buffer, "%d.%d.%d.32", 10 + i, j, k*l2_size + m);
            ipv4.SetBase (buffer, "255.255.255.240");
            ipv4.Assign (ndc_tmp);
          }
        }
      }
    }
  }
  if (level_num >= 4) {
    NS_LOG_INFO ("Connect l3 switches to l4 switches");
    for (int i = 0; i < l4_num; i++) {
      for (int j = 0; j < l4_size; j++) {
        for (int k = 0; k < l3_num; k++) {
          for (int m = 0; m < l3_size; m++) {
            NodeContainer nc_tmp = NodeContainer (l4[i].Get (j), l3[i*l3_num+k].Get (m));
            NetDeviceContainer ndc_tmp = p2p.Install (nc_tmp);
            sprintf (buffer, "%d.%d.%d.48", 10 + i, j, k*l3_size + m);
            ipv4.SetBase (buffer, "255.255.255.240");
            ipv4.Assign (ndc_tmp);
          }
        }
      }
    }
  }
  if (level_num >= 5) {
    NS_LOG_INFO ("Connect l4 switches to l5 switches");
    for (int i = 0; i < 1; i++) {
      for (int j = 0; j < l5_size; j++) {
        for (int k = 0; k < l4_num; k++) {
          for (int m = 0; m < l4_size; m++) {
            NodeContainer nc_tmp = NodeContainer (l5.Get (j), l4[i*l4_num+k].Get (m));
            NetDeviceContainer ndc_tmp = p2p.Install (nc_tmp);
            sprintf (buffer, "%d.%d.%d.64", 10 + i, j, k*l4_size + m);
            ipv4.SetBase (buffer, "255.255.255.240");
            ipv4.Assign (ndc_tmp);
          }
        }
      }
    }
  }



  if (differentQueueSize != 0)
  {
    std::cout << "Different queue sizes are not supported in this version yet. EXIT." << std::endl;
    exit(1);
    /*
  	//Possible queue size in Bytes
	  uint32_t queueSizeChoice = 10;
	  uint32_t queueSize[10] = {300000, 350000, 400000, 450000, 500000, 550000, 600000, 650000, 700000, 750000};

	  bool queueSizeSameInDifferentRun = true; //For each queue, whether the queue size is same in different runs or not.
	  if (queueSizeSameInDifferentRun)
	  {
	  	srand (0);
	  }
	  else
	  {
	  	srand (time (NULL));
	  }

  	NS_LOG_INFO ("Set queue sizes");
  	std::ostream *outputQueueSize = streamQueueSize->GetStream();
	  int type = 0;// Servers
	  int deviceNum = 1;  
	  for (int i = 0; i < rack_num; i++) {
	    for (int j = 0; j < rack_size; j++) {
	    	uint32_t chosenQueueSize = queueSize[ rand() % queueSizeChoice ];
			  DynamicCast<DropTailQueue>(DynamicCast<PointToPointNetDevice>(c[i].Get (j)->GetDevice(deviceNum))->GetQueue())->SetMode(ns3::Queue::QUEUE_MODE_BYTES);
			  DynamicCast<DropTailQueue>(DynamicCast<PointToPointNetDevice>(c[i].Get (j)->GetDevice(deviceNum))->GetQueue())->SetMaxBytes(chosenQueueSize);
			  *outputQueueSize << type << " " << i*rack_size+j << " "<< deviceNum << " " << DynamicCast<DropTailQueue>(DynamicCast<PointToPointNetDevice>(c[i].Get (j)->GetDevice(deviceNum))->GetQueue())->GetMaxBytes()<< std::endl;
	    }
	  }
	  type = 1;// ToRs
	  for (int i = 0; i < rack_num; i++) {
	    for (deviceNum = 1; deviceNum <= ( rack_size+l2_num ); deviceNum++) {
	    	uint32_t chosenQueueSize = queueSize[ rand() % queueSizeChoice ];
	    	DynamicCast<DropTailQueue>(DynamicCast<PointToPointNetDevice>(tor.Get (i)->GetDevice(deviceNum))->GetQueue())->SetMode(ns3::Queue::QUEUE_MODE_BYTES);
	    	DynamicCast<DropTailQueue>(DynamicCast<PointToPointNetDevice>(tor.Get (i)->GetDevice(deviceNum))->GetQueue())->SetMaxBytes(chosenQueueSize);
	    	*outputQueueSize << type << " " << i << " "<< deviceNum << " " << DynamicCast<DropTailQueue>(DynamicCast<PointToPointNetDevice>(tor.Get (i)->GetDevice(deviceNum))->GetQueue())->GetMaxBytes()<< std::endl;
	    }
	  }
	  type = 2;// l2 switches
	  for (int i = 0; i < l2_num; i++) {
	    for (deviceNum = 1; deviceNum <= rack_num; deviceNum++) {
	    	uint32_t chosenQueueSize = queueSize[ rand() % queueSizeChoice ];
	    	DynamicCast<DropTailQueue>(DynamicCast<PointToPointNetDevice>(l2.Get (i)->GetDevice(deviceNum))->GetQueue())->SetMode(ns3::Queue::QUEUE_MODE_BYTES);
	    	DynamicCast<DropTailQueue>(DynamicCast<PointToPointNetDevice>(l2.Get (i)->GetDevice(deviceNum))->GetQueue())->SetMaxBytes(chosenQueueSize);
	    	*outputQueueSize << type << " " << i << " "<< deviceNum << " " << DynamicCast<DropTailQueue>(DynamicCast<PointToPointNetDevice>(l2.Get (i)->GetDevice(deviceNum))->GetQueue())->GetMaxBytes()<< std::endl;
	    }
	  }
    */
  }

  for (int i = 0; i < rack_num_total; i++) {
    for (int j = 0; j < rack_size; j++) {
      //DynamicCast<PointToPointNetDevice>(c[i].Get (j)->GetDevice(1))->SetLogTxTime(true);
    }
  }



  NS_LOG_INFO ("Populate routing tables.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();



  NS_LOG_INFO ("Create Applications.");
  srand (time (NULL));
  uint16_t port = 9;   // Discard port (RFC 863)
  for (int i = 0; i < rack_num_total; i++) {
    for (int j = 0; j < rack_size; j++) {
  
      NS_LOG_LOGIC ("Client " << i << " " << j);

      double current_time = 0;
      while (current_time < simulation_stop_time) {
        double r = 0;
        while (r == 0 || r == 1) {
          r = (double) rand () / RAND_MAX;
        }
        current_time += - log (r) / lambda;
        NS_LOG_LOGIC ("Time " << current_time);

        // generate random fanout
        double fanout = 1;
        double tmp = (double) rand ()/ RAND_MAX;
        for (int k = 1; k < 4; k++) {
          if (tmp <= FANOUT_CDF[1][k]) {
            fanout = FANOUT_CDF[0][k];
            break;
          }
        }
        NS_LOG_LOGIC ("Fanout " << fanout);

        // install requester on the clients
			  ApplicationContainer echoClientApps;
        for (int k = 0; k < fanout; k++) {
          // generate random message size
          uint32_t maxBytes = 0;
          tmp = (double) rand () / RAND_MAX;
          for (int l = 1; l < 12; l++) {
            if (tmp <= DCTCP_CDF[1][l]) {
              maxBytes = DCTCP_CDF[0][l];
              break;
            }
          }          

          // generate random server
          int i_tmp = i;
          int j_tmp = j;
          while (i_tmp == i && j_tmp == j) {
            i_tmp = rand() % rack_num_total;
            j_tmp = rand() % rack_size;
          }

          sprintf (buffer, "%d.%d.%d.1", 10+i_tmp/(rack_num*l2_num), i_tmp%(rack_num*l2_num), j_tmp);
          UdpEchoClientHelper echoClient(Ipv4Address(buffer), port);

          echoClient.SetAttribute ("RequestDataSize", UintegerValue (maxBytes));
          echoClient.SetAttribute ("MaxPackets", UintegerValue (1));

          echoClientApps.Add (echoClient.Install (c[i].Get (j), streamFlowInfo));
//          std::cout<<"DEBUG: flow start at "<<current_time<<" s, requester "<<i<<" "<<j<<" server "<<i_tmp<<" "<<j_tmp<<", length "<<maxBytes<<std::endl;
        }

        echoClientApps.Start (Seconds(current_time));
        echoClientApps.Stop (Seconds (simulation_stop_time));
      }
    }
  } 

  PacketSinkHelper sink("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  ApplicationContainer sinkApps;
  for (int i = 0; i < rack_num_total; i++) {
    for (int j = 0; j < rack_size; j++) {
      sinkApps.Add (sink.Install (c[i].Get (j)));
    }
  }
  sinkApps.Start (Seconds (0));
  sinkApps.Stop (Seconds (simulation_stop_time));

  // probe traffic
  uint16_t port_echo = 10;
  ApplicationContainer pingers;
  for (int i = 0; i < rack_num_total; i++) {
    for (int j = 0; j < rack_size; j++) {
      
      //Randomly choose num_dst_server destination servers
      //Only allow probing between different L2-blocks
      int *dstServerList = new int[rack_size*(rack_num_total-rack_num)];
      for (int i_tmp = 0; i_tmp < rack_size*(rack_num_total-rack_num); i_tmp++) {
        dstServerList[i_tmp] = 0;
      }
      srand (i*rack_size+j);
      int tmp_numChosenServer;
      do
      {
        dstServerList[rand() % (rack_size*(rack_num_total-rack_num))] += 1;
        tmp_numChosenServer = 0;
        for (int i_tmp = 0; i_tmp < rack_size*(rack_num_total-rack_num); i_tmp++) {
          tmp_numChosenServer += dstServerList[i_tmp];
        }
      } while ( tmp_numChosenServer < num_dst_server );

      for (int i_tmp = 0; i_tmp < rack_num_total; i_tmp++) {
        for (int j_tmp = 0; j_tmp < rack_size; j_tmp++) {
          int index;
          //if (i_tmp < i) {
          if (i_tmp/rack_num < i/rack_num) {
            index = i_tmp * rack_size + j_tmp;
          }
          else if (i_tmp/rack_num > i/rack_num) {
            index = (i_tmp-rack_num) * rack_size + j_tmp;
          }
          else {
            continue; //No probe traffic between servers under the same L2-block
          }

          if (dstServerList[index] > 0) { 
          	sprintf (buffer, "%d.%d.%d.1", 10+i_tmp/(rack_num*l2_num), i_tmp%(rack_num*l2_num), j_tmp);
            UdpEchoClientHelper client (Ipv4Address(buffer), port_echo);
            client.SetAttribute ("MaxPackets", UintegerValue (simulation_stop_time / ( reconstruction_period/dstServerList[index] ) + 1));
            client.SetAttribute ("Interval", TimeValue (Seconds (reconstruction_period/dstServerList[index])));
            client.SetAttribute ("ReconstructionInterval", TimeValue (Seconds (reconstruction_period)));
            client.SetAttribute ("PacketSize", UintegerValue (64)); // Don't change it! If it really needs to be changed, some other params in applications & point-to-point-net-device also need to be changed.

            ApplicationContainer pinger = client.Install (c[i].Get (j));
            client.SetFill (pinger.Get (0), 0, 64);
            //DynamicCast<UdpEchoClient>( pinger.Get(0) ) -> SetSrcDstAddr(i, j, i_tmp, j_tmp);
            //DynamicCast<UdpEchoClient>( pinger.Get(0) ) -> SetNetworkParam (level_num, rack_size, rack_num, l2_size, l2_num, l3_size, l3_num, l4_size, l4_num, l5_size);
            //DynamicCast<UdpEchoClient>( pinger.Get(0) ) -> SetRTTinfoStream(streamRTTinfo);
            pingers.Add (pinger);
          }
        }
      }

      delete []dstServerList;

    }
  }
  pingers.Start (Seconds (0));
  pingers.Stop (Seconds (simulation_stop_time));

  UdpEchoServerHelper server (port_echo);
  ApplicationContainer echoers;
  for (int i = 0; i < rack_num_total; i++) {
    for (int j = 0; j < rack_size; j++) {
      echoers.Add (server.Install (c[i].Get (j)));
    }
  }
  echoers.Start (Seconds (0));
  echoers.Stop (Seconds (simulation_stop_time));

  if (enablePcapTracing != 0)
  {
    // The filename: "rpc-X-1.pcap" X=i*rack_size+j
    if (enableServerPcapTracing != 0)
    {
      NodeContainer pcapContainer_server;
      for (int i = 0; i < rack_num_total; i++) {
        for (int j = 0; j < rack_size; j++) {
          pcapContainer_server.Add (c[i].Get (j));
        }
      }
      p2p.EnablePcap ("server/rpc-server", pcapContainer_server);
    }

    if (enableTorPcapTracing != 0)
    {
      NodeContainer pcapContainer_tor;
      for (int i = 0; i < l2_num*l3_num*l4_num; i++) {
        for (int j = 0; j < rack_num; j++) {
          pcapContainer_tor.Add (tor[i].Get (j));
        }      
      }
      p2p.EnablePcap ("tor/rpc-tor", pcapContainer_tor);
    }

    if (enableL2PcapTracing != 0)
    {
      NodeContainer pcapContainer_l2;
      for (int i = 0; i < l2_num*l3_num*l4_num; i++) {
        for (int j = 0; j < l2_size; j++) {
          pcapContainer_l2.Add (l2[i].Get (j));
        }      
      }
      p2p.EnablePcap ("l2/rpc-l2", pcapContainer_l2);
    }

    if (enableL3PcapTracing != 0)
    {
      if (level_num >= 3) {
        NodeContainer pcapContainer_l3;
        for (int i = 0; i < l3_num*l4_num; i++) {
          for (int j = 0; j < l3_size; j++) {
            pcapContainer_l3.Add (l3[i].Get (j));
          }      
        }
        p2p.EnablePcap ("l3/rpc-l3", pcapContainer_l3);
      }
    }

  }


  // queue size monitor
  double sampling_step = 0.000001;
  int type = 0; //type = 0: Servers
  int deviceNum = 1;  
  for (int i = 0; i < rack_num_total; i++) {
    for (int j = 0; j < rack_size; j++) {
      for (double sampling_time = 0; sampling_time < simulation_stop_time; sampling_time = sampling_time + sampling_step) {
        struct queueLog queueLogInfo;
        queueLogInfo.node = c[i].Get (j);
        queueLogInfo.type = type;
        queueLogInfo.nodeNum = i*rack_size+j;
        queueLogInfo.deviceNum = deviceNum;
        queueLogInfo.sampling_time = sampling_time;
        Simulator::Schedule (Seconds (sampling_time), &handler, queueLogInfo, streamQueue);
      }
    }
  }

  type = 1; //type = 1: ToRs
  for (int i = 0; i < l2_num*l3_num*l4_num; i++) {
    for (int j = 0; j < rack_num; j++) {
      for (deviceNum = 1; deviceNum <= ( rack_size+l2_size ); deviceNum++) {
        for (double sampling_time = 0; sampling_time < simulation_stop_time; sampling_time = sampling_time + sampling_step) {
          struct queueLog queueLogInfo;
          queueLogInfo.node = tor[i].Get (j);
          queueLogInfo.type = type;
          queueLogInfo.nodeNum = i*rack_num+j;
          queueLogInfo.deviceNum = deviceNum;
          queueLogInfo.sampling_time = sampling_time;
          Simulator::Schedule (Seconds (sampling_time), &handler, queueLogInfo, streamQueue);
        }
      }
    }
  }
  type = 2; //type = 2: l2 switches
  for (int i = 0; i < l2_num*l3_num*l4_num; i++) {
    for (int j = 0; j < l2_size; j++) {
      for (deviceNum = 1; deviceNum <= rack_num+l3_size; deviceNum++) {
        for (double sampling_time = 0; sampling_time < simulation_stop_time; sampling_time = sampling_time + sampling_step) {
          struct queueLog queueLogInfo;
          queueLogInfo.node = l2[i].Get (j);
          queueLogInfo.type = type;
          queueLogInfo.nodeNum = i*l2_size+j;
          queueLogInfo.deviceNum = deviceNum;
          queueLogInfo.sampling_time = sampling_time;
          Simulator::Schedule (Seconds (sampling_time), &handler, queueLogInfo, streamQueue);
        }
      }
    }
  }
  if (level_num >= 3) {
    type = 3; //type = 3: l3 switches
    for (int i = 0; i < l3_num*l4_num; i++) {
      for (int j = 0; j < l3_size; j++) {
        for (deviceNum = 1; deviceNum <= l2_size*l2_num+l4_size; deviceNum++) {
          for (double sampling_time = 0; sampling_time < simulation_stop_time; sampling_time = sampling_time + sampling_step) {
            struct queueLog queueLogInfo;
            queueLogInfo.node = l3[i].Get (j);
            queueLogInfo.type = type;
            queueLogInfo.nodeNum = i*l3_size+j;
            queueLogInfo.deviceNum = deviceNum;
            queueLogInfo.sampling_time = sampling_time;
            Simulator::Schedule (Seconds (sampling_time), &handler, queueLogInfo, streamQueue);
          }
        }
      }
    }
  }
  if (level_num >= 4) {
    type = 4; //type = 4: l4 switches
    for (int i = 0; i < l4_num; i++) {
      for (int j = 0; j < l4_size; j++) {
        for (deviceNum = 1; deviceNum <= l3_size*l3_num+l5_size; deviceNum++) {
          for (double sampling_time = 0; sampling_time < simulation_stop_time; sampling_time = sampling_time + sampling_step) {
            struct queueLog queueLogInfo;
            queueLogInfo.node = l4[i].Get (j);
            queueLogInfo.type = type;
            queueLogInfo.nodeNum = i*l4_size+j;
            queueLogInfo.deviceNum = deviceNum;
            queueLogInfo.sampling_time = sampling_time;
            Simulator::Schedule (Seconds (sampling_time), &handler, queueLogInfo, streamQueue);
          }
        }
      }
    }
  }
  if (level_num >= 5) {
    type = 5; //type = 5: l5 switches
    for (int i = 0; i < 1; i++) {
      for (int j = 0; j < l5_size; j++) {
        for (deviceNum = 1; deviceNum <= l4_size*l4_num; deviceNum++) {
          for (double sampling_time = 0; sampling_time < simulation_stop_time; sampling_time = sampling_time + sampling_step) {
            struct queueLog queueLogInfo;
            queueLogInfo.node = l5.Get (j);
            queueLogInfo.type = type;
            queueLogInfo.nodeNum = j;
            queueLogInfo.deviceNum = deviceNum;
            queueLogInfo.sampling_time = sampling_time;
            Simulator::Schedule (Seconds (sampling_time), &handler, queueLogInfo, streamQueue);
          }
        }
      }
    }
  }


  NS_LOG_INFO ("Run Simulation.");

//  p2p.EnableAsciiAll (ascii.CreateFileStream ("rpc.tr"));
//  p2p.EnableAscii (ascii.CreateFileStream ("rpc.tr"), 0, 1);

  Simulator::Stop (Seconds (simulation_stop_time));
  Simulator::Run ();
  Simulator::Destroy ();
}
