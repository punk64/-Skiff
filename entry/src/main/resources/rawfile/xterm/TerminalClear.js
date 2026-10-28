 class TerminalClear {
  constructor(term, prompt) {
    this.term = term;
    this.inputStartMarker = null;
    this.prompt = prompt; // 自定义提示符
  }

  // 记录输入起始位置
  markInputStart() {
    this.inputStartMarker = this.term.registerMarker(0);
    this.term.onData(() => {
      if (!this.inputStartMarker) {
        this.markInputStart();
      }
    });
  }

  // 清除多行输入的核心方法
  clearMultilineInput() {
    if (!this.inputStartMarker) return;

    // 1. 计算需要清除的行数
    const startRow = this.inputStartMarker.line;
    const endRow = this.term.buffer.active.cursorY;
    const lineCount = endRow - startRow + 1;

    // 2. 定位到输入起始位置
    this.term.write(`\x1B[${startRow + 1};1H`);

    // 3. 执行清除操作（三种模式）
    this._performClear(lineCount);

    // 4. 重置输入状态
    this._resetInputState();
  }

  // 私有方法：执行实际清除
  _performClear(lineCount) {
    // 方法1：逐行清除
    if (lineCount <= 5) {
      new Array(lineCount).fill().forEach(() => {
        this.term.write("\x1B[2K\r\n");
      });
      this.term.write(`\x1B[${lineCount}A`);
    }
    // 方法2：使用滚动清除
    else {
      this.term.write(`\x1B[${lineCount}T`);
    }

    // 方法3：使用删除行命令
    this.term.write(`\x1B[${lineCount}M`);
  }

  // 重置输入状态
  _resetInputState() {
    this.inputStartMarker.dispose();
    this.markInputStart();
    this.term.write(this.prompt);
  }

  // 自动换行计算（考虑宽字符）
  _calculateWrappedLines(input) {
    const cols = this.term.cols;
    let lineCount = 1;
    let currentWidth = 0;

    for (const char of input) {
      const w = this._charWidth(char);
      currentWidth += w;

      if (currentWidth > cols) {
        lineCount++;
        currentWidth = w % cols;
      }
    }

    return lineCount;
  }

  // 字符宽度计算
  _charWidth(char) {
    return char.charCodeAt(0) > 255 ? 2 : 1;
  }

  // 初始化
  init() {
    this.markInputStart();
    this.term.onKey((e) => {
      if (e.domEvent.ctrlKey && e.domEvent.key === "c") {
        this.clearMultilineInput();
      }
    });
  }
}
