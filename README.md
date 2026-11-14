# 闪词（ShanCi）· 四六级单词辅助背诵

一个**离线、单用户**的四六级（CET-4/6）英语单词辅助背诵系统，使用 **C++17 + Qt 6 Widgets + SQLite** 实现。

系统以“离线账户 + 手动同步”为基础，以**单词本、错题本、每日词表、字典查询**为学习入口，以**题目生成、做题、结果统计、错题复习**为主要手段，以**本地 SQLite + 本地文件**为数据支撑，帮助用户提高四六级单词水平。

---

## 一、功能概览

| 模块 | 主要功能 |
| --- | --- |
| 登录界面 | 离线登录、新建账户、选择本地账户、**记住上次登录账号**、**可选保持登录状态（下次自动登录）**、**重新设置密码** |
| 主界面 | 学习数据概览（词库/单词本/错题本/今日词表）、功能导航 |
| 查询字典 | **在词库中查询**、**搜索框下方实时联想（最多 5 条，点击即跳转）**、查看详情（只读）、导入词表、加入单词本 |
| 单词本 | **可创建多个单词本（收藏夹）**：新建/重命名/删除、查询筛选、查看详情、编辑、移出、批量添加词书 |
| 错题本 | 查看（含**编号**与**错题关联单词**列）、筛选、编辑、移除、标记已掌握 |
| 每日词表 | **多个自定义背诵计划**：新建 / 选择 / 保存 / 删除；设置背诵词书、背诵天数、每天背新词数、每天复习数；用所选计划生成今日词表（新词 + 到期复习 + 错题复习）、查看进度与覆盖情况 |
| **练习** | **统一的做题入口**：选择来源（每日词表 / 单词本 / 错题本 / 词库随机），设置题量，开始做题 |
| 做题页 | 显示题目、选择/输入答案、提交、下一题、交卷（选择题选项优先取**形近词**干扰） |
| 结果页 | 得分与正确/错误统计、错题批量加入错题本、重做、返回主界面 |
| 个人设置 | **账户信息与修改密码**、每日词数、题型偏好、主题（浅色/深色）、同步路径、字号、音频开关、拼写严格校验、自动加错题 |
| 同步页 | 导出、导入、冲突处理（较新者优先/本地优先/导入优先/合并）、查看同步日志 |

**核心概念区分：**

- **词库（查询字典）**：随程序导入的四级/六级全部单词，用于查询与出题取材。
- **单词本（收藏夹）**：用户自建、可建多个，手动收录生词；与词库相互独立。
- **每日词表（背诵计划）**：从词库按顺序取新词、并复习到期单词，形成每日背单词任务。
- **错题本**：**只能由做题产生**——词不是题，因此任何"手动把单词加入错题本"的入口都已移除；
  只有答错的题（或从同步文件导入的历史错题）才会进入错题本。
- **练习**：唯一发起做题的入口，集中在左侧菜单，避免各页面重复出现"开始做题"。

### 账户与登录

- **记住上次登录账号**：登录成功后记录用户名，下次启动自动填入并选中对应本地账户。
- **保持登录状态（可选）**：勾选后保存一个由「用户 ID + 口令散列 + 盐」派生的令牌到 `config.json`
  （**不保存明文密码**）；下次启动直接进入主界面，不再弹出登录框。
  修改密码后令牌自动失效（因为派生自口令散列），需重新登录。
- **重新设置密码**：登录页「重新设置密码」或「个人设置 → 修改密码」，均需先校验原密码。

### 生成结果不为空

- 每日词表：无计划、或当天既无新词也无到期复习时，**返回错误而不创建空词表**；
  即便如此也不会删掉当天已有的词表（先生成成功再替换）。
- 题表：若所有单词都无法生成题目，会**回滚已写入的题表与题目**，不会残留 0 长度试卷。

### 题目生成与去歧义

生成题目时会主动消除歧义、提升质量：

