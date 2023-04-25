#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , retransmission_timeout_(retx_timeout)
    , absolute_last_(0) // assume one should be sent, 0 is the last byte
    , consecutive_retransmissions_(0)
    , retransmission_timer_(-1) // -1 means not starting
    , is_end_sent_(false)
    , absolute_ackno_(0)
    , window_size_(-1)
    { }

uint64_t TCPSender::bytes_in_flight() const { 
    uint64_t bytes_in_flight=0;
    for(const TCPSegment & outstanding_segment:outstanding_segments_){
        bytes_in_flight+=outstanding_segment.length_in_sequence_space();
    }
    return bytes_in_flight;
 }

void TCPSender::fill_window() {
   
    while(absolute_last_>=_next_seqno && !is_end_sent_){
        // keeps adding TCPsegment until:
        // 1. absolute_last_+1 == _next_seqno
        // or
        // 2. _stream is empty(input_ended()=true)
        
        TCPSegment next_segment;
        size_t len=min(TCPConfig::MAX_PAYLOAD_SIZE,absolute_last_-_next_seqno+1);
        if(_next_seqno==0 && len==absolute_last_-_next_seqno+1){
            len--; // if limited by TCPConfig::MAX_PAYLOAD_SIZE, no need to -1
            next_segment.header().syn=true; // the first time, should add syn flag
            cout<<"TCPSender::fill_window(): send first time"<<endl;
        }
        std::string segment_content{_stream.read(len)};
  
        Buffer segment_content_buffer{std::move(segment_content)};
        next_segment.payload()=segment_content_buffer;
    
        size_t syn_byte= next_segment.header().syn? 1:0;
        if(_stream.eof()&&(next_segment.payload().str().size()+syn_byte<absolute_last_-_next_seqno+1)){
            is_end_sent_=true;
            next_segment.header().fin=true; // when the _stream has been read out
        }
        // cout<<"next_segment.header().fin: "<<next_segment.header().fin<<endl;
        next_segment.header().seqno=next_seqno();
        if(next_segment.length_in_sequence_space()==0){
            // nothing in the input stream should be sent
            break;
        }
        cout<<"TCPSender::fill_window(): send sth"<<endl;
        send_segment(next_segment);
        _next_seqno+=next_segment.length_in_sequence_space();
        // cout<<"in while: _segments_out.empty(): "<<_segments_out.empty()<<std::endl;
        outstanding_segments_.push_back(next_segment);
        
    }
    // cout<<"_segments_out.empty(): "<<_segments_out.empty()<<std::endl;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    // remove the tcp segment in outstanding_segments_ after fully acked
    // reset the retransmission timeout
    std::cout<<"TCPSender::ack_received called with ackno.raw_value(): "<<ackno.raw_value()<<" window_size: "<<window_size<<std::endl;
    retransmission_timeout_=_initial_retransmission_timeout; // Set the RTO back to its initial value.
    consecutive_retransmissions_=0; // Reset the count of consecutive retransmissions back to zero.
    std::vector<TCPSegment> new_outstanding_segments;
    uint64_t absolute_ackno= unwrap(ackno, _isn, _next_seqno);
    window_size_=static_cast<int>(window_size);
    if(absolute_ackno>_next_seqno){
        // impossible ackno
        return ;
    }
    if(absolute_ackno+window_size-1>absolute_last_){
        absolute_last_=absolute_ackno+window_size-1; // update the recorded window right
    }
    if(absolute_ackno<=absolute_ackno_){
        return ;
    } else {
        absolute_ackno_=absolute_ackno;
    }
    // if(absolute_ackno>_next_seqno){
    //     _next_seqno=absolute_ackno; // update the recorded window left
    // }
    
    for(const TCPSegment & outstanding_segment:outstanding_segments_){
        // compare absolute seqno
        WrappingInt32 seqno=outstanding_segment.header().seqno;
        uint64_t absolute_seqno= unwrap(seqno, _isn, _next_seqno);
        uint64_t segment_length=outstanding_segment.length_in_sequence_space();
        if(segment_length==0){
            std::cout<<"bug: empty segment in outstanding vector"<<std::endl;
        }
        uint64_t absolute_last=absolute_seqno+segment_length-1;
        if(absolute_last>=absolute_ackno){
            // still not fully acked
            new_outstanding_segments.push_back(outstanding_segment);
        }
    }
    outstanding_segments_=new_outstanding_segments;
    if(window_size==0 && outstanding_segments_.empty()){
        // send one byte 
        TCPSegment next_segment;
        std::string segment_content{_stream.read(1)};
        Buffer segment_content_buffer{std::move(segment_content)};
        next_segment.payload()=segment_content_buffer;
        next_segment.header().seqno=next_seqno();
        if(next_segment.length_in_sequence_space()==0){
            if(_stream.eof()){
                next_segment.header().fin=true;
            } else {
                // no need to send
                return ;
            }
        }
        _segments_out.emplace(next_segment);
        _next_seqno+=next_segment.length_in_sequence_space();
        outstanding_segments_.push_back(next_segment);
    }
    if(!outstanding_segments_.empty()){
        retransmission_timer_=0;
    } else {
        retransmission_timer_=-1;
    }
    
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if(retransmission_timer_==-1){
        if(!outstanding_segments_.empty()){
            std::cout<<"bug: timer does not start but there exists outstanding segments"<<std::endl;
        }
        
        return ;
    }
    retransmission_timer_+=static_cast<int>(ms_since_last_tick);
    if(static_cast<unsigned int>(retransmission_timer_)>=retransmission_timeout_){
        if(outstanding_segments_.empty()){
            std::cout<<"bug: timer still running when there is no outstanding segment"<<std::endl;
            // should not come here
        } else {
            send_segment(outstanding_segments_.front());
        }
        if(window_size_!=0){
            consecutive_retransmissions_++;
            retransmission_timeout_+=retransmission_timeout_;
        }
        
        retransmission_timer_=0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { 
    return consecutive_retransmissions_;
}

void TCPSender::send_empty_segment() {
    TCPSegment emptyseg{};
    TCPHeader & segheader=emptyseg.header();
    segheader.ackno=next_seqno(); // attention: used for ack
    segheader.seqno=next_seqno();
    // segheader.ack=true;     // not sure, it should not work
    // segheader.win=window_size_; // not sure, it should not work
    send_segment(emptyseg);
}

void TCPSender::send_segment(TCPSegment & seg){
    // send segment and set up the internal state
    _segments_out.emplace(seg);
    if(retransmission_timer_==-1 && seg.length_in_sequence_space()>0){
        retransmission_timer_=0;
    }
}