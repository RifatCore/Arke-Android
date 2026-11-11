#pragma once

#include <string>
#include <map>
#include <set>
#include <functional>
#include <vector>
#include "arke/types.hpp"

namespace arke::shader {

using IncludeCallback = std::function<std::string(const std::string& includePath)>;

class Preprocessor {
public:
    Preprocessor();

    void setMacro(const std::string& name, const std::string& value = "1");
    void removeMacro(const std::string& name);
    void setIncludeCallback(IncludeCallback cb) { m_includeCallback = cb; }

    std::string preprocess(const std::string& source, const std::string& filename = "shader.glsl");

private:
    std::string processInternal(const std::string& source, const std::string& filename, std::set<std::string>& includeStack);
    std::string evaluateCondition(const std::string& expr);

    std::map<std::string, std::string> m_macros;
    IncludeCallback m_includeCallback;
};

} // namespace arke::shader
