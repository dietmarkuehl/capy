// bench/beman/sndr_any_read_some_sender.hpp                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BOOST_CAPY_BENCH_SNDR_ANY_READ_SOME_SENDER
#define BOOST_CAPY_BENCH_SNDR_ANY_READ_SOME_SENDER

#include <beman/execution/execution.hpp>
#include <memory>
#include <utility>

namespace ex = beman::execution;

// ----------------------------------------------------------------------------

    struct sndr_any_read_some_sender {
        using sender_concept = ex::sender_t;
        using completion_signatures = 
            ex::completion_signatures<ex::set_value_t(std::size_t)>;
        template <typename...>
        static consteval auto get_completion_signatures() {
            return completion_signatures();
        }

        struct state_base {
            virtual ~state_base() = default;
            virtual void set_value(std::size_t) = 0;
        };

        struct inner_state_base {
            virtual ~inner_state_base() = default;
            virtual void start() = 0;
            state_base* object = nullptr;
            void complete(std::size_t size) {
                object->set_value(size);
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
        struct state: state_base {
            using operation_state_concept = ex::operation_state_t;
            using receiver_t = std::remove_cvref_t<Rcvr>;
            receiver_t rcvr;
            inner_state_base* inner_state;
            
            state(Rcvr&& rcvr, inner_state_base* is)
                : rcvr(std::forward<Rcvr>(rcvr))
                , inner_state(is)
            {
            }
            void start() & noexcept {
                inner_state->object = this;
                inner_state->start();
            }
            void set_value(std::size_t n) override {
                ex::set_value(std::move(rcvr), n);
            }
        };

        inner_state_base* inner_state;

        template <typename Sndr>
        struct rep_t {
            std::optional<sndr_any_read_some_sender::inner_state_t<Sndr>> inner;
            sndr_any_read_some_sender::inner_state_t<Sndr>& emplace(Sndr sndr) {
                return inner.emplace(std::move(sndr));
            }
        };

        template <ex::sender Sndr>
        explicit sndr_any_read_some_sender(rep_t<Sndr>& rep, Sndr sndr)
            : inner_state(&rep.emplace(std::move(sndr)))
        {
            static_assert(ex::sender<sndr_any_read_some_sender>);
            static_assert(ex::sender_in<sndr_any_read_some_sender>);
        }
        template <ex::receiver Rcvr>
        auto connect(Rcvr&& rcvr) && {
            static_assert(ex::operation_state<state<Rcvr>>);
            return state<Rcvr>(std::forward<Rcvr>(rcvr), inner_state);
        }
    };

// ----------------------------------------------------------------------------

#endif
