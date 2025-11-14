# MyLang Compiler (`mycc`)

MyLang adalah bahasa pemrograman sederhana mirip C, tetapi memiliki sistem *ownership* ala Rust:
- Mendukung move semantics
- Borrow (`&T`) dan mutable borrow (`&mut T`)
- Drop otomatis untuk tipe heap (misalnya `string`)
- Borrow checker statis (tanpa garbage collector)

Compiler ini ditulis sepenuhnya dalam **C (C99)** dan menghasilkan **assembly x86_64 (NASM)** yang dapat dijalankan di:
- Linux (ELF64 / SysV ABI)
- Windows (Win64 / Microsoft x64 ABI)

---

## ✨ Fitur Bahasa

### Tipe Data
- `int`
- `string`
- `&T` immutable reference
- `&mut T` mutable reference
- `Rc<T>` (future plan)

### Variabel
```
let x: int = 10;
let s: string = "Hello";
let y = x;    // move: x tidak valid lagi
let r = &x;   // borrow
let mr = &mut x; // mutable borrow
```

### Kontrol Alur
```
if (cond) {
} else {
}

while (cond) {
}
```

### Built-in Functions
- `print(x)`
- `clone(s)`
- Auto-drop string pada scope exit

---

## 🧠 Arsitektur Compiler

Pipeline compiler:

1. **Lexer**  
   Membaca karakter → token.

2. **Parser**  
   Recursive descent parser → AST.

3. **AST**  
   Representasi struktur program.

4. **Semantic Analyzer**  
   Mengecek:
   - Type checking  
   - Scope  
   - Variabel tidak dideklarasi  

5. **Borrow Checker**  
   Memastikan:
   - After move, var invalid  
   - Banyak immutable borrow OK  
   - Hanya satu mutable borrow  
   - Borrow tidak melebihi lifetime owner  
   - Tidak boleh mutable + immutable bersamaan  

6. **Code Generator**  
   Menghasilkan NASM x86_64 ASM:
   - ELF64 (Linux)  
   - Win64 (Windows)  

7. **Runtime**  
   Implementasi:
   - `runtime_new_string`
   - `runtime_clone_string`
   - `runtime_print_string`
   - `runtime_print_int`

---

## 📁 Struktur Direktori

```
mylang/
├── include/
├── src/
├── examples/
├── docs/
└── Makefile
```

---

## 🔧 Build Compiler

### Linux
```
make
```

### Windows (MSYS2/Mingw64)
```
make
```

---

## ▶️ Compile Program MyLang

```
mycc input.my -o output
```

---

## ⚙️ Assemble & Link (via Makefile)

```
make example
make run-example
```

---

## 🧪 Contoh Program

```
let s: string = "Hello!";
print(s);

let n: int = 5;
print(n);
```

---

## ❗ Borrow Checker Contoh

### Error (use-after-move)
```
let a = 10;
let b = a;
print(a);   // ERROR
```

### Error (mutable + immutable borrow)
```
let x = 1;
let r = &x;
let m = &mut x;  // ERROR
```

---

## 📜 Lisensi
MIT License.