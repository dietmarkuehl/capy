//
// Copyright (c) 2026 Steve Gerbino
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/capy
//

#ifndef BOOST_CAPY_BENCH_SNDR_ANY_READ_STREAM_HPP
#define BOOST_CAPY_BENCH_SNDR_ANY_READ_STREAM_HPP

#include "sndr_any_read_some_sender.hpp"

#include <boost/capy/buffers.hpp>

#include <memory>
#include <utility>

/// Standalone value-type erased sender stream.
///
/// Mirrors capy::any_read_stream: stores any sender stream behind
/// a vtable, heap-allocated. Does NOT inherit from
/// sndr_io_read_stream — this is a fully independent erasure
/// mechanism.
class sndr_any_read_stream
{
    struct stream_holder_base {
        virtual ~stream_holder_base() = default;
        virtual sndr_any_read_some_sender read_some(boost::capy::mutable_buffer) = 0;
    };

    std::unique_ptr<stream_holder_base> stream_;

    template <typename Stream>
    struct stream_holder: stream_holder_base {
        using sender_t = decltype(std::declval<Stream>().read_some(std::declval<boost::capy::mutable_buffer>()));

        Stream                            stream;
        sndr_any_read_some_sender::rep_t<sender_t> rep;
        stream_holder(Stream s)
            : stream(std::move(s))
        {
        }
        sndr_any_read_some_sender read_some(boost::capy::mutable_buffer buf) override {
            return sndr_any_read_some_sender(rep, stream.read_some(buf));
        }
    };

public:
    template <class Stream>
    explicit sndr_any_read_stream(Stream s)
        : stream_(new stream_holder<Stream>(std::move(s)))
    {
    }
    sndr_any_read_some_sender read_some(boost::capy::mutable_buffer buf)
    {
        return stream_->read_some(buf);
    }
};

#endif
