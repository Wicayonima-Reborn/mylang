# MyLang Compiler (`mycc`)

![Build](https://img.shields.io/badge/build-manual-lightgrey)
![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-brightgreen)
![Arch](https://img.shields.io/badge/arch-x86__64-orange)

<img src="./logo.svg" alt="MyLang Logo" width="360" />

MyLang is a **lightweight systems programming language** inspired by C, designed with **Rust-like ownership and borrowing**, but implemented with a **minimal and explicit model**.

The main goal of MyLang is to provide **memory safety without garbage collection**, while keeping the compiler simple, transparent, and educational.

The compiler is written entirely in **C (C99)** and generates **x86_64 NASM assembly**, targeting:

* **Linux** (ELF64 / SysV ABI)
* **Windows** (Win64 / Microsoft x64 ABI)

> Philosophy: *C-like syntax, Rust-like safety, no GC, no hidden magic.*

---

## Key Features

* Explicit move semantics (single ownership model)
* Immutable (`&T`) and mutable (`&mut T`) borrowing
* Static borrow checker (compile-time enforcement)
* Automatic `drop` for heap-allocated types (`string`)
* No garbage collector
* Clear and strict diagnostics for ownership and borrow errors

---

## Design Goals

MyLang is built with the following goals in mind:

* **Predictable memory behavior** — no runtime GC, no implicit cloning
* **Explicit ownership** — every value has a clear owner
* **Compile-time safety** — memory errors are rejected before execution
* **Minimal compiler complexity** — easy to read, hack, and extend
* **Educational value** — suitable for learning compiler construction and ownership models

---

## ❓ Why MyLang Exists

Modern systems languages often trade simplicity for abstraction.

MyLang exists to explore a middle ground:

* Safer than C
* Simpler than Rust
* Explicit rather than magical

It is designed for:

* Compiler and language design experiments
* Studying ownership and borrow checking concepts
* Building small, safe, low-level programs

---

## Data Types

* `int`
* `string` (heap-allocated, automatically dropped)
* `&T` (immutable reference)
* `&mut T` (mutable reference)
* `Rc<T>` *(planned, not implemented yet)*

Example:

```mylang
let x: int = 10;
let s: string = "Hello";

let y = x;        // move → x becomes invalid

let r = &y;       // immutable borrow
// let mr = &mut y; // ERROR while r is alive
```

---

## Variables & Ownership

* All assignments use **move semantics** by default
* After a value is moved, the previous variable becomes invalid

```mylang
let a = 10;
let b = a;
print(a); // ERROR: use-after-move
```

Explicit cloning for heap types:

```mylang
let s1: string = "hi";
let s2 = clone(s1); // deep clone
```

---

## 🔁 Control Flow

```mylang
if (cond) {
    print(1);
} else {
    print(0);
}

while (cond) {
    print(cond);
}
```

---

## 🧠 Borrow Checker Rules

The MyLang borrow checker enforces:

* No use-after-move
* Multiple immutable borrows are allowed
* Only one mutable borrow at a time
* Mutable and immutable borrows cannot coexist
* Borrows must not outlive their owner

### ❌ Mutable + Immutable Borrow

```mylang
let x = 1;
let r = &x;
let m = &mut x; // ERROR
```

### ❌ Use-after-move

```mylang
let a = 10;
let b = a;
print(a); // ERROR
```

---

## Built-in Functions

* `print(int)`
* `print(string)`
* `clone(string) -> string`
* Automatic `drop` at scope exit

---

## Compiler Architecture

Compiler pipeline:

1. **Lexer** — characters → tokens
2. **Parser** — recursive descent → AST
3. **AST** — structural representation
4. **Semantic Analyzer**

   * Type checking
   * Scope validation
   * Undefined variable detection
5. **Borrow Checker** — ownership & lifetime validation
6. **Code Generator**

   * NASM x86_64 assembly
   * ELF64 & Win64 ABI
7. **Runtime Library**

   * `runtime_new_string`
   * `runtime_clone_string`
   * `runtime_print_string`
   * `runtime_print_int`

---

## Directory Structure

```
mylang/
├── include/     # Headers
├── src/         # Compiler source
├── runtime/     # Runtime library
├── examples/    # Example programs
├── docs/        # Documentation
└── Makefile
```

---

## 🔧 Building the Compiler

### Linux

```sh
make
```

### Windows (MSYS2 / MinGW64)

```sh
make
```

---

## Compiling a Program

```sh
mycc input.my -o output
```

---

## Example Program

```mylang
let s: string = "Hello!";
print(s);

let n: int = 5;
print(n);
```

---

## Project Status

This project is **early-stage and experimental**.

* Not all features are stable
* APIs and semantics may change
* Intended primarily for learning and experimentation

---

## Roadmap

* `Rc<T>` / shared ownership
* Functions and return values
* Structs and user-defined types
* Improved borrow checker diagnostics
* Simple optimizer

---

## Changelog

See full history at:
👉 [CHANGELOG.md](CHANGELOG.md)

---

## License

MIT License. See [LICENSE](LICENSE).
