# c_memory_allocator

利用 Linux `sbrk` 系統呼叫，從零實作 `malloc`、`free`、`calloc`、`realloc` 四個動態記憶體配置函式。

這是一個學習向的小型專案，目的是理解 C 語言動態記憶體配置在最底層的大致運作，並非用於生產環境。

## 實作內容

| 函式 | 對應標準函式 | 說明 |
|------|------------|------|
| `mmalloc(size)` | `malloc` | 透過 `sbrk` 從 heap 配置一塊記憶體 |
| `mfree(ptr)` | `free` | 釋放先前配置的記憶體；若為最後一個 block 則歸還給 OS |
| `mcalloc(num, nsize)` | `calloc` | 配置零初始化的記憶體，含乘法溢位檢查 |
| `mrealloc(ptr, size)` | `realloc` | 重新調整配置大小，必要時複製資料到新的 block |

## 運作原理

每個配置的 block 前面都帶有一個 **header**，記錄該 block 的大小、是否空閒、以及指向下一個 block 的指標。所有 header 為 singly linked list。

```
          head                                    tail
           |                                       |
           v                                       v
     +-----------+--------+    +-----------+--------+
     |  header   |  data  |--->|  header   |  data  |
     +-----------+--------+    +-----------+--------+
     | size      |        |    | size      |        |
     | is_free   |        |    | is_free   |        |
     | next  ----+--------+    | next=NULL |        |
     +-----------+--------+    +-----------+--------+

                       Program Break (brk) ----^
```

**配置流程 (`mmalloc`)：**
1. 掃描鏈結串列，尋找第一個夠大的空閒 block（first-fit）
2. 若找到，標記為已使用並回傳
3. 若找不到，透過 `sbrk` 擴展 heap，建立新的 block

**釋放流程 (`mfree`)：**
- 若該 block 位於 heap 的尾端（緊鄰 program break），透過 `sbrk(-size)` 縮小 heap 並從鏈結串列移除
- 否則，僅將該 block 標記為空閒，留給未來的 `mmalloc` 重複使用

## 編譯與執行

```bash
gcc -o allocator main.c -lpthread
./allocator
```

程式內透過一系列 `assert` 進行驗證——若執行後無任何輸出，代表所有測試通過。

## FAQ

### Q: 為什麼 header 使用 `union` 而非單純的 `struct`？

`union` 中的 `char stub[32]` 確保 header 至少佔 32 bytes，讓後方的資料區域自然對齊。

### Q: 為什麼使用 first-fit 而非 best-fit？

First-fit 較簡單且較快——找到第一個夠大的 block 就直接使用。Best-fit 需要掃描整條串列找到最小的足夠 block，能減少浪費但增加配置時間。作為學習專案，first-fit 能清楚展示核心概念。

### Q: 為什麼 `mfree` 只有在最後一個 block 時才會歸還記憶體給 OS？

`sbrk` 只能移動 program break，因此只有位於 heap 頂端（緊鄰 `brk`）的 block 才能歸還給 OS，中間的 block 只能標記為空閒，等待未來配置時重複使用。

### Q: 這個 allocator 是 thread-safe 的嗎？

基本上是。所有配置與釋放操作都受一個全域 `pthread_mutex` 保護。但這也意味著所有執行緒都在爭搶同一把鎖。

### Q: 被標記釋放的 block 在被重用時，是否會做 block 的分割？

目前實作會直接重用整個 block，因此沒有分割。例如一個 1024 bytes 的空閒 block 被用於 16 bytes 的請求，將造成 1008 bytes 的浪費。

### Q: 為什麼對齊到 16 bytes？

```c
size = (size + 15) & ~((size_t)15);
```

16 bytes 對齊滿足 x86-64 上大多數資料型別的對齊需求。C 標準要求 `malloc` 回傳的記憶體必須適合任何標準型別的對齊。

## 參考資料

- [A. Kuznetsov — Write a simple memory allocator](https://arjunsreedharan.org/post/148675821737/memory-allocators-101-write-a-simple-memory)
- [glibc malloc internals](https://sourceware.org/glibc/wiki/MallocInternals)
- [Doug Lea's malloc (dlmalloc)](http://gee.cs.oswego.edu/dl/html/malloc.html)

## 授權

本專案僅供學習用途，可自由使用。
