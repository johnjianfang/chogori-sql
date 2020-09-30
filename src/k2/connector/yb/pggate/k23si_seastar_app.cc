/*
MIT License

Copyright(c) 2020 Futurewei Cloud

    Permission is hereby granted,
    free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :

    The above copyright notice and this permission notice shall be included in all copies
    or
    substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS",
    WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
    DAMAGES OR OTHER
    LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "k23si_seastar_app.h"
#include "k23si_queue_defs.h"
#include "k23si_txn.h"

namespace k2pg {
namespace gate {

PGK2Client::PGK2Client():
    _client(k2::K23SIClientConfig()) {
}

seastar::future<> PGK2Client::gracefulStop() {
    return std::move(_poller);
}

seastar::future<> PGK2Client::start() {
    // start polling the request queues only on core 0
    if (seastar::engine().cpu_id() == 0) {
        _poller = _poller.then([this] {
            return seastar::do_until(
                [] {
                    return false; // TODO break out of poller pool if asked to exit
                },
                [this] {
                    return _pollForWork();
                }
            );
        });
    }
    return seastar::make_ready_future();
}

// Helper function used to poll a given queue. The given Func visitor is called with each element
// pulled off the queue in sequence
template <typename Q, typename Func>
seastar::future<> pollQ(Q& q, Func&& visitor) {
    // lock the mutex before manipulating the queue
    std::unique_lock lock(requestQMutex);

    std::vector<seastar::future<>> futs;
    futs.reserve(q.size());

    while (!q.empty()) {
        futs.push_back(
            seastar::do_with(std::move(q.front()), std::forward<Func>(visitor), [](auto& req, auto& visitor) {
                return visitor(req)
                    .handle_exception([&req](auto exc) {
                        req.prom.set_exception(exc);
                    });
            }));
        q.pop();
    }
    return seastar::when_all_succeed(futs.begin(), futs.end());
}

seastar::future<> PGK2Client::_pollBeginQ() {
    return pollQ(beginTxQ, [this](auto& req) {
        return _client.beginTxn(req.opts)
            .then([this, &req](auto&& txn) {
                req.prom.set_value(K23SITxn(txn.mtr()));  // send a copy to the promise

                _txns[txn.mtr()] = std::move(txn);
            });
    });
}

seastar::future<> PGK2Client::_pollEndQ() {
    return pollQ(endTxQ, [this](auto& req) {
        auto fiter = _txns.find(req.mtr);
        if (fiter == _txns.end()) {
            req.prom.set_value(k2::EndResult(k2::dto::K23SIStatus::OperationNotAllowed("invalid txn id")));
            return seastar::make_ready_future();
        }

        return fiter->second.end(req.shouldCommit)
            .then([this, &req](auto&& endResult) {
                _txns.erase(req.mtr);
                req.prom.set_value(std::move(endResult));
            });
    });
}

seastar::future<> PGK2Client::_pollSchemaGetQ() {
    return pollQ(schemaGetTxQ, [this](auto& req) {
        return _client.getSchema(req.collectionName, req.schemaName, req.schemaVersion)
            .then([this, &req](auto&& result) {
                req.prom.set_value(std::move(result));
            });
    });
}

seastar::future<> PGK2Client::_pollSchemaCreateQ() {
    return pollQ(schemaCreateTxQ, [this](auto& req) {
        return _client.createSchema(req.collectionName, req.schema)
            .then([this, &req](auto&& result) {
                req.prom.set_value(std::move(result));
            });
    });
}

seastar::future<> PGK2Client::_pollReadQ() {
    return pollQ(readTxQ, [this](auto& req) mutable {
        auto fiter = _txns.find(req.mtr);
        if (fiter == _txns.end()) {
            req.prom.set_value(k2::ReadResult<k2::dto::SKVRecord>(k2::dto::K23SIStatus::OperationNotAllowed("invalid txn id"), k2::dto::SKVRecord()));
            return seastar::make_ready_future();
        }
        return fiter->second.read(std::move(req.record))
            .then([this, &req](auto&& readResult) {
                req.prom.set_value(std::move(readResult));
            });
    });
}

seastar::future<> PGK2Client::_pollScanReadQ() {
    //TODO add scan support
    return seastar::make_ready_future();
}

seastar::future<> PGK2Client::_pollWriteQ() {
    return pollQ(writeTxQ, [this](auto& req) mutable {
        auto fiter = _txns.find(req.mtr);
        if (fiter == _txns.end()) {
            req.prom.set_value(k2::WriteResult(k2::dto::K23SIStatus::OperationNotAllowed("invalid txn id"), k2::dto::K23SIWriteResponse{}));
            return seastar::make_ready_future();
        }
        return fiter->second.write(req.record, req.erase)
            .then([this, &req](auto&& writeResult) {
                req.prom.set_value(std::move(writeResult));
            });
    });
}

seastar::future<> PGK2Client::_pollForWork() {
    return seastar::when_all_succeed(
        _pollBeginQ(), _pollEndQ(), _pollSchemaGetQ(), _pollSchemaCreateQ(), _pollScanReadQ(), _pollReadQ(), _pollWriteQ());
}

}  // namespace gate
}  // namespace k2pg