#pragma once
#include <functional>
#include "intrusive_list.h"

namespace signals {

template <typename T>
struct signal;

template <typename... Args>
struct signal<void (Args...)> {
    using slot_t = std::function<void (Args...)>;

    struct connection : intrusive::list_element<struct connection_tag> {
        connection() = default;

        connection(signal* sig, slot_t slot)
            : sig(sig)
            , slot(std::move(slot)) {
            sig->connections.push_front(*this);
        }

        connection(connection&& other) noexcept
            : sig(other.sig)
            , slot(std::move(other.slot)) {
            replace(other);
        }

        connection& operator=(connection&& other) noexcept {
            if (this != &other) {
                disconnect();
                sig = other.sig;
                slot = std::move(other.slot);
                replace(other);
            }
            return *this;
        }

        ~connection() {
            disconnect();
        }

        void replace(connection& replacable) {
            if (replacable.is_linked()) {
                sig->connections.insert(sig->connections.as_iterator(replacable), *this);
                replacable.unlink();
                for (auto token = sig->current; token; token = token->prev) {
                    if (token->it != sig->connections.end() && &*token->it == &replacable) {
                        token->it = sig->connections.as_iterator(*this);
                    }
                }
            }
        }

        void disconnect() {
            if (is_linked()) {
                unlink();
                slot = {};
                for (auto token = sig->current; token; token = sig->current->prev) {
                    if (token->it != sig->connections.end() && &*token->it == this) {
                        ++token->it;
                    }
                }
                sig = nullptr;
            }
        }

        signal* sig;
        slot_t slot;
    };
    using connections_t = intrusive::list<connection, struct connection_tag>;

    signal() = default;

    signal(signal const&) = delete;
    signal& operator=(signal const&) = delete;

    ~signal() {
        for (iteration_token* token = current; token; token = token->prev) {
            token->deleted = true;
        }
    }

    connection connect(slot_t slot) noexcept {
        return connection(this, slot);
    }

    struct iteration_token {
        explicit iteration_token(signal const* sig)
            : prev(sig->current)
            , it(sig->connections.begin())
            , deleted(false) {
            sig->current = this;
        }

        iteration_token* prev;
        typename connections_t::const_iterator it;
        bool deleted;
    };

    void operator()(Args... args) const {
        iteration_token token(this);
        try {
            while (current->it != connections.end()) {
                auto copy = current->it;
                ++current->it;
                copy->slot(args...);
                if (token.deleted) {
                    return;
                }
            }
        } catch (...) {
            current = token.prev;
            throw;
        }
        current = token.prev;
    }

 private:
    mutable iteration_token* current = nullptr;
    connections_t connections;
};

}
