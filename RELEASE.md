# MyLang v0.1 — Safe Native Foundations

This is the first public release of **MyLang**, a minimal systems language combining **safety**, **simplicity**, and **native speed**.  
It features full ownership semantics, a working borrow checker, and native cross-platform machine code generation.

---

## ⭐ Features

- Move semantics & borrowing  
- Native NASM backend  
- Win64 & SysV ABI support  
- Minimal runtime (heap-allocated strings, printing, cloning)  
- Basic language syntax (`let`, `if`, `while`, `print`)  
- Full compiler pipeline:
  - Lexer  
  - Parser  
  - AST generation  
  - Semantic analysis  
  - Borrow checker  
  - NASM codegen  
  - Linking to native executable  

---

## 🖥 Platform Support

- **Windows (x86_64)**  
- **Linux (x86_64)**  

---

## 🛠 Installation

Clone the repository and build using:

```sh
make
```

This will compile the compiler (`mycc`) and runtime components.

---

## 🧭 Roadmap (v0.2 and beyond)

- `func` keyword + function system  
- `func main()` entrypoint  
- Structs & methods  
- Trait system  
- Internal IR  
- Optimization passes  
- `mylang run` high-level command  
- Formatter & code style tools  

---

## 🎨 Logo (SVG)

```svg
<svg width="400" height="150" viewBox="0 0 400 150" xmlns="http://www.w3.org/2000/svg"><rect width="400" height="150" fill="#2b2b2b"/><circle cx="120" cy="60" r="10" fill="#4caf50"/><circle cx="180" cy="60" r="10" fill="#4caf50"/><path d="M95 90 Q150 140 205 90" stroke="#4caf50" stroke-width="12" fill="transparent" stroke-linecap="round"/><text x="280" y="100" text-anchor="middle" font-size="50" font-family="Arial, sans-serif" fill="white">MYLANG</text></svg>
```
