#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : 
    _output(capacity), 
    _capacity(capacity),
    _head_index(0),
    _eof(false) ,
    _unreassembled(){}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // if read, update window 
    _head_index=_output.bytes_read();

    if(eof) _eof = true;
    if(index >= _head_index+_capacity || index+data.size() < _head_index) return;

    // first to add to unreassembled
    // after merging, data ordered into separated slices with no overlaping and adjacence 
    string merged_data=data;
    size_t merged_index=index;
    for(auto iter = _unreassembled.begin();iter!=_unreassembled.end();)
	{
        // if have overlaping (when <) or adjacence (when ==)
		if(max(iter->first,merged_index)<= min(iter->first+iter->second.size(),merged_index+merged_data.size()))
		{
            // start pos in substr use min to prevent out of data bound
            // no upper bound, so substr has just one param
			if(iter->first<merged_index) // need to change merged index
			{
				merged_data=iter->second+merged_data.substr(min(iter->first+iter->second.size()-merged_index,merged_data.size()));
				merged_index=iter->first;
			}
			else
			{
				merged_data=merged_data+iter->second.substr(min(merged_index+merged_data.size()-iter->first,iter->second.size()));
			}
            // merging causes merged block disappeare
			iter=_unreassembled.erase(iter); // iter = next now
		}
		else
			iter++;
	}
    _unreassembled.insert(std::make_pair(merged_index,merged_data));

    // put reassembleable data to output
    // after find key, just need put once for merged unreassembled map before
    for(auto iter=_unreassembled.begin();iter!=_unreassembled.end()&&iter->first<=_head_index+_output.buffer_size();)
    {
        string write_data;
        write_data=iter->second.substr(min(_head_index+_output.buffer_size()-iter->first,iter->second.size()),_capacity-_output.buffer_size());
        _output.write(write_data);

        // part of data is out of bound
	    if(iter->first+iter->second.size()>_head_index+_capacity)
	    {
		    string unwritten_data = iter->second.substr(_head_index+_capacity-iter->first);
		    _unreassembled.insert(std::make_pair(_head_index+_capacity,unwritten_data));
	    }

	    _unreassembled.erase(iter);

        //if eof is out of bound , empty ==false
        if(_eof&&empty())
            _output.end_input();
    	return;
    }
}

size_t StreamReassembler::unassembled_bytes() const { 
    size_t unass_bytes=0;

    for(auto it: _unreassembled)
    {
        unass_bytes+=it.second.size();
    }

    return unass_bytes; 
}

bool StreamReassembler::empty() const { return unassembled_bytes()==0; }

size_t StreamReassembler::ack_index() const { return _output.bytes_read()+_output.buffer_size(); }