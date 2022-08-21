#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const {cerr<<"3"<<endl; return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const {cerr<<"4"<<endl; return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {cerr<<"recv"<<endl;
    if(!_active) return;
    _time_since_last_segment_received = 0; 

    // check if the RST has been set
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }

    // if seg.ack is set, it's for sender
    // if length in sequence space > 0, it's for receier
    // it can both for sender and receiver

    // for sender 
    // check if the ACK has been set(receive from remote receiver)
    if (seg.header().ack)
        _sender.ack_received(seg.header().ackno, seg.header().win);

    // for receiver
    // give it to receiver(receive from remote sender)
    // if it is from receiver just for ack (ack==true,data=='') ,it doesn't disrupt receiver for no data
    _receiver.segment_received(seg);
    // receive need to send ack by sender's help
    if (seg.length_in_sequence_space() > 0)
    {
        _sender.fill_window();
        if(_sender.segments_out().empty())
            _sender.send_empty_segment();
    }
        // avoid _sender's out is empty

    // responding to a “keep-alive” segment.
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0)
        && seg.header().seqno == _receiver.ackno().value() - 1)
        _sender.send_empty_segment();

    send_segment(); if(_sender.win()==0&&seg.header().ack!=0)_segments_out=std::queue<TCPSegment>();

    // check if need to linger
    if (inbound_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) { cerr<<"write"<<endl;
    if (!data.size()) return 0;
    size_t actually_write = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segment();
    return actually_write;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {cerr<<"tick"<<endl;if(_sender.win()==0)_segments_out=std::queue<TCPSegment>();
    if(!_active) return;

    // tick the sender to do the retransmit
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    send_segment();

    // abort the connection,sent a empty rst
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        TCPSegment segment;
        segment.header().rst=true;
        //clear tcpsegments so that the first is rst
        _segments_out=std::queue<TCPSegment>();
        _segments_out.push(segment);
        _active = false;
    }

    // check if done
    if (inbound_ended() && outbound_ended()) {
        if (!_linger_after_streams_finish) {
            //_segments_out=std::queue<TCPSegment>();
            _active = false;
        } else if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            //_segments_out=std::queue<TCPSegment>();
            _active = false;
        }
    }
}

void TCPConnection::end_input_stream() {cerr<<"end"<<endl;
    _sender.stream_in().end_input();
    // send FIN
    _sender.fill_window();
    send_segment();
}

void TCPConnection::connect() {cerr<<"conn"<<endl;
    // send SYN
    _sender.fill_window();
    send_segment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            
            TCPSegment segment;
            segment.header().rst=true;
            //clear tcpsegments so that the first is rst
            _segments_out=std::queue<TCPSegment>();
            _segments_out.push(segment);_active = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_segment() {
    while (!_sender.segments_out().empty()) {
        TCPSegment segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        // ask receiver for ack and window size
        // when syn is not received, ackno=null, receiver can't reply ack
        optional<WrappingInt32> ackno = _receiver.ackno();
        if (ackno.has_value()) {
            segment.header().ack = true;
            segment.header().ackno = ackno.value();
        }
        segment.header().win = static_cast<uint16_t>(_receiver.window_size());
        _segments_out.push(segment);
    }
}

// prereqs1 : The inbound stream has been fully assembled and has ended.
bool TCPConnection::inbound_ended() {
    return _receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended();
}
// prereqs2 : The outbound stream has been ended by the local application and fully sent (including
// the fact that it ended, i.e. a segment with fin ) to the remote peer.
// prereqs3 : The outbound stream has been fully acknowledged by the remote peer.
bool TCPConnection::outbound_ended() {
    return _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
           _sender.bytes_in_flight() == 0;
}