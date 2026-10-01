# 工作任务

> 历史初始任务。原目录设计、代理安排和设备连接状态仅记录当时要求；当前结构以根目录 README 和 AGENTS 为准。

根据任务内容，指派 subagents 执行、验证任务，由你负责指挥、验收。

## 任务内容

### esp32 bot ux

实现 grok bot 的 bot 可控动画组件。要求设计合理、功能充沛、性能优秀、个性化，可以被如下设备集成使用。

### m5stack stopwatch

> https://docs.m5stack.com/en/core/StopWatch

- 集成 bot ux
- 根据设备特点增强 bot ux 的互动性等
- 具备一定的手表功能（不是秒表，不需要秒表功能），展示时间、电量、充电等
- 设置页

### m5stack core2

> https://docs.m5stack.com/en/core/Core2_v1.3

- 集成 bot ux
- 根据设备特点增强 bot ux 的互动性等
- 模拟 codex 定制键盘（Codex Micro）的功能
- 根据设备特点设计模拟的 Codex Micro 的功能的交互等
- 设置页
- 装配了 M5GO Battery Bottom2 (for Core2 only) V1.3 (https://docs.m5stack.com/en/base/Base_M5GO_Bottom2_v1.3)

### 技术&架构

- 目录设计：
  - stopwatch
    - bot-ux-watch
  - core2
    - bot-ux-codex-core2
  - bot-ux
- 多级目录，添加 AGENTS.md
- tmp：临时目录

## subagents 列表

> 根据如下要求和定义创建 subagents
> 模型使用 {gpt 5.6 sol}(high medium) 或者 {gpt 5.6 terra fast}(high xhigh)

- researcher：检索信息，汇总资料。
- ux professor：擅长用户交互、体验设计，把关 developer 实现的 UI。
- esp32 professor：esp32 开发专家，深度了解 esp32、m5stack 技术栈，负责技术方案和代码的 review 、工程架构设计。
- m5stack stopwatch developer：m5stack stopwatch 的开发者。
- m5stack core2 developer：m5stack core2 的开发者。
- m5stack stopwatch QA：m5stack stopwatch 的测试人员。
- m5stack core2 QA：m5stack core2 的测试人员。

## 执行建议

- 添加 .gitignore ，将临时文件放入 tmp 内，其他临时目录、产物目录、依赖目录记录到 .gitignore 中。
- 使用 subagents 交叉、并行执行。
- 不过度设计边界场景。
- 优先执行开发和 code review，对于需要人工参与和测试环境的验收先跳过。
- 可自行安装依赖和必要的工具。
- 允许自主决策。
- 按需合理 commit 和切出 worktree 。
- 两个设备都已经连接。
- 当前的 bot-ux 的实现我很不满意，与 grok bot 的 bot 动画相差很大。
- core2 的实现的键盘也不是我期望的 codex micro 的模拟。
  