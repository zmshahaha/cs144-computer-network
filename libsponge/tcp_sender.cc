#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

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
    , _retransmission_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // sent a SYN before sent other segment
    if (!_syn_flag) {
        TCPSegment seg;
        seg.header().syn = true;
        send_segment(seg);
        _syn_flag = true;
        return;
    }

    /*
     * If the receiver has announced a window size of zero, 
     * the fill window method should act like the window size is one. 
     * The sender might end up sending a single byte that gets rejected 
     * (and not acknowledged) by the receiver, but this can also provoke 
     * the receiver into sending a new acknowledgment segment where 
     * it reveals that more space has opened up in its window. 
     * Without this, the sender would never learn that 
     * it was allowed to start sending again. 
     */
    size_t window_size = (_window_size>0) ? _window_size:1;

    // window start from _recv_ackno not the same concept as lab2
    // when window isn't full and never sent FIN
    // when window_size not set in ack_recv func, remain < 0
    while (window_size > _next_seqno - _recv_ackno && !_fin_flag) {
        size_t size = min(TCPConfig::MAX_PAYLOAD_SIZE,window_size - (_next_seqno - _recv_ackno));
        TCPSegment seg;
        string str = _stream.read(size);
        seg.payload() = Buffer(std::move(str));
        // add FIN
        if (seg.length_in_sequence_space() < window_size && _stream.eof()) {
            seg.header().fin = true;
            _fin_flag = true;
        }
        // stream is empty( still need send msg when windowsize==0 )
        if (seg.length_in_sequence_space() == 0 && _window_size != 0)
            return;
        send_segment(seg);
        // when stream is empty and _window_size=0,it can be a infinite loop because _next_seqno 
        // can't go on and always equal to recv_seqno.so need to avoid
        if(_window_size==0)
            return;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _recv_ackno);
    // out of window, invalid ackno
    if (abs_ackno > _next_seqno)
        return;

    // if ackno is legal, modify _window_size before return
    _window_size = window_size;

    // not accept new message(not pop outstanding)
    if (abs_ackno <= _recv_ackno)
        return;

    // accept new message and window go forward
    _recv_ackno = abs_ackno;
    // pop all elment before ackno
    while (!_segments_outstanding.empty()) {
        TCPSegment seg = _segments_outstanding.front();
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= abs_ackno) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
        } else
            break;
    }
    
    fill_window();

    _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retransmission = 0;

    // if have other outstanding segment, restart timer
    if (!_segments_outstanding.empty()) {
        _timer_running = true;
        _timer = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer += ms_since_last_tick;
    if (_timer >= _retransmission_timeout && !_segments_outstanding.empty()) {
        // send not acked data just when timeout
        _segments_out.push(_segments_outstanding.front());
        // if not ack syn, the transaction not start,so dont need hurry
        // if ack but win=0, the sender want know when receiver can receive in time
        if(_window_size || _segments_outstanding.front().header().syn )
        {
            _consecutive_retransmission++;
            _retransmission_timeout *= 2;
        }
        _timer_running = true;
        _timer = 0;
    }
    if (_segments_outstanding.empty()) 
        _timer_running = false;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission; }

void TCPSender::send_empty_segment() {
    // empty segment doesn't need store to outstanding queue
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

void TCPSender::send_segment(TCPSegment &seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _segments_outstanding.push(seg);
    _segments_out.push(seg);
    if (!_timer_running) {  // start timer
        _timer_running = true;
        _timer = 0;
    }
}