| 题型 | 界面呈现 | 去歧义措施 |
| --- | --- | --- |
| 英文选中文释义 | 单词 + 音标，必要时附**英文释义** | 中文释义过短/多义/与词池中其他词同义时，自动补充英释；干扰项取**形近词**的释义（如 adapt / adopt / adept / adult） |
| 中文释义拼写英文 | 释义 + **首字母** + **字母数** + 字母占位（如 `a_______`） | 首字母与长度共同锁定唯一答案，避免多个近义词都能填 |
| 形近词匹配词义 | 中文释义（必要时附英释），选项为**形近单词** | 排除与正确答案同义/近义的干扰项，避免出现两个"正确答案" |
| 选词填空 | 英文例句挖空 | 干扰项取**同词性 + 不同义**的形近词，避免靠语法或语义排除；同义干扰项一律剔除 |

其他保证：

- 选项释义互为子串（如"适应"与"适应；适合"）会被判定为重复，不会同时出现。
- 选项数量不足时用合成干扰补齐，保证 4 个选项。
- 正确答案位置随机，不会固定在某一项。
- 解析中给出音标、英释、其他释义与例句。

**题目生成策略（第四层）**：英文选中文释义、中文释义拼写英文单词、相似词匹配词义、选词填空；内置干扰项生成与答案校验。

---

## 二、技术栈

- 语言 / 标准：C++17
- 界面：Qt 6.8 Widgets
- 数据库：SQLite（内置 amalgamation，`third_party/sqlite`）
- 数据交换：JSON（Qt JSON）
- 音频：Qt Multimedia（未安装时自动回退 Windows MCI）
- 构建：CMake + Ninja
- 测试：Qt Test
- 编译器：MinGW-w64 GCC 13.1（随 Qt 分发）

---

## 三、目录结构

```
工程代码/
├─ CMakeLists.txt            构建脚本
├─ build.ps1                 一键构建脚本（自动探测/安装 Qt 工具链）
├─ deploy.ps1                部署 Qt 运行时（windeployqt）
├─ README.md                 说明文档
├─ data/dict/                内置词表（四级/六级 JSONL，随工程分发）
├─ third_party/sqlite/       SQLite amalgamation（sqlite3.c / sqlite3.h）
├─ docs/                     设计文档
├─ scripts/
│  └─ toolchain.ps1          Qt / MinGW / Ninja 探测与自动安装
├─ dist/                     输出目录（闪词.exe、ShanCi-source.zip）
├─ tools/
│  ├─ ImportTool.cpp         命令行词表导入工具
│  ├─ package.py             单文件打包脚本
│  ├─ make_source_zip.ps1    打包全部源码为 zip（用于迁移）
│  └─ bootstrapper/          单文件启动器（自解压，纯 Win32/C）
├─ tests/                    单元 / 集成测试
└─ src/
   ├─ core/        基础设施：AppContext、配置、日志、路径、工具、音频
   ├─ domain/      领域层：实体、枚举、核心规则
   ├─ data/        数据访问层：SqliteDb、Database、Repository/DAO
   ├─ service/     应用服务层：认证/单词/单词本/错题/题目/每日/字典/设置/同步
   ├─ generator/   题目生成：题型策略、干扰项生成
   ├─ importer/    JSONL 词表导入
   └─ ui/          表示层：登录、主界面、各功能页面、对话框、主题
```

### 工程文件清单与迁移

**构建所需的全部文件都在本工程目录内**（共 107 个），不依赖任何工程外的源码：

- 源码 / 头文件：`src/`、`tests/`、`tools/`
- 第三方库源码：`third_party/sqlite/`（SQLite amalgamation，直接参与编译）
- 词表数据：`data/dict/CET4.jsonl`、`CET6.jsonl`
- 构建脚本：`CMakeLists.txt`、`build.ps1`、`deploy.ps1`、`scripts/toolchain.ps1`
- 打包脚本：`tools/package.py`、`tools/make_source_zip.ps1`、`tools/bootstrapper/`

`build.ps1` 启动时会**校验这些文件是否齐全**；若工程被不完整地复制，会直接列出缺失文件并以提示退出，而不是给出莫名其妙的编译错误。

