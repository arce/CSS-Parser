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

class CSSProperty {
public:
  std::string name;
  std::string value;
  CSSProperty(const std::string& n = "", const std::string& v = "") : name(n), value(v) {}
  std::string toString() const { return name + ": " + value + ";"; }
};

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
  void setProperty(const std::string& name, const std::string& value) { m_properties[name] = value; }
  bool hasProperty(const std::string& name) const { return m_properties.find(name) != m_properties.end(); }
  std::string getProperty(const std::string& name) const {
    auto it = m_properties.find(name);
    return it != m_properties.end() ? it->second : "";
  }
  void removeProperty(const std::string& name) { m_properties.erase(name); }
  const std::unordered_map<std::string, std::string>& getProperties() const { return m_properties; }
  
  std::string toString() const {
    std::ostringstream ss;
    ss << m_selector << " {\n";
    for (const auto& [name, value] : m_properties) ss << "  " << name << ": " << value << ";\n";
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
  
  CSSRule* getRule(const std::string& selector) {
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
      auto existing = it->second;
      for (const auto& kv : rule.getProperties()) {
        existing->setProperty(kv.first, kv.second);
      }
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
    auto end = str.find_last_not_of(" \t\n\r");
    if (start == std::string::npos || end == std::string::npos) return "";
    return str.substr(start, end - start + 1);
  }
  
  static std::vector<std::string> splitRules(const std::string& cssText) {
    std::vector<std::string> rules;
    std::string currentRule;
    int braceCount = 0;
    for (char c : cssText) {
      currentRule += c;
      if (c == '{') braceCount++;
      if (c == '}') braceCount--;
      if (braceCount == 0 && !currentRule.empty() && currentRule.find('}') != std::string::npos) {
        if (!trim(currentRule).empty()) rules.push_back(currentRule);
        currentRule.clear();
      }
    }
    if (!trim(currentRule).empty()) rules.push_back(currentRule);
    return rules;
  }
  
  static std::pair<std::string, std::string> splitSelectorAndBody(const std::string& ruleStr) {
    size_t bracePos = ruleStr.find('{');
    if (bracePos == std::string::npos) return {"", ""};
    std::string selector = trim(ruleStr.substr(0, bracePos));
    std::string body = trim(ruleStr.substr(bracePos + 1));
    if (!body.empty() && body.back() == '}') { body.pop_back(); body = trim(body); }
    return {selector, body};
  }
  
  static std::vector<std::string> splitProperties(const std::string& propertiesStr) {
    std::vector<std::string> properties;
    std::stringstream ss(propertiesStr);
    std::string property;
    while (std::getline(ss, property, ';')) {
      std::string trimmed = trim(property);
      if (!trimmed.empty()) properties.push_back(trimmed);
    }
    return properties;
  }
  
  static std::pair<std::string, std::string> parseProperty(const std::string& propStr) {
    size_t colonPos = propStr.find(':');
    if (colonPos == std::string::npos) return {"", ""};
    std::string name = trim(propStr.substr(0, colonPos));
    std::string value = trim(propStr.substr(colonPos + 1));
    return {name, value};
  }
  
public:
  static CSSDocument parse(const std::string& cssText) {
    CSSDocument doc;
    auto rules = splitRules(cssText);
    for (const auto& ruleStr : rules) {
      auto [selector, body] = splitSelectorAndBody(ruleStr);
      if (selector.empty() || body.empty()) continue;
      CSSRule rule(selector);
      auto properties = splitProperties(body);
      for (const auto& propStr : properties) {
        auto [name, value] = parseProperty(propStr);
        if (!name.empty() && !value.empty()) rule.setProperty(name, value);
      }
      doc.addRule(std::move(rule));
    }
    return doc;
  }
  
  static CSSDocument parseFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Could not open file: " + filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str());
  }
};

}

#endif
