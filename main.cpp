#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <inttypes.h>

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

    template<class DataSource>
    void send(DataSource& source) {
        socket_.send(source.get_buffers());
    }
};

//! Generate time-series from random walk
struct RandomWalk {
    std::random_device                  randdev_;
    std::mt19937                        generator_;
    std::normal_distribution<double>    distribution_;

    std::vector<double>                 values;

    RandomWalk(size_t N, double start, double mean, double stddev)
        : generator_(randdev_())
        , distribution_(mean, stddev)
        , values(N, start)
    {
    }

    double next(size_t ix) {
        values.at(ix) += distribution_(generator_);
        return values.at(ix);
    }
};


struct TimestampGenerator {
    std::uint64_t current_;
    std::uint64_t step_;

    TimestampGenerator() = default;
    TimestampGenerator(TimestampGenerator const&) = default;
    TimestampGenerator& operator = (TimestampGenerator const&) = default;

    TimestampGenerator(uint64_t origin, uint64_t step)
        : current_(origin)
        , step_(step)
    {
    }

    uint64_t next() {
        current_ += step_;
        return current_;
    }
};


//! Protocol data unit that contains many values
struct PDU {
    std::string const&         series;
    uint64_t                timestamp;
    std::vector<double> const& values;
};

struct RowGenerator {
    std::string series;  //< This should be a compound series name - "cpu|mem|iops tag=val"
    TimestampGenerator tsgen;
    RandomWalk xsgen;

    RowGenerator(std::string sname, TimestampGenerator const& ts, size_t N, double start, double mean, double stddev)
        : series(sname)
        , tsgen(ts)
        , xsgen(N, start, mean, stddev)
    {
    }

    void step() {
        for (auto ix = 0u; ix < xsgen.values.size(); ix++) {
            xsgen.next(ix);
        }
    }

    PDU next() {
        step();
        return {
            series,
            tsgen.next(),
            xsgen.values,
        };
    }
};

struct ProtocolStream {
    enum {
        BUFFER_SIZE = 24*1024,
    };
    RowGenerator gen;
    std::vector<char> buffer;
    int pos;

    ProtocolStream(std::string sname, TimestampGenerator const& ts, size_t N, double start, double mean, double stddev)
        : gen(sname, ts, N, start, mean, stddev)
        , pos(0)
    {
        buffer.resize(BUFFER_SIZE);
    }

    bool add_one(std::vector<boost::asio::const_buffer>* list) {
        PDU pdu = gen.next();
        // Static part
        size_t original_size = list->size();
        list->push_back(boost::asio::const_buffer("+", 1));
        list->push_back(boost::asio::const_buffer(pdu.series.data(), pdu.series.size()));
        list->push_back(boost::asio::const_buffer("\r\n", 2));
        // Dynamic part
        int sz = static_cast<int>(buffer.size()) - pos;
        char* ptr = buffer.data() + pos;
        const char* pbegin = ptr;

        // Timestamp
        int res = snprintf(ptr, static_cast<size_t>(sz), "+%" PRIu64 "\r\n", pdu.timestamp);
        if (res < 0 || res > sz) {
            list->resize(original_size);
            return false;
        }
        sz -= res;
        ptr += res;
        pos += res;

        // Num values
        res = snprintf(ptr, static_cast<size_t>(sz), "*%d\r\n", static_cast<int>(pdu.values.size()));
        if (res < 0 || res > sz) {
            list->resize(original_size);
            return false;
        }
        sz -= res;
        ptr += res;
        pos += res;

        // Values
        for (auto val: pdu.values) {
            res = snprintf(ptr, static_cast<size_t>(sz), "+%.17g\r\n", val);
            if (res < 0 || res > sz) {
                list->resize(original_size);
                return false;
            }
            sz -= res;
            ptr += res;
            pos += res;
        }

        size_t size = static_cast<size_t>(ptr - pbegin);
        list->push_back(boost::asio::const_buffer(pbegin, size));
        return true;
    }

    void reset() {
        pos = 0;
    }
};

int main(int argc, char *argv[])
{
    struct S {
        ProtocolStream stream;

        S()
            : stream("foo|bar tag=value", TimestampGenerator(10000000000, 100000), 2, 10.0, 0.0, 0.001)
        {
        }

        std::vector<boost::asio::const_buffer> get_buffers() {
            std::vector<boost::asio::const_buffer> buffers;
            stream.reset();
            while(stream.add_one(&buffers));
            return buffers;
        }
    };

    boost::asio::io_service io;
    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("127.0.0.1"), 8282);
    Connection connection(io, ep);
    connection.connect();
    S s;
    for (int i = 0; i < 1000000; i++) {
        connection.send(s);
    }
    return 0;
}