**整包迁移到另一台电脑**（推荐）：

```powershell
# 生成一个包含全部源码/词表/脚本的压缩包
powershell -ExecutionPolicy Bypass -File .\tools\make_source_zip.ps1
# -> dist\ShanCi-source.zip（约 11 MB）
```

把 `dist\ShanCi-source.zip` 拷到新机器解压，运行 `build.ps1` 即可。

> **唯一在工程外的**是编译器工具链（Qt / MinGW / Ninja），由 `build.ps1` 自动探测或自动下载安装——它不是本工程的源码。
>
> 若工程路径含中文（如 `…\软件工程\工程代码`），Qt 的 `moc.exe` 无法处理，
> 构建脚本会创建一个 ASCII **目录联接**（默认 `C:\shanci`，无权限时回退到 `%LOCALAPPDATA%\ShanCi-build`）。
> 该联接**只是路径别名**，源文件与构建产物仍然全部保存在本工程目录的 `build/` 下；
> 若想完全避免，把工程放到纯英文路径（如 `C:\Dev\ShanCi`）即可。

---

## 四、分层设计（对应需求七层）

| 层 | 需求 | 代码位置 |
| --- | --- | --- |
| 第一层 表示层与用户交互 | 登录/主界面/单词本/错题本/每日词表/做题/结果/字典/设置/同步 | `src/ui/**` |
| 第二层 应用服务层 | 认证、单词、单词本、错题、题目、每日词表、字典、设置、同步 | `src/service/Services.h` 及各 `.cpp` |
| 第三层 领域层与核心规则 | 实体、答案校验、评分、复习安排、错题自动管理、离线账户规则 | `src/domain/**` |
| 第四层 题目生成与练习 | 题型策略、干扰项生成、答案校验、题表生成、评分统计 | `src/generator/**`、`src/service/QuestionService.cpp` |
| 第五层 同步功能 | 手动导出/导入、冲突处理、同步日志（错题本 + 个人设置） | `src/service/SyncService.cpp` |
| 第六层 数据访问与存储 | Repository/DAO、SQLite、本地文件目录、数据库管理 | `src/data/**` |
| 第七层 基础设施与辅助 | 配置、日志、异常处理（Status/Result）、音频、打包发布 | `src/core/**`、`deploy.ps1` |

详见 [`docs/architecture.md`](docs/architecture.md)、[`docs/usecase.md`](docs/usecase.md)、[`docs/sequence.md`](docs/sequence.md)、[`docs/database.md`](docs/database.md)。

---

## 五、构建与运行

### 前置条件

| 组件 | 说明 |
| --- | --- |
| Windows 10/11 x64 | 目标平台 |
| CMake | ≥ 3.21，需在 `PATH` 中（单文件打包还需要 Python 3） |
| Qt 6 / MinGW-w64 / Ninja | **无需手动安装**：构建脚本会自动探测；若找不到，会自动用 `aqtinstall` 下载安装（约 1 GB，需要联网） |

> **首次构建很省事**：只要机器上有 CMake + Python 3 + 网络，直接运行 `build.ps1` 即可，
> 脚本会自动装好 Qt 6.8.3 + MinGW + Ninja，无需改任何路径。
> 若想禁用自动安装，加 `-NoAutoInstall`。

**自动探测的位置**（按顺序）：

- 环境变量 `QT_DIR` / `MINGW_DIR` / `NINJA_DIR`
- `C:\Qt\<版本>\mingw_64`、`%USERPROFILE%\Qt\...`、`%LOCALAPPDATA%\Qt\...`（自动取最高版本）
- `PATH` 上的 `qmake6` / `qmake` / `g++` / `ninja`

**手动指定**（已自行安装 Qt 时）：

