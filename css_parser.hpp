#ifndef CSS_PARSER_HPP
#define CSS_PARSER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <optional>

namespace CSS {

// CSSProperty removed: it was defined but never used internally.
// CSSRule uses unordered_map<string,string> directly for properties.

class CSSRule {
private:
  std::string m_selector;
  std::unordered_map<std::string, std::string> m_properties;

public:
  CSSRule(const std::string& sel = "") : m_selector(sel) {}
  CSSRule(const std::string& sel, const std::unordered_map<std::string, std::string>& props)
    : m_selector(sel), m_properties(props) {}

  const std::string& getSelector() const { return m_selector; }
  void setSelector(const std::string& sel) { m_selector = sel; }

  void setProperty(const std::string& name, const std::string& value) {
    m_properties[name] = value;
  }

  bool hasProperty(const std::string& name) const {
    return m_properties.find(name) != m_properties.end();
  }

  // Returns nullopt when the property doesn't exist, distinguishing it from
  // a property that is present but has an empty value.
  std::optional<std::string> getProperty(const std::string& name) const {
    auto it = m_properties.find(name);
    if (it == m_properties.end()) return std::nullopt;
    return it->second;
  }

  // Convenience wrapper: returns the property value or a fallback default.
  std::string getPropertyOr(const std::string& name,
                            const std::string& defaultValue = "") const {
    auto it = m_properties.find(name);
    return it != m_properties.end() ? it->second : defaultValue;
  }

  void removeProperty(const std::string& name) { m_properties.erase(name); }

  const std::unordered_map<std::string, std::string>& getProperties() const {
    return m_properties;
  }

  std::string toString() const {
    std::ostringstream ss;
    ss << m_selector << " {\n";
    for (const auto& [name, value] : m_properties)
      ss << "  " << name << ": " << value << ";\n";
    ss << "}";
    return ss.str();
  }
};

class CSSDocument {
private:
  std::unordered_map<std::string, std::shared_ptr<CSSRule>> m_rules;
  std::vector<std::string> m_selectorOrder;

public:
  bool hasSelector(const std::string& selector) const {
    return m_rules.find(selector) != m_rules.end();
  }

  // Non-const: returns a mutable pointer, or nullptr if not found.
  CSSRule* getRule(const std::string& selector) {
    auto it = m_rules.find(selector);
    return it != m_rules.end() ? it->second.get() : nullptr;
  }

  // Const overload: allows getRule() on const CSSDocument references.
  const CSSRule* getRule(const std::string& selector) const {
    auto it = m_rules.find(selector);
    return it != m_rules.end() ? it->second.get() : nullptr;
  }

  void addRule(const CSSRule& rule) {
    const std::string& selector = rule.getSelector();
    if (selector.empty()) return;
    auto it = m_rules.find(selector);
    if (it == m_rules.end()) {
      m_rules[selector] = std::make_shared<CSSRule>(rule);
      m_selectorOrder.push_back(selector);
    } else {
      // Merge: later properties overwrite earlier ones (standard CSS cascade).
      auto existing = it->second;
      for (const auto& kv : rule.getProperties())
        existing->setProperty(kv.first, kv.second);
    }
  }

  void addRule(std::shared_ptr<CSSRule> rule) {
    if (rule) addRule(*rule);
  }

  void removeSelector(const std::string& selector) {
    m_rules.erase(selector);
    auto it = std::find(m_selectorOrder.begin(), m_selectorOrder.end(), selector);
    if (it != m_selectorOrder.end()) m_selectorOrder.erase(it);
  }

  void clear() { m_rules.clear(); m_selectorOrder.clear(); }

  size_t getRuleCount() const { return m_rules.size(); }

  std::vector<std::string> getAllSelectors() const { return m_selectorOrder; }

  std::string toString() const {
    std::ostringstream ss;
    for (size_t i = 0; i < m_selectorOrder.size(); ++i) {
      const auto& selector = m_selectorOrder[i];
      auto it = m_rules.find(selector);
      if (it != m_rules.end()) {
        ss << it->second->toString();
        if (i < m_selectorOrder.size() - 1) ss << "\n\n";
      }
    }
    return ss.str();
  }
};

class CSSParser {
private:
  static std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\n\r");
    auto end   = str.find_last_not_of(" \t\n\r");
    if (start == std::string::npos || end == std::string::npos) return "";
    return str.substr(start, end - start + 1);
  }

  // Strip /* ... */ block comments from CSS source.
  // Newlines inside comments are preserved so that line-based diagnostics
  // (if added later) remain accurate.
  static std::string removeComments(const std::string& css) {
    std::string result;
    result.reserve(css.size());
    size_t i = 0;
    while (i < css.size()) {
      if (i + 1 < css.size() && css[i] == '/' && css[i + 1] == '*') {
        i += 2; // skip opening /*
        while (i + 1 < css.size() && !(css[i] == '*' && css[i + 1] == '/')) {
          if (css[i] == '\n') result += '\n'; // preserve line structure
          ++i;
        }
        if (i + 1 < css.size()) i += 2; // skip closing */
      } else {
        result += css[i++];
      }
    }
    return result;
  }

