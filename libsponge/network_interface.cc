#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.payload() = dgram.serialize();  //diff between with func move?
    if (_mac_ip_cache.count(next_hop_ip)) {
        frame.header().dst = _mac_ip_cache[next_hop_ip].mac;
        _frames_out.push(frame);
    } else {
        _pend_frames[next_hop_ip].pend_ip_frames.push(frame);
        send_arp_request();
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // if is valid frame
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address)
        return nullopt;

    // if is a valid ipv4 frame
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) == ParseResult::NoError)
            return dgram;
        return nullopt;
    }

    // if is a arp
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage msg;
        if (msg.parse(frame.payload()) == ParseResult::NoError) {
            uint32_t ip = msg.sender_ip_address;
            // update cache
            _mac_ip_cache[ip].mac = msg.sender_ethernet_address;
            _mac_ip_cache[ip].time_to_erase = _timer + MAX_CACHE_TIME;
            // push which is received ip and update pend frame
            while (!_pend_frames[ip].pend_ip_frames.empty())
            {
                _pend_frames[ip].pend_ip_frames.front().header().dst=_mac_ip_cache[ip].mac;
                _frames_out.push(_pend_frames[ip].pend_ip_frames.front());
                _pend_frames[ip].pend_ip_frames.pop();
            }
            _pend_frames.erase(ip);

            if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == _ip_address.ipv4_numeric())
                send_helper(ARPMessage::OPCODE_REPLY,msg.sender_ip_address,msg.sender_ethernet_address);
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _timer += ms_since_last_tick;
    for (auto it = _mac_ip_cache.begin(); it != _mac_ip_cache.end();)
    {
        if(it->second.time_to_erase<_timer)
            // don't forget to return to iter
            it=_mac_ip_cache.erase(it);
        else
            it++;
    }

    send_arp_request();
}

void NetworkInterface::send_arp_request()
{
    // why auto it : _pend failed??
    for(auto it=_pend_frames.begin(); it!=_pend_frames.end();it++)
    {
        // the first frame pended in this ip
        // init arq_req_time and send an arp req
        if(it->second.arp_req_time<=_timer){
            uint32_t ip=it->first;
            send_helper(ARPMessage::OPCODE_REQUEST,ip);
            it->second.arp_req_time=_timer+MAX_RETX_WAITING_TIME;
        }
    }
}

void NetworkInterface::send_helper(const uint16_t& arp_opcode,
                                   const uint32_t& target_ip_address,
                                   const EthernetAddress& target_ethernet_address)
{
    ARPMessage msg;
    EthernetFrame arp_frame;
    msg.opcode = arp_opcode;
    msg.sender_ethernet_address = _ethernet_address;
    msg.sender_ip_address = _ip_address.ipv4_numeric();
    if(arp_opcode==ARPMessage::OPCODE_REQUEST)
        msg.target_ethernet_address = {0,0,0,0,0,0};
    else
        msg.target_ethernet_address = target_ethernet_address;
    msg.target_ip_address = target_ip_address;
    arp_frame.header().type = EthernetHeader::TYPE_ARP;
    arp_frame.header().src = _ethernet_address;
    arp_frame.header().dst = target_ethernet_address;
    arp_frame.payload() = msg.serialize();  //diff between with func move?
    _frames_out.push(arp_frame);
}