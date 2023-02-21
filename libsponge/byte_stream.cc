#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):  _error(false),
                                                capacity_(capacity),
                                                bytes_stored_(""),
                                                bytes_written_(0),
                                                bytes_read_(0),                       
                                                end_(false) { }

size_t ByteStream::write(const string &data) {
    size_t store_length;
    if(data.size()+bytes_stored_.size()>capacity_){
        store_length=capacity_-bytes_stored_.size();
        bytes_stored_+=data.substr(0,capacity_-bytes_stored_.size());
    } else {
        store_length=data.size();
        bytes_stored_+=data;
    }
    bytes_written_+=store_length;
    return store_length;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string peek_string;
    if(len>bytes_stored_.size()){
        peek_string=bytes_stored_;
        // bytes_stored_.clear();
    } else {
        peek_string=bytes_stored_.substr(0,len);
        // bytes_stored_=bytes_stored_.substr(len);
    }
    return peek_string;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    if(len>bytes_stored_.size()){
        bytes_read_+=bytes_stored_.size();
        bytes_stored_.clear();
    } else {
        bytes_read_+=len;
        bytes_stored_=bytes_stored_.substr(len);
    }
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string read_string = peek_output(len);
    pop_output(len);
    return read_string;
}

void ByteStream::end_input() {
    end_=true;
}

bool ByteStream::input_ended() const { return end_; }

size_t ByteStream::buffer_size() const { return bytes_stored_.size(); }

bool ByteStream::buffer_empty() const { return bytes_stored_.empty(); }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return bytes_written_; }

size_t ByteStream::bytes_read() const { return bytes_read_; }

size_t ByteStream::remaining_capacity() const { return capacity_-bytes_stored_.size(); }
