
# 知识预处理模块设计文档

## 1. 模块概述

知识预处理模块（Preprocessor）是知识图谱系统中的一个重要组件，负责对输入文本进行一系列预处理操作，包括文本清洗、分块、元数据提取、分词和关键词提取等功能。该模块采用流水线（Pipeline）设计模式，支持灵活配置和扩展。

## 2. 核心组件

### 2.1 预处理流水线（PreprocessingPipeline）

预处理流水线是整个模块的核心，负责协调各个处理步骤的执行。主要特点：

- 支持动态添加和配置处理步骤
- 提供默认的预处理步骤序列
- 支持并行处理多个文档
- 提供详细的处理日志和错误处理

### 2.2 文本清洗器（TextCleaner）

负责对输入文本进行清洗和规范化处理。内置的清洗规则包括：

- HTML标签移除
- 空白字符规范化
- 文档头部格式信息清理
- 特殊控制字符移除
- HTML实体处理
- 多换行合并

支持两种清洗规则：
- 正则表达式规则（RegexReplacementRule）
- 函数规则（FunctionRule）

### 2.3 文本分块器（TextSplitter）

提供多种文本分块策略：

- 固定大小分块（FixedSizeSplitter）
- 按句子分块（SentenceSplitter）
- 按段落分块（ParagraphSplitter）

### 2.4 中文分词器（ChineseTokenizer）

基于jieba分词实现，提供多种分词模式：

- 默认模式（DEFAULT）：精确分词
- 搜索引擎模式（SEARCH）：细粒度分词
- HMM模式（HMM）：基于隐马尔可夫模型的分词
- 混合模式（MIX）：结合精确分词和HMM

主要功能：
- 支持多种分词模式
- 关键词提取
- 停用词过滤
- 用户自定义词典支持

### 2.5 元数据提取器（MetadataExtractor）

负责提取文档的元数据信息，包括：

- 基本文本统计信息：
  - 字符数
  - 行数
  - 词数
- 文件元数据（根据文件类型提取）

## 3. 处理流程

预处理流水线的标准处理流程：

1. 文本清洗（CleaningStep）
   - 移除HTML标签
   - 规范化空白字符
   - 清理特殊字符
   - 处理HTML实体

2. 文本分块（SplittingStep）
   - 根据配置的分块策略进行分块
   - 支持自定义分块参数

3. 元数据提取（MetadataExtractionStep）
   - 提取文本统计信息
   - 提取文件元数据

4. 分词处理（TokenizationStep）
   - 根据配置的分词模式进行分词
   - 支持停用词过滤

5. 关键词提取（KeywordExtractionStep）
   - 提取文档关键词
   - 记录关键词到元数据

## 4. 配置选项

预处理流水线支持以下配置选项：

```cpp
struct PreprocessingOptions {
    bool clean;                    // 是否执行文本清洗
    bool split;                    // 是否执行文本分块
    bool extractMetadata;          // 是否提取元数据
    bool tokenize;                 // 是否执行分词
    bool extractKeywords;          // 是否提取关键词
    SplitStrategy splitStrategy;   // 分块策略
    TokenizeMode tokenizeMode;     // 分词模式
    size_t keywordCount;           // 关键词数量
    std::map<std::string, std::string> splitterOptions;  // 分块器选项
};
```

## 5. 错误处理

模块采用统一的错误处理机制：

- 每个处理步骤都有独立的异常捕获
- 提供详细的错误日志
- 在发生错误时提供合理的默认行为
- 支持错误恢复和继续处理

## 6. 性能优化

- 使用智能指针管理资源
- 支持并行处理多个文档
- 正则表达式优化
- 分词器缓存机制

## 7. 使用示例

```cpp
// 创建预处理流水线
auto pipeline = std::make_shared<PreprocessingPipeline>();

// 配置处理选项
PreprocessingOptions options;
options.clean = true;
options.split = true;
options.extractMetadata = true;
options.tokenize = true;
options.extractKeywords = true;
options.splitStrategy = SplitStrategy::PARAGRAPH;
options.tokenizeMode = TokenizeMode::DEFAULT;
options.keywordCount = 10;

// 处理文档
PreprocessingResult result;
pipeline->process("input.txt", result, options);
```

## 8. 注意事项

1. 文件路径处理
   - 确保词典文件路径正确
   - 处理文件不存在的情况

2. 内存管理
   - 使用智能指针避免内存泄漏
   - 大文件处理时注意内存使用

3. 并发处理
   - 注意线程安全性
   - 合理控制并发数量

4. 错误处理
   - 提供合理的错误恢复机制
   - 记录详细的错误日志

5. 性能考虑
   - 合理设置分块大小
   - 根据实际需求选择分词模式
