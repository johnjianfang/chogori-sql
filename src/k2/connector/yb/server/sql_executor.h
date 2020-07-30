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

#ifndef CHOGORI_SQL_SQL_EXECUTOR_H
#define CHOGORI_SQL_SQL_EXECUTOR_H

#include "yb/common/status.h"

using namespace yb;

namespace k2 {
namespace sql {
    class SqlExecutor {

    public:
        SqlExecutor() = default;
        ~SqlExecutor();

        CHECKED_STATUS Init();

        // Waits for the tablet server to complete the initialization.
        CHECKED_STATUS WaitInited();

        CHECKED_STATUS Start();

        virtual void Shutdown();

        virtual Env* GetEnv();

        const std::string& LogPrefix() const {
            return log_prefix_;
        }

        void set_cluster_uuid(const std::string& cluster_uuid);

        std::string cluster_uuid() const;

    protected:
        virtual CHECKED_STATUS RegisterServices();

        std::atomic<bool> initted_{false};

        mutable simple_spinlock lock_;

        std::string cluster_uuid_;

    private:
        // Auto initialize some of the service flags that are defaulted to -1.
        void AutoInitServiceFlags();

        std::string log_prefix_;

        std::atomic<uint64_t> catalog_version_{0};
    };
}
}
#endif //CHOGORI_SQL_SQL_EXECUTOR_H
