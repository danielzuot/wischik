/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
 * 
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
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "udp-echo-client.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UdpEchoClientApplication");
NS_OBJECT_ENSURE_REGISTERED (UdpEchoClient);

TypeId
UdpEchoClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UdpEchoClient")
    .SetParent<Application> ()
    .AddConstructor<UdpEchoClient> ()
    .AddAttribute ("MaxPackets", 
                   "The maximum number of packets the application will send",
                   UintegerValue (100),
                   MakeUintegerAccessor (&UdpEchoClient::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval", 
                   "The time to wait between packets",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&UdpEchoClient::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("ReconstructionInterval", 
                   "The time of reconstruction interval",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&UdpEchoClient::m_reconInterval),
                   MakeTimeChecker ())
    .AddAttribute ("RemoteAddress", 
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&UdpEchoClient::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", 
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&UdpEchoClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&UdpEchoClient::SetDataSize,
                                         &UdpEchoClient::GetDataSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&UdpEchoClient::m_txTrace))
  ;
  return tid;
}

UdpEchoClient::UdpEchoClient ()
{
  NS_LOG_FUNCTION (this);
  m_sent = 0;
  m_socket = 0;
  m_sendEvent = EventId ();
  m_data = 0;
  m_dataSize = 0;
}

UdpEchoClient::~UdpEchoClient()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;

  delete [] m_data;
//  delete [] m_randominterval;
  for (uint32_t i=0; i<num_pktInReconInterval; i++)
  {
    delete [] marker0[i];
    delete [] marker1[i];
  }
  delete [] marker0;
  delete [] marker1;
  m_data = 0;
  m_dataSize = 0;
}

void 
UdpEchoClient::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void 
UdpEchoClient::SetRemote (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address (ip);
  m_peerPort = port;
}

void 
UdpEchoClient::SetRemote (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address (ip);
  m_peerPort = port;
}

void
UdpEchoClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
UdpEchoClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
          m_socket->Bind();
          m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
        {
          m_socket->Bind6();
          m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&UdpEchoClient::HandleRead, this));

  bool probePktSentTimeSeed_fix = true;
//  m_randominterval = new double[m_count*3];
  uint32_t srcNodeID = m_socket->GetNode()->GetId();
  if (probePktSentTimeSeed_fix)
    {
      randomseed = (Ipv4Address::ConvertFrom( m_peerAddress ).Get()/256)%65536 + 6317*srcNodeID;
    }
  else
    {
      randomseed = (Ipv4Address::ConvertFrom( m_peerAddress ).Get()/256)%65536 + 6317*srcNodeID + time (NULL);
    }
  srand(randomseed);
  double firstSentTime = m_interval.GetSeconds() * (double) rand () / RAND_MAX;

  num_pktInReconInterval = (uint32_t)round(m_reconInterval.GetSeconds()/m_interval.GetSeconds());
  
  marker0 = new uint8_t*[num_pktInReconInterval];
  marker1 = new uint8_t*[num_pktInReconInterval];
  for (uint32_t i=0; i<num_pktInReconInterval; i++)
  {
    marker0[i] = new uint8_t[11];
    marker1[i] = new uint8_t[11];
    for (uint32_t tmpIndex = 0; tmpIndex < 11; tmpIndex++)
    {
      marker0[i][tmpIndex] = rand()%256;
      marker1[i][tmpIndex] = rand()%256;
    }
  }

  ScheduleTransmit (Seconds (firstSentTime));
}

void 
UdpEchoClient::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  Simulator::Cancel (m_sendEvent);
}

void 
UdpEchoClient::SetDataSize (uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << dataSize);

  //
  // If the client is setting the echo packet data size this way, we infer
  // that she doesn't care about the contents of the packet at all, so 
  // neither will we.
  //
  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
  m_size = dataSize;
}

uint32_t 
UdpEchoClient::GetDataSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_size;
}

void
UdpEchoClient::SetSrcDstAddr (int srcAddr0, int srcAddr1, int dstAddr0, int dstAddr1)
{
  m_srcAddr0 = srcAddr0;
  m_srcAddr1 = srcAddr1;
  m_dstAddr0 = dstAddr0;
  m_dstAddr1 = dstAddr1;
}

