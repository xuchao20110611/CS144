#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    WrappingInt32 seqno=seg.header().seqno;
    if(seg.header().syn){
        setStart(seqno);
        seqno = WrappingInt32{seqno.raw_value()+1};
        // return ; // only set the start and return
    }
    if(!isstart_){
        // directly return as there is no isn
        return;
    }
    uint64_t absolute_checkpoint_64=1+_reassembler.stream_out().bytes_written();
    uint64_t absolute_index_64=unwrap(seqno, isn_32_,absolute_checkpoint_64);
    string data{seg.payload().str()};
    // appear bad algo
    if(absolute_index_64==0 && !seg.header().syn){
        // it should be SYN
        // throw("bad algo: segment received");
        // cout<<"bad algo: segment received"<<endl;
    }
    if(absolute_index_64==0){
        // send with index which is supposed to be SYN
        cout<<"TCPReceiver::segment_received: absolute_index_64==0"<<endl;
        return;
    }
    // cout<<"absolute_index_64 in seg received: "<<absolute_index_64<<endl;
    // cout<<"write finish: "<< seg.header().fin<<endl;
    _reassembler.push_substring(data, absolute_index_64-1, seg.header().fin);
    
}

void TCPReceiver::setStart(WrappingInt32 isn_32){
    isn_32_=isn_32;
    isstart_=true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(!isstart_){
        return std::nullopt;
    }
    uint64_t absolute_index_64=1+_reassembler.stream_out().bytes_written();
    if(_reassembler.empty()){
        absolute_index_64+=1;
    }
    // the number of bytes written equals the stream index of the next wanted byte
    WrappingInt32 noend_ackno = wrap(absolute_index_64, isn_32_);
    // if(_reassembler.empty()){
    //     return WrappingInt32{noend_ackno.raw_value()+1};
    // } else {
    //     return noend_ackno;
    // }
    // cout<<"bytes_written: "<<_reassembler.stream_out().bytes_written()
    //     <<" absolute_index_64: "<<absolute_index_64
    //     <<" is empty: "<<_reassembler.empty()
    //     <<" is end set: "<<_reassembler.endtest()
    //     <<" is empty map: "<<_reassembler.emptytest()<<endl;
    return noend_ackno;
    
}

size_t TCPReceiver::window_size() const {
    size_t unassembled_bytes_index=_reassembler.stream_out().bytes_written();
    size_t unread_bytes_index=_reassembler.stream_out().bytes_read();
    return _capacity-(unassembled_bytes_index-unread_bytes_index);
}
