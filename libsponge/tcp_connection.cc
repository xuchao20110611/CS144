#include "tcp_connection.hh"

#include <iostream>
#include <climits>
// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return time_last_segment_received_;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // cout<<"TCPConnection::segment_received: "<<endl;
    const TCPHeader & receive_header=seg.header();
    time_last_segment_received_=0;
    if(receive_header.rst){
        set_RST();
        return ;
    }
    
    _receiver.segment_received(seg);
    if(_receiver.stream_out().eof() && !is_fin_sent_){
        //  the TCPConnection's inbound stream ends before
        //  the TCPConnection has ever sent a fin segment
        _linger_after_streams_finish=false;
    }
    
    if(receive_header.ack){
        _sender.ack_received(receive_header.ackno,receive_header.win);
    }
    if(seg.length_in_sequence_space()>0){
        // sent in reply, to reflect an update in the ackno and window size.
        _sender.send_empty_segment();
    } else if(receive_header.syn){
        _sender.send_empty_segment();
        is_active_=true;
    } else if((_receiver.ackno().has_value()) 
        && (seg.length_in_sequence_space() == 0)
        && (seg.header().seqno == _receiver.ackno().value() - 1)){
        _sender.send_empty_segment();
    }
    send_TCPSegment_from_sender();
}

bool TCPConnection::active() const { return is_active_; }

size_t TCPConnection::write(const string &data) {
    size_t write_len=_sender.stream_in().write(data);
    _sender.fill_window();
    send_TCPSegment_from_sender();
    return write_len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // cout<<"TCPConnection::tick: ms_since_last_tick: "<<ms_since_last_tick<<endl;
    _sender.tick(ms_since_last_tick);
    time_last_segment_received_+=ms_since_last_tick;
    if(_sender.consecutive_retransmissions()>TCPConfig::MAX_RETX_ATTEMPTS){
        send_RST();
        return ;
    }
    send_TCPSegment_from_sender();
    bool prereq4=false; //  Prereq 4
    if(_linger_after_streams_finish){
        if(time_last_segment_received_>=10*static_cast<size_t>(_cfg.rt_timeout)){
            prereq4=true;
        }
    } else {
        prereq4=true;
    }
    if(inbound_stream().eof()
        && (_sender.bytes_in_flight()==0 && _sender.stream_in().eof())
        && prereq4){
        is_active_=false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_TCPSegment_from_sender();
}

void TCPConnection::connect() {
    // std::cout<<"TCPConnection::connect(): start connecting"<<endl;
    _sender.fill_window();
    send_TCPSegment_from_sender();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_RST();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}


void TCPConnection::send_TCPSegment_from_sender(){
    // cout<<"TCPConnection::send_TCPSegment_from_sender()"<<endl;
    while(!_sender.segments_out().empty()){
        TCPSegment send_seg=_sender.segments_out().front();
        _sender.segments_out().pop();
        if(_receiver.ackno().has_value()){
            send_seg.header().ack=true;
            send_seg.header().ackno=_receiver.ackno().value();
            size_t win=_receiver.window_size();
            if(win>USHRT_MAX){
                win=USHRT_MAX;
            }
            send_seg.header().win=static_cast<uint16_t>(win);
        }
        // cout<<"TCPConnection::send_TCPSegment_from_sender(): send sth"<<endl;
        if(send_seg.header().fin){
            is_fin_sent_=true;
        }
        _segments_out.emplace(send_seg);

    }
}

void TCPConnection::send_RST(){
    send_TCPSegment_from_sender(); // clear cache
    _sender.send_empty_segment();
    TCPSegment send_seg=_sender.segments_out().front();
    _sender.segments_out().pop();
    send_seg.header().rst=true;
    _segments_out.emplace(send_seg);
    set_RST();
    
}

void TCPConnection::set_RST(){
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    is_active_=false;
}