```powershell
# 方式一：命令行参数
.\build.ps1 -QtDir "D:\Qt\6.8.3\mingw_64" -MingwDir "D:\Qt\Tools\mingw1310_64" -NinjaDir "D:\Qt\Tools\Ninja"

# 方式二：环境变量
$env:QT_DIR    = "D:\Qt\6.8.3\mingw_64"
$env:MINGW_DIR = "D:\Qt\Tools\mingw1310_64"
$env:NINJA_DIR = "D:\Qt\Tools\Ninja"
.\build.ps1
```

### 一键构建

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1            # 编译
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Clean     # 清理后编译
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Tests     # 编译并运行测试
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Run       # 编译并启动
```

产物：`build\bin\ShanCi.exe`

> **注意（中文路径）**：Qt 的 `moc.exe` 无法处理含非 ASCII（中文）字符的路径。
> 当工程位于中文目录时，`build.ps1` 会自动创建一个 ASCII 目录联接
> （默认 `C:\shanci` → 工程目录），并通过该联接进行构建；构建产物仍位于工程的 `build` 目录。

### 常见构建问题

| 现象 | 处理 |
| --- | --- |
| `Qt not found: C:\Qt\...` | 旧版本脚本写死了路径。当前脚本已改为自动探测/安装；若仍报错，运行 `.\build.ps1 -NoAutoInstall` 查看已探测到的路径，或设置 `QT_DIR` |
| 自动安装很慢 / 失败 | 检查网络；也可手动安装 Qt 6.8.3（MinGW 64-bit），再设置 `QT_DIR` / `MINGW_DIR` |
| `CMake was not found on PATH` | 安装 CMake 并勾选 “Add to PATH”，或重启终端 |
| 提示需要 Ninja | 自动安装会带 Ninja；离线环境请自行安装 Ninja 并设置 `NINJA_DIR` |
| 中文路径下构建失败 | 脚本会自动创建 ASCII 联接（默认 `C:\shanci`；无 C 盘根目录写权限时自动回退到 `%LOCALAPPDATA%\ShanCi-build`），实际路径记录在 `.buildroot`。若仍失败，把工程移到纯英文路径（如 `C:\Dev\ShanCi`）即可 |

### 打包为可分发目录

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy.ps1
```

执行后 `build\bin` 即为自包含目录（含 Qt 运行时 DLL 与内置词表），可整体复制到任意机器运行。

### 打包为单个 .exe（双击即启动）

```powershell
python tools\package.py            # 构建 + 部署 + 打包（推荐）
python tools\package.py --no-build # 复用现有 build\bin，仅打包
```

产物：

```
dist\闪词.exe       约 34 MB，单文件
```

- 该文件为**自解压启动器**（`tools/bootstrapper/Bootstrapper.c`），不依赖 Qt、不依赖 C++ 运行库。
- 双击后自动把内置运行时解压到 `%LOCALAPPDATA%\ShanCi\app` 并启动主程序；
  再次启动时若版本未变则直接启动（秒开）。
- 主程序编译为 **GUI 子系统程序**，启动时**不会出现命令行黑框**。
- **首次运行**会自动导入内置的四级 + 六级词表（8535 词），登录后自动填充单词本，开箱即用。
- 用户数据（账户、单词本、错题本等）保存在 `%LOCALAPPDATA%\ShanCi\app\data`，
  升级重新打包时**不会被覆盖**。
- 可自由重命名该 exe 或复制到任意位置（如桌面）。

> 提示：部分杀毒软件可能对“自解压”类程序提示，属于常见误报，添加信任即可。

### 导入词表（词库）

四级/六级词表在**首次启动时已自动导入**，通常无需手动操作。若需补充其他词表：

- 图形界面：**查询字典 → 导入词表**（选择 `.jsonl` 文件）
- 命令行（批量/维护用）：

```powershell
build\bin\vocab-import.exe --builtin                    # 导入内置四级 + 六级词表
build\bin\vocab-import.exe data\dict\CET4.jsonl         # 导入指定文件
build\bin\vocab-import.exe --fill-wordbook alice        # 把词库全部单词批量加入某用户默认单词本
build\bin\vocab-import.exe --help
```

