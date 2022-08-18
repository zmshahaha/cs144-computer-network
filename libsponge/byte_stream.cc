#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) :
    _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t can_write = _capacity - _buffer.size();
    size_t real_write = min(can_write, data.length());
    
    _buffer+=data.substr(0,real_write);
    _written_bytes += real_write;
    
    return real_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    return _buffer.substr(0,min(len, _buffer.size()));
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    if (len > _buffer.size()) {
        set_error();
        return;
    }
    _buffer=_buffer.substr(len);
    _read_bytes += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string read_result=peek_output(len);
    pop_output(read_result.size());
    return read_result;
}

void ByteStream::end_input() { _is_write_end = true; }

bool ByteStream::input_ended() const { return _is_write_end; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return _buffer.empty() && _is_write_end; }

size_t ByteStream::bytes_written() const { return _written_bytes; }

size_t ByteStream::bytes_read() const { return _read_bytes; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
