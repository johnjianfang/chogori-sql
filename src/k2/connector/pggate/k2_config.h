//
// THE SOFTWARE IS PROVIDED "AS IS",
// WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//        AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//        DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "k2_log.h"

namespace k2pg {
namespace gate {

class Config {
public:
    Config();
    ~Config();
    nlohmann::json& operator()() {
        return _config;
    }

    template<typename T>
    T get(const std::string& key, T defaultV) {
        auto iter = _config.find(key);
        if (iter != _config.end()) {
            return iter.value();
        }
        else {
            return std::move(defaultV);
        }
    }
private:
    nlohmann::json _config;
};

} // ns gate
} // ns k2pg