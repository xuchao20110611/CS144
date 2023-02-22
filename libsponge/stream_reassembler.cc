#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity),
                                                            start_(0),
                                                            isend_(false),
                                                            index_to_string_() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    string buf=data;
    uint64_t left=index;
    uint64_t right=index+buf.size()-1;
    if(right<start_||data.empty()){
        if(eof){
            _output.end_input();
        }
        send_to_output();
        return ;
    }
    if(left<start_){
        buf=buf.substr(start_-left);
        left=start_;
    }
    if(right>start_+_capacity-1){
        buf=buf.substr(0,start_+_capacity-left);
        right=start_+_capacity-1;
    } else if(eof){
        isend_=true;
    }
    // traverse the whole map, merge the overlaped bunches
    map<uint64_t,string>::iterator map_it=index_to_string_.begin();
    map<uint64_t,string>::reverse_iterator map_rbegin=index_to_string_.rbegin();
    if(map_it==index_to_string_.end()) {
        // empty map
        index_to_string_[left]=buf;
    } else if(right<map_it->first-1){
        index_to_string_[left]=buf;
    } else if(left>map_rbegin->first+map_rbegin->second.size()){
        index_to_string_[left]=buf;
    } else {
        bool isin=false;
        string segregated_str = "";
        uint64_t newstart;
        while(map_it!=index_to_string_.end()) {
            if(isin){
                if(right<map_it->first-1){
                    isin=false;
                    index_to_string_[newstart]=segregated_str;
                    break;
                } else {
                    segregated_str=merge_string(segregated_str,newstart,map_it->second,map_it->first);
                    map_it=index_to_string_.erase(map_it);
                }
            } else {
                if(map_it->first>right+1){
                    index_to_string_[left]=buf;
                    break;
                } else if(map_it->first+map_it->second.size()>=left){
                    isin=true;
                    newstart=min(map_it->first,left);
                    segregated_str=merge_string(map_it->second, map_it->first, buf, left);
                    map_it=index_to_string_.erase(map_it);
                } else {
                    map_it++;
                }
            }
        }
        if(isin && !segregated_str.empty()){
            index_to_string_[newstart]=segregated_str;
        }
        if(segregated_str[0]>='0'&&segregated_str[0]<='9')
            cout<<"segregated: "<<segregated_str<<endl;
        // cout<<"from index: "<<newstart <<" "<<segregated_str.size()<<endl;
    }
    send_to_output();

    return ;
    
    
}

void StreamReassembler::send_to_output(){
    // check and send to out bytestream
    // @eof
    // @start_
    if(empty()){
        _output.end_input();
        return ;
    } else if(index_to_string_.empty()){
        return ;
    }
    if(index_to_string_.begin()->first==start_){
        size_t write_byte_num=_output.write(index_to_string_.begin()->second);
        if(write_byte_num==index_to_string_.begin()->second.size()){
            index_to_string_.erase(index_to_string_.begin());
        } else if(write_byte_num>0){
            // write part of the string into the stream
            index_to_string_.begin()->second=index_to_string_.begin()->second.substr(write_byte_num);
        }
        start_+=write_byte_num;
    }
    if(empty()){
        _output.end_input();
    }
}
size_t StreamReassembler::unassembled_bytes() const { 
    size_t unassembled_byte_num=0;
    for (const auto & byte:index_to_string_){
        unassembled_byte_num+=byte.second.size();
    }
    return unassembled_byte_num;
 }

bool StreamReassembler::empty() const {
    return index_to_string_.empty() && isend_;
}

string StreamReassembler::merge_string(string & str1, uint64_t index1,string & str2, uint64_t index2){
    uint64_t left=min(index1,index2);
    uint64_t right=max(index1+str1.size()-1,index2+str2.size()-1);
    string merged_str(right-left+1,'?');
    for(uint64_t i=0;i<merged_str.size();i++){
        uint64_t pos=left+i;
        if(index1<=pos && index1+str1.size()-1>=pos){
            merged_str[i]=str1[pos-index1];
        } else {
            merged_str[i]=str2[pos-index2];
        }
        if(index1<=pos && index1+str1.size()-1>=pos &&index2<=pos && index2+str2.size()-1>=pos){
            if(str1[pos-index1]!=str2[pos-index2]){
                // throw(double(5.6));
            }
        }
    }
    return merged_str;
    // return "";
}