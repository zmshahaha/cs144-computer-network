#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader head = seg.header();
    const string data=seg.payload().copy();
    
    if (head.syn==false && _isn_set==false)
        return;
    
    if(head.syn==true)
    {
        _isn=head.seqno;
        _isn_set=true;
        if (head.fin)
            _fin_set = true;
        _reassembler.push_substring(data, 0, _fin_set);
        return;
    }
    
    if(head.fin==true)
        _fin_set=true;

    _checkpoint=unwrap(head.seqno,_isn,_checkpoint)-1;
    _reassembler.push_substring(data,_checkpoint,_fin_set);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(_isn_set==false)return {};
    return wrap(static_cast<uint64_t>(_reassembler.ack_index())+1+(_reassembler.empty() && _fin_set),_isn);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
