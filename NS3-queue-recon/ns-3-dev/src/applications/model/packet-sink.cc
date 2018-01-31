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
 *
 * Author:  Tom Henderson (tomhend@u.washington.edu)
 */
#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "packet-sink.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PacketSink");
NS_OBJECT_ENSURE_REGISTERED (PacketSink);

TypeId 
PacketSink::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PacketSink")
    .SetParent<Application> ()
    .AddConstructor<PacketSink> ()
    .AddAttribute ("Local", "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&PacketSink::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol", "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&PacketSink::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&PacketSink::m_rxTrace))
    .AddTraceSource ("Tx", "A packet has been sent",
                     MakeTraceSourceAccessor (&PacketSink::m_txTrace))
  ;
  return tid;
}

PacketSink::PacketSink ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_totalRx = 0;
}

PacketSink::~PacketSink()
{
  NS_LOG_FUNCTION (this);
  for (std::list<uint8_t*>::iterator it=m_path.begin(); it!=m_path.end(); ++it)
  {
    delete []*it;
  }
}

uint32_t PacketSink::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
PacketSink::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket> >
PacketSink::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void PacketSink::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}


// Application Methods
void PacketSink::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      m_socket->Bind (m_local);
      m_socket->Listen ();
//      m_socket->ShutdownSend ();
      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: joining multicast on a non-UDP socket");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&PacketSink::HandleRead, this));
  m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&PacketSink::HandleAccept, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&PacketSink::HandlePeerClose, this),
    MakeCallback (&PacketSink::HandlePeerError, this));
}

void PacketSink::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  while(!m_socketList.empty ()) //these are accepted sockets, close them
    {
      Ptr<Socket> acceptedSocket = m_socketList.front ();
      m_socketList.pop_front ();
      acceptedSocket->Close ();
    }
  if (m_socket) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void PacketSink::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () == 0)
        { //EOF
          break;
        }
      m_totalRx += packet->GetSize ();
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s packet sink received "
                       <<  packet->GetSize () << " bytes from "
                       << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                       << " port " << InetSocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << m_totalRx << " bytes");
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s packet sink received "
                       <<  packet->GetSize () << " bytes from "
                       << Inet6SocketAddress::ConvertFrom(from).GetIpv6 ()
                       << " port " << Inet6SocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << m_totalRx << " bytes");
        }
      m_rxTrace (packet, from);

      // Get Requested data size
      packet->RemoveAllPacketTags ();
      packet->RemoveAllByteTags ();
      uint8_t buffer[200];
      uint32_t pktlength = packet->CopyData(buffer, 200);
      NS_LOG_INFO ("Pkt length "<<pktlength<<" contents " << buffer );

      uint32_t requestDataSize = 0;
      for (uint32_t tmp = 0; tmp < pktlength - 1; tmp++)
      {
        requestDataSize *= 10;
        requestDataSize += buffer[tmp]-48;
      }
      NS_LOG_INFO ("Requested data size "<<requestDataSize );


      uint32_t from_address = InetSocketAddress::ConvertFrom(from).GetIpv4 ().Get();
      uint32_t from_port = InetSocketAddress::ConvertFrom (from).GetPort ();
      NS_LOG_INFO ( from_address<< " "<<from_port );


      bool dataPktPathSeed_fix = true;
      uint32_t randomseed;
      uint32_t srcNodeID = m_socket->GetNode()->GetId(); //The ID of the node that this socket is associated with
      if (dataPktPathSeed_fix)
      {
        randomseed = from_address/256*257 + from_port*17 + srcNodeID;
      }
      else
      {
        randomseed = from_address/256*257 + from_port*17 + srcNodeID + time (NULL);
      }

      // Update the remainDataSize list
      if (m_fromIPAddr.empty())
      {
        m_fromIPAddr.push_front (from_address);
        m_fromPort.push_front (from_port);
        m_remainDataSize.push_front (requestDataSize);
        uint8_t *path = new uint8_t[11];
        srand(randomseed);
        for (uint32_t tmpIndex = 0; tmpIndex < 11; tmpIndex++)
        {
          path[tmpIndex] = rand()%256;
        }
        m_path.push_front (path);
      }
      else
      {
        std::list<uint32_t>::iterator m_fromIPAddr_it = m_fromIPAddr.begin();
        std::list<uint32_t>::iterator m_fromPort_it = m_fromPort.begin();
        std::list<uint32_t>::iterator m_remainDataSize_it = m_remainDataSize.begin();
        std::list<uint8_t*>::iterator m_path_it = m_path.begin();
        bool found = true;
        while ( ( *m_fromIPAddr_it != from_address ) || ( *m_fromPort_it != from_port ) )
        {
          if (m_fromIPAddr_it == m_fromIPAddr.end())
          {
            found = false;
            break;
          }
          ++m_fromIPAddr_it;
          ++m_fromPort_it;
          ++m_remainDataSize_it;
          ++m_path_it;
        }
        if (!found)
        {
          m_fromIPAddr.push_front (from_address);
          m_fromPort.push_front (from_port);
          m_remainDataSize.push_front (requestDataSize);
          uint8_t *path = new uint8_t[11];
          srand(randomseed);
          for (uint32_t tmpIndex = 0; tmpIndex < 11; tmpIndex++)
          {
            path[tmpIndex] = rand()%256;
          }
          m_path.push_front (path);
        }
        else // Found
        {
          if (*m_remainDataSize_it != 0)
          {
//            std::cout<<"New data request before the current one is completed in same socket"<<std::endl;
          }
          *m_remainDataSize_it = requestDataSize;
        }
      }