void 
UdpEchoClient::SetNetworkParam (uint32_t level_num, uint32_t rack_size, uint32_t rack_num, uint32_t l2_size, uint32_t l2_num, uint32_t l3_size, uint32_t l3_num, uint32_t l4_size, uint32_t l4_num, uint32_t l5_size)
{
  m_level_num = level_num;
  m_rack_size = rack_size;
  m_rack_num = rack_num;
  m_l2_size = l2_size;
  m_l2_num = l2_num;
  m_l3_size = l3_size;
  m_l3_num = l3_num;
  m_l4_size = l4_size;
  m_l4_num = l4_num;
  m_l5_size = l5_size;
}

void 
UdpEchoClient::SetRTTinfoStream (Ptr<OutputStreamWrapper> streamRTTinfo)
{
  m_streamRTTinfo = streamRTTinfo;
}

void 
UdpEchoClient::SetFill (std::string fill)
{
  NS_LOG_FUNCTION (this << fill);

  uint32_t dataSize = fill.size () + 1;

  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memcpy (m_data, fill.c_str (), dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
UdpEchoClient::SetFill (uint8_t fill, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memset (m_data, fill, dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
UdpEchoClient::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << fillSize << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  if (fillSize >= dataSize)
    {
      memcpy (m_data, fill, dataSize);
      m_size = dataSize;
      return;
    }

  //
  // Do all but the final fill.
  //
  uint32_t filled = 0;
  while (filled + fillSize < dataSize)
    {
      memcpy (&m_data[filled], fill, fillSize);
      filled += fillSize;
    }

  //
  // Last fill may be partial
  //
  memcpy (&m_data[filled], fill, dataSize - filled);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
UdpEchoClient::ScheduleTransmit (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &UdpEchoClient::Send, this);
}

void 
UdpEchoClient::Send (void)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> p;
  if (m_dataSize)
    {
      //
      // If m_dataSize is non-zero, we have a data buffer of the same size that we
      // are expected to copy and send.  This state of affairs is created if one of
      // the Fill functions is called.  In this case, m_size must have been set
      // to agree with m_dataSize
      //
      //std::cout << m_srcAddr0<<" "<< m_srcAddr1<<" "<< m_dstAddr0<<" "<< m_dstAddr1<< " m_sent: " << m_sent<< " "<<m_sent/2<< " " << Simulator::Now ().GetSeconds ()<< std::endl;
      NS_ASSERT_MSG (m_dataSize == m_size, "UdpEchoClient::Send(): m_size and m_dataSize inconsistent");
      NS_ASSERT_MSG (m_data, "UdpEchoClient::Send(): m_dataSize but no m_data");
      p = Create<Packet> (m_data, m_dataSize);      
      memset (m_data, *m_data + 1, 8); // The first 8 bytes are filled by sequence number
      memcpy (m_data + 8, marker0[( (m_sent+1)/2 )%num_pktInReconInterval], 11); // The next 11 bytes are filled by route marker0 (randomly created by UDP echo cilent). They are used to choose routes in 11 hops of the UDP probe pkts. The next 1 byte is empty
      //Here, the payload written here will be used in the next packet, so we use (m_sent+1) rather than (m_sent) here.
      memcpy (m_data + 20, marker1[( (m_sent+1)/2 )%num_pktInReconInterval], 11); // The next 11 bytes are filled by route marker1 (also randomly created by UDP echo cilent). They are used to choose routes in 11 hops of the UDP echo pkts. The next 1 byte is empty
//      memset (m_data + 32, 0, m_dataSize-32); // The remaining bytes are filled by 0
    }
  else
    {
      //std::cout << m_srcAddr0<<" "<< m_srcAddr1<<" "<< m_dstAddr0<<" "<< m_dstAddr1<< " m_sent: " << m_sent<< " "<<m_sent/2<< " " << Simulator::Now ().GetSeconds ()<< " Create new pkt"<<std::endl;
      //
      // If m_dataSize is zero, the client has indicated that she doesn't care 
      // about the data itself either by specifying the data size by setting
      // the corresponding atribute or by not calling a SetFill function.  In 
      // this case, we don't worry about it either.  But we do allow m_size
      // to have a value different from the (zero) m_dataSize.
      //
      p = Create<Packet> (m_size);
    }
  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  m_socket->Send (p);
//  InetSocketAddress targetaddress = InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort);
//  p->RemoveAllPacketTags ();
//  p->RemoveAllByteTags ();
//  std::cout<< DynamicCast<PointToPointNetDevice>(m_socket->GetNode()->GetDevice(1))->GetQueue()->GetNBytes() <<std::endl;
//  m_socket->SendTo (p, 0, targetaddress);
  NS_LOG_INFO ("sent packet to " << m_peerAddress);

//  m_txTrace (p);

  ++m_sent;

  if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv4Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv6Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
    }

  if (m_sent < 2*m_count) 
    {
      if (m_sent%2 == 1)
      {
        ScheduleTransmit (Seconds (30e-6)); //30us
      }
      else
      {
        ScheduleTransmit ( Seconds( m_interval.GetSeconds() - 30e-6 ) );
      }
    }
}

void
UdpEchoClient::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }

      double probeTxTime, probeRxTime, echoTxTime;
      double echoRxTime = Simulator::Now().GetSeconds();
      uint8_t echoPkt[200];
      uint32_t echoPktLength = packet->CopyData(echoPkt, 200);
      memcpy ( &probeTxTime, echoPkt + echoPktLength - 32, 8 );
      memcpy ( &probeRxTime, echoPkt + echoPktLength - 24, 8 );
      memcpy ( &echoTxTime, echoPkt + echoPktLength - 16, 8 );

      uint32_t srcIPAddr = 16777216*(10+m_srcAddr0/(m_rack_num*m_l2_num)) + 65536*(m_srcAddr0%(m_rack_num*m_l2_num)) + 256*m_srcAddr1 + 1;
      uint32_t dstIPAddr = 16777216*(10+m_dstAddr0/(m_rack_num*m_l2_num)) + 65536*(m_dstAddr0%(m_rack_num*m_l2_num)) + 256*m_dstAddr1 + 1;
      uint32_t protocol = 17; // UDP protocol number
      Address srcAddr;
      m_socket->GetSockName (srcAddr);
      uint32_t srcPort = InetSocketAddress::ConvertFrom (srcAddr).GetPort ();
      uint32_t dstPort = InetSocketAddress::ConvertFrom (from).GetPort ();
      uint32_t tupleValueBase = srcIPAddr + dstIPAddr + protocol + srcPort + dstPort;
      int selectIndex0[7]; //selectIndex for probe pkt. For 2, 3, 4, 5-layer network, we have 1, 3, 5, 7 indexes, respectively.
      int selectIndex1[7]; //selectIndex for echo pkt. For 2, 3, 4, 5-layer network, we have 1, 3, 5, 7 indexes, respectively.
      for (int tmpIndex = 0; tmpIndex < 7; tmpIndex++)
      {
        selectIndex0[tmpIndex] = -1;
        selectIndex1[tmpIndex] = -1;
      }

      if (m_level_num == 5)
      {
        //Add route marker0 into tupleValue. Then calculate selectIndex. Index in echoPkt = 8bytes(Seq number) + the ?th byte in marker0 region
        selectIndex0[0] = ( tupleValueBase + (uint8_t) echoPkt[8] ) % m_l2_size;
        selectIndex0[1] = ( tupleValueBase + (uint8_t) echoPkt[9] ) % m_l3_size;
        selectIndex0[2] = ( tupleValueBase + (uint8_t) echoPkt[10] ) % m_l4_size;
        selectIndex0[3] = ( tupleValueBase + (uint8_t) echoPkt[11] ) % m_l5_size;
        selectIndex0[4] = ( tupleValueBase + (uint8_t) echoPkt[12] ) % m_l4_size;
        selectIndex0[5] = ( tupleValueBase + (uint8_t) echoPkt[13] ) % m_l3_size;
        selectIndex0[6] = ( tupleValueBase + (uint8_t) echoPkt[14] ) % m_l2_size;
        //Add route marker1 into tupleValue. Then calculate selectIndex. Index in echoPkt = 8bytes(Seq number) + 12bytes(marker0) + the ?th byte in marker0 region
        selectIndex1[0] = ( tupleValueBase + (uint8_t) echoPkt[20] ) % m_l2_size;
        selectIndex1[1] = ( tupleValueBase + (uint8_t) echoPkt[21] ) % m_l3_size;
        selectIndex1[2] = ( tupleValueBase + (uint8_t) echoPkt[22] ) % m_l4_size;
        selectIndex1[3] = ( tupleValueBase + (uint8_t) echoPkt[23] ) % m_l5_size;
        selectIndex1[4] = ( tupleValueBase + (uint8_t) echoPkt[24] ) % m_l4_size;
        selectIndex1[5] = ( tupleValueBase + (uint8_t) echoPkt[25] ) % m_l3_size;
        selectIndex1[6] = ( tupleValueBase + (uint8_t) echoPkt[26] ) % m_l2_size;
      }
      else if (m_level_num == 4)
      {
        //Add route marker0 into tupleValue. Then calculate selectIndex. Index in echoPkt = 8bytes(Seq number) + the ?th byte in marker0 region
        selectIndex0[0] = ( tupleValueBase + (uint8_t) echoPkt[8] ) % m_l2_size;
        selectIndex0[1] = ( tupleValueBase + (uint8_t) echoPkt[9] ) % m_l3_size;
        selectIndex0[2] = ( tupleValueBase + (uint8_t) echoPkt[10] ) % m_l4_size;
        selectIndex0[3] = ( tupleValueBase + (uint8_t) echoPkt[11] ) % m_l3_size;
        selectIndex0[4] = ( tupleValueBase + (uint8_t) echoPkt[12] ) % m_l2_size;
        //Add route marker1 into tupleValue. Then calculate selectIndex. Index in echoPkt = 8bytes(Seq number) + 12bytes(marker0) + the ?th byte in marker0 region
        selectIndex1[0] = ( tupleValueBase + (uint8_t) echoPkt[20] ) % m_l2_size;
        selectIndex1[1] = ( tupleValueBase + (uint8_t) echoPkt[21] ) % m_l3_size;
        selectIndex1[2] = ( tupleValueBase + (uint8_t) echoPkt[22] ) % m_l4_size;
        selectIndex1[3] = ( tupleValueBase + (uint8_t) echoPkt[23] ) % m_l3_size;
        selectIndex1[4] = ( tupleValueBase + (uint8_t) echoPkt[24] ) % m_l2_size;
      }
      else if (m_level_num == 3)
      {
        //Add route marker0 into tupleValue. Then calculate selectIndex. Index in echoPkt = 8bytes(Seq number) + the ?th byte in marker0 region
        selectIndex0[0] = ( tupleValueBase + (uint8_t) echoPkt[8] ) % m_l2_size;
        selectIndex0[1] = ( tupleValueBase + (uint8_t) echoPkt[9] ) % m_l3_size;
        selectIndex0[2] = ( tupleValueBase + (uint8_t) echoPkt[10] ) % m_l2_size;
        //Add route marker1 into tupleValue. Then calculate selectIndex. Index in echoPkt = 8bytes(Seq number) + 12bytes(marker0) + the ?th byte in marker0 region
        selectIndex1[0] = ( tupleValueBase + (uint8_t) echoPkt[20] ) % m_l2_size;
        selectIndex1[1] = ( tupleValueBase + (uint8_t) echoPkt[21] ) % m_l3_size;
        selectIndex1[2] = ( tupleValueBase + (uint8_t) echoPkt[22] ) % m_l2_size;
      }
      else // m_level_num == 2
      {
        //Add route marker0 into tupleValue. Then calculate selectIndex. Index in echoPkt = 8bytes(Seq number) + the ?th byte in marker0 region
        selectIndex0[0] = ( tupleValueBase + (uint8_t) echoPkt[8] ) % m_l2_size;
        //Add route marker1 into tupleValue. Then calculate selectIndex. Index in echoPkt = 8bytes(Seq number) + 12bytes(marker0) + the ?th byte in marker0 region
        selectIndex1[0] = ( tupleValueBase + (uint8_t) echoPkt[20] ) % m_l2_size;
      }

      std::ostream *output = m_streamRTTinfo->GetStream();
      *output << m_srcAddr0 << ' ' << m_srcAddr1 << ' ' << m_dstAddr0 << ' ' << m_dstAddr1 << ' ';
      for (int tmpIndex = 0; tmpIndex < 7; tmpIndex++)
      {
        *output << selectIndex0[tmpIndex] << ' ';
      }
      for (int tmpIndex = 0; tmpIndex < 7; tmpIndex++)
      {
        *output << selectIndex1[tmpIndex] << ' ';
      }
      *output << (int)(probeTxTime * 1e9 + 0.5) << ' ' << (int)(probeRxTime * 1e9 + 0.5) << ' ' << (int)(echoTxTime * 1e9 + 0.5) << ' ' << (int)(echoRxTime * 1e9 + 0.5) << std::endl;
    }
}

} // Namespace ns3