> 导入的是**词库**（全局单词），不会自动塞进你的单词本。
> 单词本是用户自建的收藏夹：在「查询字典」中查到生词后点「加入单词本」，
> 或使用「单词本 → 批量添加词书…」按词书批量收藏。

### 每日词表能覆盖全部单词吗？

**能，但取决于每天的进度。** 说明如下：

- 每天生成词表时，系统按词书顺序取"**尚未排入计划**"的单词作为新词，所以只要持续使用，
  未背过的词会一天天被排入，最终覆盖整个词库（不会漏词、也不会重复作为新词出现）。
- 当天已排入但**未作答**的词，会在之后的词表里**继续以复习形式出现**，直到答对（或掌握）为止。
- 「每日词表」页会显示提示：按当前"每天背新词数"**约需多少天覆盖全部词库**；
  若"背诵天数"小于该值会给出 ⚠ 提醒（此时仍会继续排词，只是超出原定天数）。
- 建议把"背诵天数 × 每天背新词数"设置为 ≥ 词库总量，例如 8535 词、每天 20 词 → 约 428 天。

### 错题从哪里来？

**只有做题（答错）才会产生错题。** 生成题目/题表本身不会产生任何错题——
`generatePaper` 只写 `questions`/`papers`/`paper_entries`，只有当你在「练习」中**答错**并提交后，
该题对应的单词才会被记入错题本（可在「个人设置 → 答错自动加入错题本」关闭）。
另有两条非答题来源：结果页的"错题加入错题本"（针对本次已答错的题）与"手动同步"导入历史错题。
（这两项已由测试 `questionGenerationCreatesNoWrongEntries` 验证。）

---

## 六、数据存放位置

程序采用**便携式**数据目录：与可执行文件同级的 `data` 目录；若该位置不可写，则自动回退到系统数据目录。

**方式一：直接运行 `build\bin\ShanCi.exe`（开发/便携模式）**

```
build\bin\data\
├─ vocab.db           SQLite 数据库
├─ vocab.log          运行日志
├─ config.json        应用级配置（主题等）
├─ audio\             本地音频缓存
├─ sync\              同步文件默认目录
└─ dict\              内置词表（随程序分发）
```

**方式二：运行单文件 `闪词.exe`（发布模式）**

程序被解压到 `%LOCALAPPDATA%\ShanCi\app`，数据位于其下的 `data` 目录：

```
%LOCALAPPDATA%\ShanCi\app\
├─ ShanCi.exe     主程序（及 Qt 运行时 DLL、插件）
├─ .payload-tag       解压版本标记
└─ data\
   ├─ vocab.db        SQLite 数据库（用户数据，升级不覆盖）
   ├─ vocab.log       运行日志
   └─ dict\           内置词表
```

首次启动时若词库为空，程序会自动导入 `data\dict` 下的四级/六级词表。

同步文件为 JSON，仅包含**错题本**与**个人设置**两项内容，不含任何云端数据。

---

## 七、测试

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Tests
```

| 测试 | 覆盖内容 |
| --- | --- |
| `test_rules` | 答案校验、拼写容错、评分、复习间隔、熟悉度、错题优先级/状态、账户校验、口令哈希、编辑距离 |
| `test_generator` | 四种题型策略、干扰项去重、策略回退 |
| `test_repositories` | SQLite 往返（含 JSON 字段）、upsert、单词本/错题本条目、事务回滚 |
| `test_services` | 账户创建/登录 → 词表导入 → 单词本 → 出题 → 作答 → 交卷 → 错题本 → 同步 → 每日词表（端到端） |

---

## 八、快捷操作说明

- **双击**表格行可查看详情（单词本）或编辑（错题本）。
- 表格支持**右键菜单**：查看详情 / 编辑 / 加入错题本 / 移除等。
- 做题页支持选择题单选与拼写题回车提交。
- 交卷后进入结果页，可一键将全部错题加入错题本，或按原单词重做。
- 在“个人设置”中可切换深色主题、调整每日词数与题型偏好。
