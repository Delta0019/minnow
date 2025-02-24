#include "socket.hh"

using namespace std;

class RawSocket : public DatagramSocket
{
public:
  RawSocket() : DatagramSocket( AF_INET, SOCK_RAW, IPPROTO_RAW ) {}
};

int main()
{
  // construct an Internet or user datagram here, and send using the RawSocket as in the Jan. 10 lecture

  RawSocket sock;
  Address addr( "127.0.0.1", 80 );
  sock.sendto( addr, "Hello, world!" );
  sock.close();

  return 0;
}
