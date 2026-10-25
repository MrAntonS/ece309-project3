# Project 3 Design Log — Tool Registry and Tool Execution

**P2 code used**: My own Project 2 implementation (USE_P2_REFERENCE=OFF).
**Tools used:** CMake, GCC, AddressSanitizer, UndefinedBehaviorSanitizer, and CTest were used for verification.

## 1. Hash table design

I chose **separate chaining** rather than open addressing. Each bucket in a vector contains a vector of `std::unique_ptr<Tool>`. `find` hashes a name and scans only its bucket. Chaining handles collisions without complicated probing logic or a completely full table. Linear probing is cache-friendly, but clusters can make lookups expensive as load factor grows. Here, predictable behavior and clear ownership outweigh that potential benefit.

My hash uses **64-bit FNV-1a**: it processes every byte, XORs it into the running hash, and multiplies by a prime. The `hash` method returns the full-width value; bucket indexing separately uses `hash(key) % bucket_count`. The order of characters affects the result. A weak alternative, `key[0] + key.size()`, ignores most characters and collides heavily for similar tool names.

The registry starts at **16 buckets**. If the next insertion would raise the load factor above **0.75**, it doubles capacity and reinserts every existing entry at the newly computed index **before** adding the new tool. This keeps `load_factor()` below the threshold after each successful insertion. Duplicate tools and null pointers are rejected without changing the table's size; rejecting a duplicate destroys the new object while preserving the original.

The 10,000-name benchmark (`tool_0` through `tool_9999`) measured:

| Strategy | Buckets | Load factor | Longest chain |
|---|---:|---:|---:|
| FNV-1a / chaining | 16,384 | 0.610352 | **6** |
| Weak hash at same capacity | 16,384 | 0.610352 | **9,000** |

Both have the same average load, but the weak hash concentrates most entries into a few buckets. A resize takes O(n), yet capacity doubles, leaving many cheap inserts before another resize. Spread over those inserts, the cost is **amortized O(1)**. Expected lookup is O(1) when names are well distributed, though worst-case collisions can still yield O(n).

## 2. Pointer stability and ownership

The buckets own `std::unique_ptr<Tool>` values; `find` returns a non-owning `Tool*`. Rehashing moves unique pointers, **not** the heap-allocated `Tool` objects. Consequently, `Tool*` addresses survive table resizing. Destroying the registry frees each tool once. The tests check pointer identity after hundreds of insertions and verify destructor counts, including rejection of duplicates.

## 3. Calculator: two-stack trace

To evaluate `2 * (3 + 4) - 5`, I keep a **values** stack and an **operators** stack. After reading `2`, values are `[2]` and operators are empty. `*` goes onto the operators stack, then `(`: values `[2]`, operators `[*, (]`. Reading `3`, `+`, and `4` produces values `[2, 3, 4]`, operators `[*, (, +]`.

On `)`, I apply `+`, replacing `3` and `4` with `7`, and remove `(`: values `[2, 7]`, operators `[*]`. When `-` arrives, the pending `*` has higher precedence, so applying it yields values `[14]`. I then push `-`, followed by the value `5`: values `[14, 5]`, operators `[-]`. End of input applies subtraction, leaving **9**. Applying an earlier operator when its precedence is **greater than or equal to** the incoming operator preserves left associativity (`10 - 4 - 3` equals `3`). The algorithm uses loops, avoiding call-stack overflow on deeply nested input.

## 4. Errors as values

A tool failure is routine data in an agent conversation: files can be missing, paths unsafe, and arithmetic invalid. Returning `false` with an explanation, or `ToolResult::failure`, lets the harness report the issue to the model instead of interrupting control flow with an exception. This also enables the model to recover. Exceptions are suitable for programmer mistakes such as asking a JSON-number value to behave like a string. The calculator guards every stack access so even unexpected token sequences return errors rather than crash. `Workspace::resolve` rejects unsafe paths without touching disk; a separate provided function checks symbolic links.

## 5. What I would change

I would store each tool's name as a separate immutable key next to its `unique_ptr`. The current design repeatedly calls `tool->name()` while looking up entries and rehashing, which may allocate temporary strings. Cached keys could improve efficiency and make identity explicit. I kept the simpler design because it meets the assignment and makes the ownership model easy to inspect.
