# TLC — Trading Language Compiler  
*A tiny domain-specific language (DSL) for stock market trading rules*

TLC is a **custom programming language** designed specifically for expressing stock-based trading logic.

It includes:

- A lexer (tokenizer)
- A parser (AST builder)
- A bytecode compiler
- A stack-based virtual machine (VM)

All written in **pure C** — no dependencies, no external libraries.

---

## ✨ Why TLC?

Most trading systems are built by forcing logic into:
- Python scripts
- Pandas pipelines
- Excel formulas
- Backtesting frameworks
- Pine Script (locked to TradingView)

**TLC goes deeper** — it gives you:

✔ Your own trading language  
✔ Your own compiler  
✔ Your own runtime  
✔ Full control and extensibility  

This means you're not *using* a platform —  
**you are building one.**

---

## 🔍 Example Code

### Moving Average Strategy

```tl
symbol "NIFTY"

if close > sma(close, 20) then
    buy 10
end


 Build

Requires GCC or Clang.

gcc -std=c11 -Wall -O2 main.c lexer.c parser.c vm.c -o tlc

On success, you'll get an executable:
./tlc

 Run

Create a .tl strategy file:

echo '
symbol "NIFTY"

if close > 100 then
    buy 10
end
' > strategy.tl

Then run:
./tlc strategy.tl

Sample output:
SYMBOL NIFTY: BUY 10


How It Works

TLC reads the .tl source code

Lexer converts it to tokens

Parser builds an AST

Compiler emits bytecode

VM executes bytecode one candle at a time

You supply the OHLCV + timestamp —
TLC supplies the decision logic.


Integration

You can embed TLC into:

C/C++ apps

Python (via FFI)

Algo trading engines

Backtest systems

Live market feed handlers

No runtime dependencies.
No VM installation.
No garbage collector.
No Python required.

Current Status

Version: 0.2 MVP (Minimal Viable Platform)
Works: Yes
Limitations:

No variables yet

No user-defined functions

Indicators are stubs (demo only)

No else blocks

No multi-symbol mode

No time-frame selection

License

MIT License
Free for personal, academic, and commercial use.

This software comes with:

No restrictions

No lock-in

No proprietary dependencies
