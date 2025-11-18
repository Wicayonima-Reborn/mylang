## 📜 Changelog
See the full changelog here → [CHANGELOG.md](CHANGELOG.md)

# MyLang Compiler (`mycc`)

<img src="./logo.svg">

MyLang is a lightweight programming language inspired by C, but equipped with Rust-like ownership:
- Move semantics  
- Immutable borrow (`&T`) and mutable borrow (`&mut T`)  
- Automatic drop for heap types (e.g., `string`)  
- Static borrow checker (no garbage collector)

The compiler is fully written in **C (C99)** and produces **x86_64 NASM assembly**, targeting:
- Linux (ELF64 / SysV ABI)
- Windows (Win64 / Microsoft x64 ABI)

---

## ✨ Language Features

### Data Types
- `int`
- `string`
- `&T` (immutable reference)
- `&mut T` (mutable reference)
- `Rc<T>` (planned feature)

### Variables
```mylang
let x: int = 10;
let s: string = "Hello";

let y = x;        // move: x becomes invalid

let r = &x;       // immutable borrow
let mr = &mut x;  // mutable borrow
```

### Control Flow
```mylang
if (cond) {
} else {
}

while (cond) {
}
```

### Built-in Functions
- `print(x)`
- `clone(s)`
- Automatic drop for `string` at scope exit

---

## 🧠 Compiler Architecture

Compiler pipeline:

1. **Lexer** — converts characters to tokens  
2. **Parser** — recursive descent → AST  
3. **AST** — structural program representation  
4. **Semantic Analyzer**  
   - Type checking  
   - Scope validation  
   - Undefined variable detection  
5. **Borrow Checker**  
   - No use-after-move  
   - Many immutable borrows allowed  
   - Only one mutable borrow  
   - Borrow cannot outlive owner  
   - No mixing mutable + immutable borrow  
6. **Code Generator**  
   - Outputs NASM x86_64 assembly  
   - Supports ELF64 & Win64  
7. **Runtime Library**  
   - `runtime_new_string`  
   - `runtime_clone_string`  
   - `runtime_print_string`  
   - `runtime_print_int`  

---

## 📁 Directory Structure

```
mylang/
├── include/
├── src/
├── examples/
├── docs/
└── Makefile
```

---

## 🔧 Building the Compiler

### Linux
```sh
make
```

### Windows (MSYS2 / Mingw64)
```sh
make
```

---

## ▶️ Compiling a MyLang Program

```sh
mycc input.my -o output
```

---

## ⚙️ Assemble & Link (using Makefile)

```sh
make example
make run-example
```

---

## 🧪 Example Program

```mylang
let s: string = "Hello!";
print(s);

let n: int = 5;
print(n);
```

---

## ❗ Borrow Checker Examples

### Error: Use-after-move
```mylang
let a = 10;
let b = a;
print(a);     // ERROR: a was moved
```

### Error: Mutable + Immutable Borrow
```mylang
let x = 1;
let r = &x;
let m = &mut x;   // ERROR: cannot mutably borrow while immutably borrowed
```

---

## 📜 License
MIT License.
[LICENSE](LICENSE)



