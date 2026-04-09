# C++ Skills & Engineering Guidelines (skills.md)

> 面向高级 C++ 架构与工程实践，指导代码生成 Agent 在设计与实现过程中遵循高性能、可维护、可扩展的原则。

---

## 0. 其他注意事项
- 尽量使用::而不是using namespace


## 1. 性能优化（Performance Optimization）

### 1.1 编译速度优化（Compile-Time Optimization）

- **头文件依赖控制**
  - 严格遵循 *Include What You Use (IWYU)* 原则，避免不必要的头文件传递依赖。
  - 优先使用前向声明（forward declaration）替代完整头文件引用，尤其在指针/引用语义下。
  - 禁止在头文件中包含大型标准库或第三方库（如 `<vector>`, `<map>`），除非接口确实依赖。

- **头文件结构设计**
  - 避免在头文件中定义复杂模板逻辑或大规模 inline 实现（除非必要）。
  - 将实现细节移入 `.cpp` 或 `.inl` 文件。
  - 使用 PImpl（Pointer to Implementation）隐藏实现细节，降低编译耦合。

- **模板与泛型编程**
  - 控制模板实例化数量，避免无约束泛型导致代码膨胀。
  - 优先使用 concept / requires（C++20）约束模板参数，减少错误实例化。
  - 避免在模板中嵌套复杂逻辑（如递归模板元编程），优先使用 constexpr 替代。

- **编译单元划分**
  - 合理拆分模块，避免“巨型翻译单元（translation unit）”。
  - 使用 Unity Build 仅在编译速度成为瓶颈时考虑。

---

### 1.2 运行时性能优化（Runtime Performance）

- **内存访问与缓存友好性**
  - 优先使用连续内存结构（如 `std::vector`），避免链式结构（如 `std::list`）。
  - 减少 cache miss：数据结构应遵循 *Structure of Arrays (SoA)* 或 *Array of Structures (AoS)* 的合理选择。
  - 避免 false sharing（尤其在多线程环境）。

- **对象构造与拷贝**
  - 避免不必要的临时对象创建（RVO / NRVO 优化）。
  - 优先使用 `emplace_back` 而非 `push_back`。
  - 对于大型对象，明确区分 copy 与 move 语义（Rule of Five）。

- **动态分配控制**
  - 避免频繁 heap 分配（new/delete），优先使用对象池（Object Pool）或内存池。
  - 对高频路径使用自定义 allocator。
  - 明确所有权，避免隐式共享导致不可控分配。

- **分支预测与指令流水**
  - 减少不可预测分支（branch misprediction）。
  - 使用 likely/unlikely（或编译器内建）优化热点路径。
  - 将热路径（hot path）代码线性化。

- **并发与多线程**
  - 避免过度锁竞争，优先使用 lock-free 或细粒度锁。
  - 明确线程模型（task-based vs thread-based）。
  - 使用 `std::atomic` 时需明确 memory order（不要默认 sequential consistency）。

---

## 2. 设计模式与架构（Design Patterns & Architecture）

### 2.1 依赖关系管理（Dependency Management）

- **依赖方向**
  - 高层模块不依赖低层实现（依赖倒置原则，DIP）。
  - 使用接口（abstract class / concept）解耦实现。

- **编译期 vs 运行时依赖**
  - 编译期依赖（模板）用于性能关键路径。
  - 运行时依赖（虚函数 / 接口）用于扩展性与解耦。

- **依赖注入（DI）**
  - 明确依赖通过构造函数注入，而非隐式获取（如全局变量）。
  - 禁止 Service Locator 滥用。

---

### 2.2 对象所有权与生命周期（Ownership & Lifetime）

- **所有权语义必须显式表达**
  - 使用类型表达所有权：
    - `std::unique_ptr` → 独占所有权
    - `std::shared_ptr` → 共享所有权（慎用）
    - 原始指针（raw pointer）→ 非拥有引用（non-owning）

