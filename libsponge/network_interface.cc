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
    
    
    if(ip2eth_.find(next_hop_ip)==ip2eth_.end()){
        // no eth stored
        ip2dgram_[next_hop_ip].push_back(dgram);
        if(arp_request_ip2time_.find(next_hop_ip)==arp_request_ip2time_.end()){
            struct ARPMessage arp_request_gram=make_ARPMessage(
                ARPMessage::OPCODE_REQUEST,
                _ethernet_address,
                _ip_address.ipv4_numeric(),
                EthernetAddress{},
                next_hop_ip
            );
            arp_request_ip2time_[next_hop_ip]=0;
            send_arpdgram(arp_request_gram, ETHERNET_BROADCAST,_ethernet_address);
        }
        
        
    } else{
        send_ipv4gram(dgram,ip2eth_[next_hop_ip],_ethernet_address);
        
    }

    

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
        struct ARPMessage arp_gram;
        if(arp_gram.parse(frame.payload().concatenate())==ParseResult::NoError){
            EthernetAddress sender_eth=arp_gram.sender_ethernet_address;
            uint32_t sender_ip=arp_gram.sender_ip_address;
            // std::string sender_ip_str=Address::from_ipv4_numeric(sender_ip).ip();
            ip2time_[sender_ip]=0;
            ip2eth_[sender_ip]=sender_eth;
            arp_request_ip2time_.erase(sender_ip);
            if(ARPMessage::OPCODE_REQUEST==arp_gram.opcode){
                if(arp_gram.target_ip_address==_ip_address.ipv4_numeric()){
                    struct ARPMessage reply_arp_gram=make_ARPMessage(
                        ARPMessage::OPCODE_REPLY,
                        _ethernet_address,
                        _ip_address.ipv4_numeric(),
                        sender_eth,
                        sender_ip
                    );
                    
                    send_arpdgram(reply_arp_gram, e_header.src, _ethernet_address);
                }               
                
            }
            if(ip2dgram_.find(sender_ip)!=ip2dgram_.end()){
                for(const InternetDatagram & dgram:ip2dgram_[sender_ip]){
                    send_ipv4gram(dgram, sender_eth,_ethernet_address);
                }
                ip2dgram_.erase(sender_ip);
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
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    std::unordered_map<uint32_t, size_t>::iterator it=ip2time_.begin();
    while(it!=ip2time_.end()){
        it->second+=ms_since_last_tick;
        if(it->second>=30000){
            ip2eth_.erase(it->first);
            it=ip2time_.erase(it);
        } else {
            it++;
        }
    }
    it=arp_request_ip2time_.begin();
    while(it!=arp_request_ip2time_.end()){
        it->second+=ms_since_last_tick;
        if(it->second>5000){
            it=arp_request_ip2time_.erase(it);
        } else {
            it++;
        }
    }
}

void NetworkInterface::send_ipv4gram(const InternetDatagram & dgram, EthernetAddress dst, EthernetAddress src){
    EthernetFrame send_eth_frame;
    send_eth_frame.payload()=dgram.serialize();
    send_eth_frame.header().type=EthernetHeader::TYPE_IPv4;
    send_eth_frame.header().src=src;
    send_eth_frame.header().dst=dst;
    _frames_out.emplace(send_eth_frame);
}

void NetworkInterface::send_arpdgram(const struct ARPMessage & dgram, EthernetAddress dst, EthernetAddress src){
    EthernetFrame arp_frame;
    arp_frame.header().type=EthernetHeader::TYPE_ARP;
    arp_frame.header().src=src;
    arp_frame.header().dst=dst;
    arp_frame.payload()=dgram.serialize();
    _frames_out.emplace(arp_frame);
}

ARPMessage NetworkInterface::make_ARPMessage(uint16_t opcode, EthernetAddress sender_ethernet_address, uint32_t sender_ip_address, EthernetAddress target_ethernet_address, uint32_t target_ip_address){
    struct ARPMessage arp_gram;
    arp_gram.opcode=opcode;
    arp_gram.sender_ethernet_address=sender_ethernet_address;
    arp_gram.sender_ip_address=sender_ip_address;
    arp_gram.target_ethernet_address=target_ethernet_address;
    arp_gram.target_ip_address=target_ip_address;
    return arp_gram;
}
