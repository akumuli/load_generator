#include <iostream>
#include <vector>
#include <string>
#include <random>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

using namespace std;

struct Connection {
    boost::asio::io_service& io_;
    boost::asio::ip::tcp::endpoint ep_;
    boost::asio::ip::tcp::socket socket_;

    Connection(boost::asio::io_service& io, boost::asio::ip::tcp::endpoint ep)
        : io_(io)
        , ep_(ep)
        , socket_(io_)
    {
    }

    //! Connect synchronously
    void connect() {
        socket_.connect(ep_);
    }

    void send() {
    }
};

//! Generate time-series from random walk
struct RandomWalk {
    std::random_device                  randdev_;
    std::mt19937                        generator_;
    std::normal_distribution<double>    distribution_;
    double                              value_;

    RandomWalk(double start, double mean, double stddev)
        : generator_(randdev_())
        , distribution_(mean, stddev)
        , value_(start)
    {
    }

    double generate() {
        value_ += distribution_(generator_);
        return value_;
    }
};

struct Generator {
    std::string column;
    RandomWalk  rwalk;
};

struct Row {
    boost::posix_time::ptime timestamp;

};

int main(int argc, char *argv[])
{
    cout << "Hello World!" << endl;
    return 0;
}
