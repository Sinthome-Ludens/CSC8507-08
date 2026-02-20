#!/bin/bash
# 日志规范自动修复脚本
# 用途：将 std::cout/std::cerr 替换为 LOG_INFO/LOG_ERROR/LOG_WARN

echo "========================================="
echo "  日志规范自动修复脚本"
echo "========================================="
echo ""

# 定义要修复的文件
FILES=(
    "CSC8503/Source/Game/Systems/Sys_Render.cpp"
    "CSC8503/Source/Core/Bridge/AssetManager.cpp"
    "CSC8503/Source/Core/Bridge/AssimpLoader.cpp"
)

# 备份目录
BACKUP_DIR="backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"

echo "1. 备份原文件到 $BACKUP_DIR/"
for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        cp "$file" "$BACKUP_DIR/"
        echo "   ✓ 备份: $file"
    else
        echo "   ✗ 文件不存在: $file"
    fi
done

echo ""
echo "2. 开始修复..."

# 修复函数
fix_file() {
    local file=$1
    echo "   处理: $file"

    # 检查文件是否已包含 Log.h
    if ! grep -q '#include "Game/Utils/Log.h"' "$file"; then
        # 在第一个 #include 后添加 Log.h
        sed -i '0,/#include/a #include "Game/Utils/Log.h"' "$file"
        echo "     ✓ 添加 #include \"Game/Utils/Log.h\""
    fi

    # 替换 std::cout（信息）
    sed -i 's/std::cout << "\([^"]*\)" << \(.*\) << "\\n";/LOG_INFO("\1" << \2);/g' "$file"
    sed -i 's/std::cout << "\([^"]*\)" << std::endl;/LOG_INFO("\1");/g' "$file"
    sed -i 's/std::cout << "\([^"]*\)\\n";/LOG_INFO("\1");/g' "$file"

    # 替换 std::cerr（错误）
    sed -i 's/std::cerr << "\([^"]*\)Error\([^"]*\)" << \(.*\) << std::endl;/LOG_ERROR("\1Error\2" << \3);/g' "$file"
    sed -i 's/std::cerr << "\([^"]*\)Failed\([^"]*\)" << \(.*\) << std::endl;/LOG_ERROR("\1Failed\2" << \3);/g' "$file"

    # 替换 std::cerr（警告）
    sed -i 's/std::cerr << "\([^"]*\)Warning\([^"]*\)" << \(.*\) << std::endl;/LOG_WARN("\1Warning\2" << \3);/g' "$file"

    echo "     ✓ 完成替换"
}

# 修复所有文件
for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        fix_file "$file"
    fi
done

echo ""
echo "3. 修复完成！"
echo ""
echo "========================================="
echo "  修复摘要"
echo "========================================="
echo "备份位置: $BACKUP_DIR/"
echo "修复文件数: ${#FILES[@]}"
echo ""
echo "请手动检查修复结果，如有问题可从备份恢复。"
echo ""
