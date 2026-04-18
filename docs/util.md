# util/

General small utility helpers.

## `phu::StringUtil::safe_strncpy`
**Purpose**
- Safe bounded C-string copy with forced null termination.

**Typical use case**
- Copy user-entered labels into fixed-size network or UI buffers without overflow.

**Need to know**
- Signature: `safe_strncpy(char* dest, const char* src, size_t dest_size)`.
- No-op when `dest_size == 0`.
- Writes `dest[dest_size - 1] = '\0'`.

**Apply when**
- Filling fixed-size protocol/string buffers safely.

**Don’t apply when**
- Using dynamic `std::string` end-to-end.

**Example**
```cpp
char label[32] = {};
phu::StringUtil::safe_strncpy(label, userText.c_str(), sizeof(label));
```
