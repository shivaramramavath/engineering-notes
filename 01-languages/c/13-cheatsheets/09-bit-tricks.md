# Bit Manipulation Tricks and Idioms (`09-bit-tricks`)

## 1. The 4 Fundamental Mask Operations
```c
// 1. Set bit n
val |= (1U << n);

// 2. Clear bit n
val &= ~(1U << n);

// 3. Toggle bit n
val ^= (1U << n);

// 4. Test bit n
bool is_set = (val & (1U << n)) != 0;
```

## 2. Classic Bitwise Idioms
```c
// Check if power of two
bool is_pow2 = (x != 0) && ((x & (x - 1)) == 0);

// Isolate lowest set bit
uint32_t lowest = x & (-x);

// Clear lowest set bit
x &= (x - 1);

// Branchless Minimum of two integers
int min_val = b ^ ((a ^ b) & -(a < b));

// Branchless Maximum of two integers
int max_val = a ^ ((a ^ b) & -(a < b));

// Branchless Sign check (-1 if negative, 0 if positive)
int sign = (x >> 31);

// Fast 32-bit Endian Byte Swap
uint32_t bswap32(uint32_t x) {
    return ((x >> 24) & 0x000000FF) |
           ((x >> 8)  & 0x0000FF00) |
           ((x << 8)  & 0x00FF0000) |
           ((x << 24) & 0xFF000000);
}
```
