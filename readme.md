# CSS Parser Library

A lightweight, header-only C++ library for parsing and manipulating CSS (Cascading Style Sheets) content.

## Features

- **Header-only** - Just include and use, no compilation required
- **No external dependencies** - Pure C++17 standard library
- **Complete CSS parsing** - Parse CSS text or files into structured objects
- **Rule management** - Add, modify, and remove CSS rules programmatically
- **Property manipulation** - Get, set, and remove individual CSS properties
- **Selector-based access** - Efficient lookup of rules by selector
- **Order preservation** - Maintains the original order of selectors
- **String output** - Generate properly formatted CSS from document objects

## Requirements

- C++17 or later
- Standard C++ library

## Installation

Simply copy `css_parser.hpp` to your project's include directory and include it:

```cpp
#include "css_parser.hpp"
```

## Usage

### Basic Parsing

```cpp
#include "css_parser.hpp"
#include <iostream>

int main() {
    std::string css = R"(
        body {
            background-color: #f0f0f0;
            margin: 0;
            padding: 20px;
        }
        
        h1 {
            color: #333;
            font-size: 24px;
        }
        
        .button {
            background: blue;
            color: white;
        }
    )";
    
    CSS::CSSDocument doc = CSS::CSSParser::parse(css);
    
    // Print all rules
    std::cout << doc.toString() << std::endl;
    
    return 0;
}
```

### Parse from File

```cpp
try {
    CSS::CSSDocument doc = CSS::CSSParser::parseFile("styles.css");
    std::cout << "Loaded " << doc.getRuleCount() << " rules" << std::endl;
} catch (const std::invalid_argument& e) {
    std::cerr << "Invalid path: " << e.what() << std::endl; // e.g. path traversal attempt
} catch (const std::runtime_error& e) {
    std::cerr << "Error: " << e.what() << std::endl; // file not found / unreadable
}
```

### Manipulating Rules

```cpp
// Check if a selector exists
if (doc.hasSelector("h1")) {
    // Get the rule
    CSS::CSSRule* rule = doc.getRule("h1");
    
    // Get a property value (returns std::optional<std::string>)
    auto color = rule->getProperty("color");
    if (color) std::cout << "h1 color: " << *color << std::endl;

    // Or use getPropertyOr() for a fallback default
    std::string fontSize = rule->getPropertyOr("font-size", "16px");
    
    // Modify an existing property
    rule->setProperty("font-size", "32px");
    
    // Add a new property
    rule->setProperty("font-weight", "bold");
    
    // Remove a property
    rule->removeProperty("margin");
}
```

### Creating New Rules

```cpp
// Create a new rule
CSS::CSSRule newRule(".header");
newRule.setProperty("background", "#2c3e50");
newRule.setProperty("color", "white");
newRule.setProperty("padding", "15px");

// Add it to the document
doc.addRule(newRule);
```

### Iterating Through Rules

```cpp
std::vector<std::string> selectors = doc.getAllSelectors();
for (const auto& selector : selectors) {
    CSS::CSSRule* rule = doc.getRule(selector);
    if (rule) {
        std::cout << "Selector: " << rule->getSelector() << std::endl;
        
        for (const auto& [name, value] : rule->getProperties()) {
            std::cout << "  " << name << ": " << value << std::endl;
        }
    }
}
```

### Removing Rules

```cpp
// Remove a selector and its associated rule
doc.removeSelector(".unused-class");
```

### Clearing All Rules

```cpp
doc.clear();
```

## API Reference

### CSSRule (class)
| Method                          | Description                                                   |
| ------------------------------- | ------------------------------------------------------------- |
| `getSelector()`                 | Returns the selector string                                   |
| `setSelector(selector)`         | Sets the selector                                             |
| `setProperty(name, value)`      | Sets or updates a property                                    |
| `hasProperty(name)`             | Checks if a property exists                                   |
| `getProperty(name)`             | Returns `std::optional<std::string>` — `nullopt` if not found |
| `getPropertyOr(name, default)`  | Returns property value or `default` if not found             |
| `removeProperty(name)`          | Removes a property                                            |
| `getProperties()`               | Returns map of all properties                                 |
| `toString()`                    | Returns formatted CSS rule                                    |

### CSSDocument (class)
| Method                     | Description                                 |
| -------------------------- | ------------------------------------------- |
| `hasSelector(selector)`    | Checks if selector exists                   |
| `getRule(selector)`        | Returns pointer to rule or nullptr          |
| `addRule(rule)`            | Adds or merges a rule                       |
| `removeSelector(selector)` | Removes selector and its rule               |
| `clear()`                  | Removes all rules                           |
| `getRuleCount()`           | Returns number of rules                     |
| `getAllSelectors()`        | Returns list of selectors (order preserved) |
| `toString()`               | Returns formatted CSS document              |

### CSSParser (static class)
| Method                | Description                                                                  |
| --------------------- | ---------------------------------------------------------------------------- |
| `parse(cssText)`      | Parses CSS string into a document; strips comments automatically             |
| `parseFile(filename)` | Parses CSS file into a document; throws `std::invalid_argument` on bad paths |

## Limitations

- Does not validate CSS syntax or property values
- Multiple selectors separated by commas are treated as a single combined selector
- Nested at-rules (`@media`, `@keyframes`, `@supports`, etc.) are detected and skipped — their inner rules are not parsed
- Nested CSS rules (SCSS/SASS) are not supported

## Example Output

Given the input CSS, `doc.toString()` produces:

```css
body {
  background-color: #f0f0f0;
  margin: 0;
  padding: 20px;
}

h1 {
  color: #333;
  font-size: 24px;
}
```

## License

Feel free to use this library in any project, open source or proprietary.

## Contributing

Contributions are welcome! Please ensure code follows the existing style and includes appropriate error handling.

## Author

CSS Parser Library - A lightweight C++ CSS parsing solution