- **生命周期管理原则**
  - 对象生命周期必须由单一责任方控制（Single Ownership Principle）。
  - 避免循环引用（shared_ptr + weak_ptr 组合解决）。
  - 禁止“隐式延长生命周期”（如返回局部对象引用）。

- **RAII（Resource Acquisition Is Initialization）**
  - 所有资源（内存、文件、锁）必须绑定到对象生命周期。
  - 禁止裸资源管理（裸 new/delete, fopen/fclose）。

---

### 2.3 常见设计模式（Patterns）

- **工厂模式（Factory）**
  - 用于解耦对象创建与使用。
  - 优先返回 `std::unique_ptr`，避免裸指针。

- **策略模式（Strategy）**
  - 使用函数对象（functor）或 `std::function` 实现可替换行为。
  - 性能关键路径避免虚函数，可使用模板策略。

- **观察者模式（Observer）**
  - 使用弱引用（weak_ptr）避免订阅者生命周期问题。
  - 注意事件风暴（event storm）风险。

- **组件化设计（Component-Based Design）**
  - 优先组合（composition）而非继承（inheritance）。
  - 避免深层继承树（>3 层）。

---

## 3. STL 使用规范（Standard Library Usage）

### 3.1 容器选择（Container Selection）

- **`std::vector`（首选容器）**
  - 默认容器选择，除非有明确理由。
  - 提前 `reserve()` 避免扩容。
  - 避免频繁 erase（会触发移动）。

- **`std::unordered_map` vs `std::map`**
  - 高频查找 → `unordered_map`
  - 有序需求 / 小数据量 → `map`

- **避免使用**
  - `std::list`：缓存不友好，除非频繁中间插入。
  - `std::deque`：除非需要双端操作。

---

### 3.2 智能指针（Smart Pointers）

- **`std::unique_ptr`（默认选择）**
  - 表达明确所有权。
  - 支持 move，不支持 copy。

- **`std::shared_ptr`（谨慎使用）**
  - 仅在确实需要共享生命周期时使用。
  - 注意引用计数开销与循环引用。

- **`std::weak_ptr`**
  - 用于打破循环引用。
  - 使用前必须 `lock()` 检查有效性。

---

### 3.3 `std::optional`

- 用于表达“可能不存在的值”，避免使用 magic value（如 -1/null）。
- 不用于表示错误（错误应使用 `expected` 或异常）。
- 避免嵌套 optional（optional<optional<T>>）。

---

### 3.4 算法与迭代器（Algorithms & Iterators）

- 优先使用 STL 算法（`std::transform`, `std::sort`），避免手写循环。
- 使用 range-based for 提高可读性。
- 注意 iterator invalidation（尤其 vector 扩容）。

---

## 4. 代码健壮性与可维护性（Robustness & Maintainability）

- **const correctness**
  - 所有不修改状态的函数必须标记为 `const`。
  - 输入参数优先使用 `const&`。

- **错误处理**
  - 明确策略：异常 vs 返回值。
  - 不允许 silent failure（必须显式处理错误）。

- **日志与调试**
  - 热路径避免日志输出。
  - 使用编译期开关控制 debug 信息。

- **命名与语义**
  - 名称必须表达语义（ownership、生命周期、用途）。
  - 禁止模糊命名（如 data, obj, tmp）。

---

## 5. Agent 编码约束（For Code Generation Agents）

- 所有资源必须使用 RAII 管理。
- 默认使用 `std::vector` 和 `std::unique_ptr`。
- 所有接口必须明确所有权语义。
- 禁止隐式全局状态。
- 性能关键路径必须避免：
  - 多余拷贝
  - 动态分配
  - 虚函数调用（除非必要）
- 所有设计必须优先考虑：
  - cache locality
  - 可测试性
  - 可扩展性

---

## 6. 总结性原则（Core Principles）

- **性能是设计结果，而非事后优化**
- **所有权必须显式表达**
- **依赖必须可控**
- **数据布局优先于算法优化**
- **简单优于复杂（但不牺牲正确性）**

---
