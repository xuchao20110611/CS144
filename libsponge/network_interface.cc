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

    DUMMY_CODE(dgram, next_hop, next_hop_ip);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    const EthernetHeader & e_header=frame.header();
    if(e_header.dst!=_ethernet_address && e_header.dst!=ETHERNET_BROADCAST){
        // not destined in this interface
        return {};
    }
    if(e_header.type==EthernetHeader::TYPE_IPv4){
        InternetDatagram ip_gram;
        
        if(ip_gram.parse(frame.payload().concatenate())==ParseResult::NoError){
            return ip_gram;
        } else {
            cout<<"NetworkInterface::recv_frame: parse ipv4 with error"<<endl;
            return {};
        }

    } else if(e_header.type==EthernetHeader::TYPE_ARP){
        ARPMessage arp_gram;
        if(arp_gram.parse(frame.payload().concatenate())==ParseResult::NoError){
            EthernetAddress sender_eth=arp_gram.sender_ethernet_address;
            uint32_t sender_ip=arp_gram.sender_ip_address;
            std::string sender_ip_str=Address::from_ipv4_numeric(sender_ip).ip();
            ip2time_[sender_ip_str]=0;
            ip2eth_[sender_ip_str]=sender_eth;
            if(ARPMessage::OPCODE_REQUEST==arp_gram.opcode){
                ARPMessage reply_arp_gram=arp_gram;
                reply_arp_gram.opcode=ARPMessage::OPCODE_REPLY;
                reply_arp_gram.sender_ethernet_address=_ethernet_address;
                reply_arp_gram.sender_ip_address=_ip_address.ipv4_numeric();
                reply_arp_gram.target_ethernet_address=sender_eth;
                reply_arp_gram.target_ip_address=sender_ip;

                EthernetFrame reply_eth_frame;
                reply_eth_frame.payload()=reply_arp_gram.serialize();
                reply_eth_frame.header().type=EthernetHeader::TYPE_ARP;
                reply_eth_frame.header().dst=e_header.src;
                reply_eth_frame.header().src=e_header.dst;

                _frames_out.emplace(reply_eth_frame);
            }
            
        } else {
            cout<<"NetworkInterface::recv_frame: parse arp with error"<<endl;
            
        }
        return {};
        
    } else {
        cerr<<"NetworkInterface::recv_frame: unsupported ethernet type"<<endl;
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }
