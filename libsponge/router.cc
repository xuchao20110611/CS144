#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    uint32_t matched_prefix=route_prefix;
    uint8_t useless_bits=32-prefix_length;
    if(useless_bits==32){      
        matched_prefix=0;
    }else {
        matched_prefix=matched_prefix>>useless_bits;       
    }
    matched_prefix_to_prefix_length_[matched_prefix]=prefix_length;
    matched_prefix_to_next_hop_[matched_prefix]=next_hop;
    matched_prefix_to_interface_num_[matched_prefix]=interface_num;
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    if(dgram.header().ttl<=1){
        return;
    }
    dgram.header().ttl--;
    uint8_t match_length=0;
    uint32_t target_ip=dgram.header().dst;
    uint32_t next_hop_ip=0;
    bool is_find=false;
    std::unordered_map<uint32_t,uint8_t>::iterator it=matched_prefix_to_prefix_length_.begin();
    while(it!=matched_prefix_to_prefix_length_.end()){
        uint32_t check_ip=it->first;
        uint8_t check_prefix_length= it->second;
        uint32_t check_target_ip;
        
        if(check_prefix_length==0){
            check_target_ip=0;
        } else {
            check_target_ip=(target_ip>>(32-check_prefix_length));
        }
        if(check_target_ip==check_ip){
            is_find=true;
            if(match_length<=check_prefix_length){
                match_length=check_prefix_length;
                next_hop_ip=check_ip;
            }
        }
        it++;
    }

    if(!is_find){
        // If no routes matched, the router drops the datagram.
        cout<<"Router::route_one_datagram: !is_find"<<endl;
        return ;
    }
    if(matched_prefix_to_next_hop_[next_hop_ip].has_value()){
        interface(matched_prefix_to_interface_num_[next_hop_ip])
        .send_datagram(dgram,matched_prefix_to_next_hop_[next_hop_ip].value());
    } else {
        interface(matched_prefix_to_interface_num_[next_hop_ip])
        .send_datagram(dgram, Address::from_ipv4_numeric(target_ip));
    }
     
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