  // Split the (comment-stripped) CSS text into individual rule strings.
  // Handles nested braces correctly (e.g., @media blocks).
  static std::vector<std::string> splitRules(const std::string& cssText) {
    std::vector<std::string> rules;
    std::string currentRule;
    currentRule.reserve(256); // avoid frequent small reallocations
    int braceCount = 0;

    for (char c : cssText) {
      currentRule += c;
      if (c == '{') {
        ++braceCount;
      } else if (c == '}') {
        --braceCount;
        if (braceCount == 0) {
          // Complete rule found — push and reset.
          std::string trimmed = trim(currentRule);
          if (!trimmed.empty()) rules.push_back(std::move(currentRule));
          currentRule.clear();
          currentRule.reserve(256);
        }
      }
    }
    // Any trailing content without a closing brace is malformed CSS; skip it.
    return rules;
  }

  static std::pair<std::string, std::string>
  splitSelectorAndBody(const std::string& ruleStr) {
    size_t bracePos = ruleStr.find('{');
    if (bracePos == std::string::npos) return {"", ""};
    std::string selector = trim(ruleStr.substr(0, bracePos));
    std::string body     = trim(ruleStr.substr(bracePos + 1));
    if (!body.empty() && body.back() == '}') {
      body.pop_back();
      body = trim(body);
    }
    return {selector, body};
  }

  // Split a CSS declaration block by ';', correctly handling:
  //   - Parenthesised values:  url(data:image/png;base64,...)  rgb(0,0,0)
  //   - Quoted strings:        content: "a;b"   content: 'x;y'
  static std::vector<std::string> splitProperties(const std::string& propertiesStr) {
    std::vector<std::string> properties;
    std::string current;
    int  parenDepth = 0;
    char inQuote    = 0; // '\0' = not in a string literal

    for (size_t i = 0; i < propertiesStr.size(); ++i) {
      char c = propertiesStr[i];

      if (inQuote) {
        current += c;
        // End of quoted string (ignore escaped quotes).
        if (c == inQuote && (i == 0 || propertiesStr[i - 1] != '\\'))
          inQuote = 0;
      } else if (c == '"' || c == '\'') {
        inQuote = c;
        current += c;
      } else if (c == '(') {
        ++parenDepth;
        current += c;
      } else if (c == ')') {
        if (parenDepth > 0) --parenDepth;
        current += c;
      } else if (c == ';' && parenDepth == 0) {
        // Real declaration boundary — not inside parens or a string.
        std::string trimmed = trim(current);
        if (!trimmed.empty()) properties.push_back(trimmed);
        current.clear();
      } else {
        current += c;
      }
    }
    // Handle the last declaration when there is no trailing semicolon.
    std::string trimmed = trim(current);
    if (!trimmed.empty()) properties.push_back(trimmed);

    return properties;
  }

  static std::pair<std::string, std::string>
  parseProperty(const std::string& propStr) {
    size_t colonPos = propStr.find(':');
    if (colonPos == std::string::npos) return {"", ""};
    std::string name  = trim(propStr.substr(0, colonPos));
    std::string value = trim(propStr.substr(colonPos + 1));
    return {name, value};
  }

  // Returns true for at-rules whose body contains nested rule-sets rather than
  // plain declarations (@media, @keyframes, @supports, @layer, …).
  // Non-nesting at-rules (@import, @charset, @namespace) return false because
  // they have no block body at all and are simply skipped via an empty body.
  static bool isNestedAtRule(const std::string& selector) {
    if (selector.empty() || selector[0] != '@') return false;
    // These at-rules never have a { … } block of their own.
    static const std::vector<std::string> leafAtRules = {
      "@import", "@charset", "@namespace"
    };
    for (const auto& leaf : leafAtRules) {
      if (selector.size() >= leaf.size() &&
          selector.substr(0, leaf.size()) == leaf)
        return false;
    }
    return true; // @media, @keyframes, @supports, @layer, @font-face, etc.
  }

  // Guard against path traversal attacks when loading CSS from user-supplied paths.
  static void validateFilePath(const std::string& filename) {
    if (filename.empty())
      throw std::invalid_argument("File path cannot be empty");
    if (filename.find("..") != std::string::npos)
      throw std::invalid_argument(
        "Invalid file path: directory traversal sequences ('..') are not allowed");
  }

public:
  static CSSDocument parse(const std::string& cssText) {
    CSSDocument doc;
    std::string cleaned = removeComments(cssText);
    auto rules = splitRules(cleaned);

    for (const auto& ruleStr : rules) {
      auto [selector, body] = splitSelectorAndBody(ruleStr);
      if (selector.empty()) continue;

      // Skip nested at-rules (@media, @keyframes, …) — their bodies contain
      // rule-sets, not declarations, so they cannot be parsed as flat properties.
      if (isNestedAtRule(selector)) continue;

      if (body.empty()) continue;

      CSSRule rule(selector);
      for (const auto& propStr : splitProperties(body)) {
        auto [name, value] = parseProperty(propStr);
        if (!name.empty() && !value.empty())
          rule.setProperty(name, value);
      }
      doc.addRule(std::move(rule));
    }
    return doc;
  }

  static CSSDocument parseFile(const std::string& filename) {
    validateFilePath(filename);
    std::ifstream file(filename);
    if (!file.is_open())
      throw std::runtime_error("Could not open file: " + filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str());
  }
};

} // namespace CSS

#endif // CSS_PARSER_HPP
