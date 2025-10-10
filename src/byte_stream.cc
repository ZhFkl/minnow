#include "byte_stream.hh"
#include "debug.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {
  available_capacity_ = capacity;
  buffer_.clear();
  buffer_.reserve(capacity_);
  pushed = 0;
  poped = 0;
}

// Push data to stream, but only as much as available capacity allows.
void Writer::push( string data )
{
  // Your code here (and in each method below)  
  if(this->is_closed() || this->has_error() || available_capacity_ <= 0){
    return;
  }
  uint64_t space = available_capacity_;
  uint64_t size = space >=data.size() ? data.size() : space;
  string data_ = data.substr(0,size);
  available_capacity_ -= size; 
  pushed += size;
  buffer_.append(data_);
  return ;

  debug( "Writer::push({}) not yet implemented", data );
}

// Signal that the stream has reached its ending. Nothing more will be written.
void Writer::close()
{
  is_closed_ = true;
  return ;
  debug( "Writer::close() not yet implemented" );
}

// Has the stream been closed?
bool Writer::is_closed() const
{
  return is_closed_;
  debug( "Writer::is_closed() not yet implemented" );
  return {}; // Your code here.
}

// How many bytes can be pushed to the stream right now?
uint64_t Writer::available_capacity() const
{

  return available_capacity_;
  debug( "Writer::available_capacity() not yet implemented" );
  return {}; // Your code here.
}

// Total number of bytes cumulatively pushed to the stream
uint64_t Writer::bytes_pushed() const
{

  return pushed;
  debug( "Writer::bytes_pushed() not yet implemented" );
  return {}; // Your code here.
}

// Peek at the next bytes in the buffer -- ideally as many as possible.
// It's not required to return a string_view of the *whole* buffer, but
// if the peeked string_view is only one byte at a time, it will probably force
// the caller to do a lot of extra work.
string_view Reader::peek() const
{
  string_view str = std::string_view(buffer_.data(),buffer_.size());
  return str;
  debug( "Reader::peek() not yet implemented" );
  return {}; // Your code here.
}

// Remove `len` bytes from the buffer.
void Reader::pop( uint64_t len )
{
  uint64_t size = buffer_.size();
  size = size >len ? len :size;
  buffer_.erase(0,size);
  poped += size;
  available_capacity_ += size;
  return;
  debug( "Reader::pop({}) not yet implemented", len );
}

// Is the stream finished (closed and fully popped)?
bool Reader::is_finished() const
{
  return (is_closed_ && buffer_.size() == 0);
  debug( "Reader::is_finished() not yet implemented" );
  return {}; // Your code here.
}

// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  return  buffer_.size();
  debug( "Reader::bytes_buffered() not yet implemented" );
  return {}; // Your code here.
}

// Total number of bytes cumulatively popped from stream
uint64_t Reader::bytes_popped() const
{

  return poped;
  debug( "Reader::bytes_popped() not yet implemented" );
  return {}; // Your code here.
}
