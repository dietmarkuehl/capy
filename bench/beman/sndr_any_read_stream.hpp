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

#include "sndr_any_read_sender.hpp"

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
    struct read_some_sender {
        using sender_concept = ex::sender_t;
        using completion_signatures = 
            ex::completion_signatures<ex::set_value_t(std::size_t)>;
        template <typename...>
        static consteval auto get_completion_signatures() {
            return completion_signatures();
        }

        struct inner_state_base {
            virtual ~inner_state_base() = default;
            virtual void start() = 0;
            void (*set_value)(void*, std::size_t) = nullptr;
            void* object = nullptr;
            void complete(std::size_t size) {
                set_value(object, size);
            }
        };

        struct inner_receiver {
            using receiver_concept = ex::receiver_t;
            inner_state_base* state;
            void set_value(std::size_t size) && noexcept {
                state->complete(size);
            }
        };

        template <ex::sender Sndr>
        struct inner_state_t: inner_state_base {
            using state_t = ex::connect_result_t<Sndr, inner_receiver>;
            state_t state;
            inner_state_t(Sndr&& sndr)
                : state(ex::connect(std::forward<Sndr>(sndr), inner_receiver(this)))
            {
            }
            void start() override {
                ex::start(this->state);
            }
        };

        template <ex::receiver Rcvr>
        struct state {
            using operation_state_concept = ex::operation_state_t;
            using receiver_t = std::remove_cvref_t<Rcvr>;
            receiver_t rcvr;
            inner_state_base* inner_state;
            
            static void set_value(void* obj, std::size_t size) {
                ex::set_value(std::move(*static_cast<receiver_t*>(obj)), size);
            }
            void start() & noexcept {
                inner_state->set_value = &set_value;
                inner_state->object = &rcvr;
                inner_state->start();
            }
        };

        inner_state_base* inner_state;

        explicit read_some_sender(inner_state_base* st)
            : inner_state(st)
        {
            static_assert(ex::sender<read_some_sender>);
        }
        template <ex::receiver Rcvr>
        auto connect(Rcvr&& rcvr) && {
            static_assert(ex::operation_state<state<Rcvr>>);
            return state<Rcvr>(std::forward<Rcvr>(rcvr), inner_state);
        }
        template <typename Sndr>
        using rep_t = std::optional<read_some_sender::inner_state_t<Sndr>>;
    };
    struct stream_holder_base {
        virtual ~stream_holder_base() = default;
        virtual read_some_sender read_some(boost::capy::mutable_buffer) = 0;
    };

    std::unique_ptr<stream_holder_base> stream_;

    template <typename Stream>
    struct stream_holder: stream_holder_base {
        using sender_t = decltype(std::declval<Stream>().read_some(std::declval<boost::capy::mutable_buffer>()));

        Stream                            stream;
        read_some_sender::rep_t<sender_t> rep;
        stream_holder(Stream s)
            : stream(std::move(s))
        {
        }
        read_some_sender read_some(boost::capy::mutable_buffer buf) override {
            rep.emplace(stream.read_some(buf));
            return read_some_sender(&*rep);
        }
    };

public:
    template <class Stream>
    explicit sndr_any_read_stream(Stream s)
        : stream_(new stream_holder<Stream>(std::move(s)))
    {
    }
    read_some_sender read_some(boost::capy::mutable_buffer buf)
    {
        return stream_->read_some(buf);
    }
};

#endif
