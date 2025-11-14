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

## 🎨 Logo (preview)

<img src="logo.svg" width="400">
