# Contributing to FLOW

Thank you for your interest in contributing to FLOW! This document provides guidelines for contributing to the project.

## How to Contribute

### Reporting Bugs

- Use the GitHub Issues tracker
- Describe the bug clearly with steps to reproduce
- Include system information (OS version, compiler, etc.)
- Provide error messages or screenshots if applicable

### Suggesting Features

- Open a GitHub Issue with the "enhancement" label
- Clearly describe the feature and its use case
- Explain why this feature would be useful

### Pull Requests

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/your-feature-name`
3. **Make your changes**:
   - Follow the existing code style
   - Add comments for complex logic
   - Test thoroughly
4. **Commit**: `git commit -m "Add: your feature description"`
5. **Push**: `git push origin feature/your-feature-name`
6. **Submit a Pull Request**

## Code Guidelines

### C++ Style
- Use C++17 features appropriately
- Follow modern C++ best practices
- Use meaningful variable and function names
- Add comments for non-obvious code
- Keep functions focused and concise

### Formatting
- Indent with 4 spaces (no tabs)
- Use `camelCase` for variables and functions
- Use `PascalCase` for classes and structs
- Place braces on same line for functions: `void Function() {`

### Commit Messages
- Use present tense: "Add feature" not "Added feature"
- Start with a verb: Add, Fix, Update, Remove, Refactor
- Keep first line under 50 characters
- Add detailed description if needed

## Building & Testing

### Build the project
```powershell
g++ -std=c++17 -O3 -mwindows -Iinclude -o build/FLOW.exe src/main_gui_v2.cpp src/FlowEngine.cpp -luser32 -lgdi32 -lcomctl32 -static-libgcc -static-libstdc++
```

### Test your changes
- Test on Windows 10 and 11
- Verify all features work as expected
- Check for memory leaks
- Ensure performance is not degraded

## Questions?

Open an issue or discussion on GitHub if you have questions about contributing.

Thank you for helping make FLOW better!