/*      std::list<uint32_t>::iterator itor;
      for (itor = m_fromIPAddr.begin(); itor != m_fromIPAddr.end(); ++itor) {
        std::cout << "m_fromIPAddr value = "  << *itor << std::endl;
      } 
      for (itor = m_fromPort.begin(); itor != m_fromPort.end(); ++itor) {
        std::cout << "m_fromPort value = "  << *itor << std::endl;
      } 
      for (itor = m_remainDataSize.begin(); itor != m_remainDataSize.end(); ++itor) {
        std::cout << "m_remainDataSize value = "  << *itor << std::endl;
      }
*/
      NS_LOG_INFO ("Sending requested data");
      SendData (socket);
    }
}

void PacketSink::SendData (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this);

  Address from;
  socket->GetPeerName (from);  

  uint32_t from_address = InetSocketAddress::ConvertFrom(from).GetIpv4 ().Get();
  uint32_t from_port = InetSocketAddress::ConvertFrom (from).GetPort ();

//  std::cout<<from_address<<" "<<from_port<<std::endl;
  std::list<uint32_t>::iterator m_fromIPAddr_it = m_fromIPAddr.begin();
  std::list<uint32_t>::iterator m_fromPort_it = m_fromPort.begin();
  std::list<uint32_t>::iterator m_remainDataSize_it = m_remainDataSize.begin();
  std::list<uint8_t*>::iterator m_path_it = m_path.begin();
  bool found = true;
  while ( ( *m_fromIPAddr_it != from_address ) || ( *m_fromPort_it != from_port ) )
  {
    if (m_fromIPAddr_it == m_fromIPAddr.end())
    {
      found = false;
      break;
    }
    ++m_fromIPAddr_it;
    ++m_fromPort_it;
    ++m_remainDataSize_it;
    ++m_path_it;
  }
  if (!found)
  {
//    std::cout<<"Error! When sending requested data"<<std::endl;
    return;
  }

  // *m_remainDataSize_it = the remaining data the socket need to send

  // !!!!!!!!!!!!!!!!!!!!!
  // Need to be fixed
  uint32_t sendSize = 1460; 
  // !!!!!!!!!!!!!!!!!!!!!

  uint32_t payloadBufferSize = 1500;
  uint8_t pktPayloadBuffer[payloadBufferSize];
  for (uint32_t tmpIndex = 0; tmpIndex < 11; tmpIndex++)
  {
    pktPayloadBuffer[tmpIndex] = (*m_path_it)[tmpIndex];
  }
  for (uint32_t tmpIndex = 11; tmpIndex < payloadBufferSize; tmpIndex++)
  {
    pktPayloadBuffer[tmpIndex] = 0;
  }

  while (*m_remainDataSize_it > 0)
    { // Time to send more

      // Make sure we don't send too many
      uint32_t toSend = std::min (sendSize, *m_remainDataSize_it);

      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> (pktPayloadBuffer, toSend);
      m_txTrace (packet);
      int actual = socket->Send (packet);
      if (actual > 0)
        {
          *m_remainDataSize_it = *m_remainDataSize_it - actual;
        }
      // We exit this loop when actual < toSend as the send side
      // buffer is full. The "DataSent" callback will pop when
      // some buffer space has freed ip.
      if ((unsigned)actual != toSend)
        {
//          std::cout<<"Send side buffer is full!"<<std::endl;
          break;
        }
    }

  // Check if time to close (all sent)
  if (*m_remainDataSize_it == 0)
    {
      socket->Close ();
    }

}

void PacketSink::DataSend (Ptr<Socket> socket, uint32_t)
{
  NS_LOG_FUNCTION (this);

//  std::cout<<"DataSend is called!"<<std::endl;

  Simulator::ScheduleNow (&PacketSink::SendData, this ,socket);
}


void PacketSink::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

/*  socket->Close();
  socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > () );
  socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket> > (),
      MakeNullCallback<void, Ptr<Socket> > () );
  NS_LOG_INFO ("Close success");

  std::list<Ptr<Socket> >::iterator index;
  index = m_socketList.begin();
  while (*index != socket)
  {
    ++index;
  }
  m_socketList.erase (index);*/
}
 
void PacketSink::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
 

void PacketSink::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback (&PacketSink::HandleRead, this));
  s->SetSendCallback (MakeCallback (&PacketSink::DataSend, this));
  m_socketList.push_back (s);
}

} // Namespace ns3
